#include "cmd.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "driver/gpio.h"
#include "esp_types.h"
#include "esp_timer.h"
#include "driver/ledc.h"

#define BA_COR         7
#define LR_COR         20
#define UD_COR         2
#define RO_COR         20

#define PWM_CH_NUM     4
#define PWM_FREQ       1000

#define GPIO_LF_DIR    25
#define GPIO_LF_PWM    26
#define GPIO_LF_OF     27

#define GPIO_RF_DIR    23
#define GPIO_RF_PWM    22
#define GPIO_RF_OF     21

#define GPIO_LR_DIR    14
#define GPIO_LR_PWM    12
#define GPIO_LR_OF     13

#define GPIO_RR_DIR     4
#define GPIO_RR_PWM     2
#define GPIO_RR_OF     15


#define GPIO_DIR_SEL  ((1ULL<<GPIO_LF_DIR) | (1ULL<<GPIO_RF_DIR) | (1ULL<<GPIO_LR_DIR) | (1ULL<<GPIO_RR_DIR))
#define GPIO_PWM_SEL  ((1ULL<<GPIO_LF_PWM) | (1ULL<<GPIO_RF_PWM) | (1ULL<<GPIO_LR_PWM) | (1ULL<<GPIO_RR_PWM))
#define GPIO_OF_SEL   ((1ULL<<GPIO_LF_OF ) | (1ULL<<GPIO_RF_OF ) | (1ULL<<GPIO_LR_OF ) | (1ULL<<GPIO_RR_OF ))

#define GPIO_SEL (GPIO_DIR_SEL | GPIO_OF_SEL)

#define GPIO_XX_CON    36
#define GPIO_OF_CON    39
#define GPIO_LR_CON    34
#define GPIO_UD_CON    35
#define GPIO_BA_CON    32
#define GPIO_RO_CON    33  

#define GPIO_CON_SEL  ((1ULL<<GPIO_BA_CON) | (1ULL<<GPIO_LR_CON) | (1ULL<<GPIO_UD_CON) | (1ULL<<GPIO_RO_CON)\
	 | (1ULL<<GPIO_OF_CON) | (1ULL<<GPIO_XX_CON))


#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (3.4179) // sample test interval for the first timer
#define TIMER_INTERVAL1_SEC   (5.78)   // sample test interval for the second timer
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload
#define ESP_INTR_FLAG_DEFAULT 0

typedef struct {
    int type;  // the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;

CAR_INFO TheCar={
    .LFront = {
        .Value=0,//当前电平
        .Dir=0,//方向
        .ClkCnt=TIMER_SCALE, //速度
        .Die=1,
        .Ch=LEDC_CHANNEL_0,
    },
    .RFront = {
        .Value=0,//当前电平
        .Dir=0,//方向
        .ClkCnt=TIMER_SCALE, //速度
        .Die=1,
		.Ch=LEDC_CHANNEL_1,
    },
    .LRear = {
        .Value=0,//当前电平
        .Dir=0,//方向
        .ClkCnt=TIMER_SCALE, //速度
        .Die=1,
		.Ch=LEDC_CHANNEL_2,

    },
    .RRear = {
        .Value=0,//当前电平
        .Dir=0,//方向
        .ClkCnt=TIMER_SCALE, //速度
        .Die=1,
		.Ch=LEDC_CHANNEL_3,

    },
};
CONTROL_INFO TheControl ={
    .BAfter = {
		.Speed = 0,
		.UpTm = 0,
		},
    .LRight =  {
		.Speed = 0,
		.UpTm = 0,
		},
    .UDown  =  {
		.Speed = 0,
		.UpTm = 0,
		},
    .Rotating =  {
		.Speed = 0,
		.UpTm = 0,
		},
		.PowerDown = 1,
    };

void UpdatePower(void){
	uint8_t Value[4]={1,1,1,1};
	if(TheControl.PowerDown){
		Value[0]=0;
		Value[1]=0;
		Value[2]=0;
		Value[3]=0;
	}
	else{
		if(TheCar.LFront.Die){
			Value[0]=0;
		}
		if(TheCar.RFront.Die){
			Value[1]=0;
		}
		if(TheCar.LRear.Die){
			Value[2]=0;
		}
		if(TheCar.RRear.Die){
			Value[3]=0;
		}
	}
	gpio_set_level(GPIO_LF_OF, Value[0]);
	gpio_set_level(GPIO_RF_OF, Value[1]);
	gpio_set_level(GPIO_LR_OF, Value[2]);
	gpio_set_level(GPIO_RR_OF, Value[3]);
}

void CleanUpTm(void){
    TheControl.Onoff.UpTm = 0;
    TheControl.BAfter.UpTm = 1500;
    TheControl.LRight.UpTm = 1500;
    TheControl.UDown.UpTm = 1500;
    TheControl.Rotating.UpTm = 1500;
}

void GetDutyTm(void){//获取占空比
	int32_t SignalMin = 1000;//#信号最小值
	int32_t SignalMiddle = 1500;//#信号中值
	int32_t SignalMax = 2000;//#信号最大值
	double Precision = 500.0;//#理解为范围（SignalMiddle-SignalMin）
	int32_t SpeedMax = 4000;//#最大速度脉冲数
	int32_t AngleSpeedMax = 6.28*0.35*0.28*4000;//#最大角速度1转 弧度值 2*Pi
	int32_t AngleMax = 40;//#最大角度
	int32_t Signal = 0;
    int32_t SignalMiddleMin = 1490;//#信号中值最低，用来消除中值误差
    int32_t SignalMiddleMax = 1510;//#信号中值最高，用来消除中值误差
//#Power开关
	Signal=(int32_t)TheControl.Onoff.UpTm;
	if(Signal > SignalMiddle){
		TheControl.PowerDown = 0;
	}
	else{
		TheControl.PowerDown = 1;
	}

//#前后
	Signal=(int32_t)TheControl.BAfter.UpTm;
    if(SignalMiddleMin < Signal && Signal < SignalMiddleMax){
        TheControl.BAfter.Speed = 0;
    }
    else{
        TheControl.BAfter.Speed = (Signal - SignalMiddle)/Precision * SpeedMax;
    }

//#左右
	Signal=(int32_t)TheControl.LRight.UpTm;
    if(SignalMiddleMin < Signal && Signal < SignalMiddleMax){
        TheControl.LRight.Speed = 0;
    }
    else{
        TheControl.LRight.Speed = (Signal - SignalMiddle)/Precision * SpeedMax;
    }

//#上下
	Signal=(int32_t)TheControl.UDown.UpTm;
    if(SignalMiddleMin < Signal && Signal < SignalMiddleMax){
        TheControl.UDown.Speed = 0;
    }
    else{
        TheControl.UDown.Speed = (Signal - SignalMin)/(Precision*2) * AngleMax;
    }

//#自转
	Signal=(int32_t)TheControl.Rotating.UpTm;
    if(SignalMiddleMin < Signal && Signal < SignalMiddleMax){
        TheControl.Rotating.Speed = 0;
    }
    else{
        TheControl.Rotating.Speed = (Signal - SignalMiddle)/Precision * AngleSpeedMax;
    }
    CleanUpTm();//及时清除遥控数据，遥控异常断开时自动停止
}

void SpeedIntegration(void){
    int32_t ClkCnt[4] = {0};
	TheCar.LFront.Die = 0;
	TheCar.RFront.Die = 0;
	TheCar.LRear.Die = 0;
	TheCar.RRear.Die = 0;
    ClkCnt[0]  = TheControl.BAfter.Speed + TheControl.LRight.Speed + TheControl.Rotating.Speed;
    ClkCnt[1]  = TheControl.BAfter.Speed - TheControl.LRight.Speed - TheControl.Rotating.Speed;
    ClkCnt[2]  = TheControl.BAfter.Speed - TheControl.LRight.Speed + TheControl.Rotating.Speed;
    ClkCnt[3]  = TheControl.BAfter.Speed + TheControl.LRight.Speed - TheControl.Rotating.Speed;
    if(0 == ClkCnt[0]){
        TheCar.LFront.ClkCnt = 0;
		TheCar.LFront.Die = 1;
     }
    else if(0 > ClkCnt[0]){
        TheCar.LFront.ClkCnt = -1*ClkCnt[0];
        TheCar.LFront.Dir = 1;
    }
    else{
        TheCar.LFront.ClkCnt = ClkCnt[0];
        TheCar.LFront.Dir = 0;
    }
    
    if(0 == ClkCnt[1]){
        TheCar.RFront.ClkCnt = 0;
		TheCar.RFront.Die = 1;
     }
    else if(0 > ClkCnt[1]){
        TheCar.RFront.ClkCnt = -1*ClkCnt[1];
        TheCar.RFront.Dir = 0;
    }
    else{
        TheCar.RFront.ClkCnt = ClkCnt[1];
        TheCar.RFront.Dir = 1;
    }
    
    if(0 == ClkCnt[2]){
        TheCar.LRear.ClkCnt = 0;
		TheCar.LRear.Die = 1;
     }
    else if(0 > ClkCnt[2]){
        TheCar.LRear.ClkCnt = -1*ClkCnt[2];
        TheCar.LRear.Dir = 1;
    }
    else{
        TheCar.LRear.ClkCnt = ClkCnt[2];
        TheCar.LRear.Dir = 0;
    }
    
    if(0 == ClkCnt[3]){
        TheCar.RRear.ClkCnt = 0;
		TheCar.RRear.Die = 1;
     }
    else if(0 > ClkCnt[3]){
        TheCar.RRear.ClkCnt = -1*ClkCnt[3];
        TheCar.RRear.Dir = 0;
    }
    else{
        TheCar.RRear.ClkCnt = ClkCnt[3];
        TheCar.RRear.Dir = 1;
    }
    gpio_set_level(GPIO_LF_DIR, TheCar.LFront.Dir);
    gpio_set_level(GPIO_RF_DIR, TheCar.RFront.Dir);
    gpio_set_level(GPIO_LR_DIR, TheCar.LRear.Dir);
    gpio_set_level(GPIO_RR_DIR, TheCar.RRear.Dir);
	UpdatePower();
	printf("BA:0x%6x,LR:0x%6x,RO:0x%6x,UD:0x%6x\n",(uint32_t)TheControl.BAfter.UpTm,(uint32_t)TheControl.LRight.UpTm,(uint32_t)TheControl.Rotating.UpTm,(uint32_t)TheControl.UDown.UpTm);

	ledc_set_duty(LEDC_HIGH_SPEED_MODE, TheCar.LFront.Ch, TheCar.LFront.ClkCnt);
	ledc_update_duty(LEDC_HIGH_SPEED_MODE, TheCar.LFront.Ch);

	ledc_set_duty(LEDC_HIGH_SPEED_MODE, TheCar.RFront.Ch, TheCar.RFront.ClkCnt);
	ledc_update_duty(LEDC_HIGH_SPEED_MODE, TheCar.RFront.Ch);

	ledc_set_duty(LEDC_HIGH_SPEED_MODE, TheCar.LRear.Ch, TheCar.LRear.ClkCnt);
	ledc_update_duty(LEDC_HIGH_SPEED_MODE, TheCar.LRear.Ch);

	ledc_set_duty(LEDC_HIGH_SPEED_MODE, TheCar.RRear.Ch, TheCar.RRear.ClkCnt);
	ledc_update_duty(LEDC_HIGH_SPEED_MODE, TheCar.RRear.Ch);
}

static void IRAM_ATTR OF_isr_handler(void* arg)
{
	static uint64_t StartTm=0;
	if(gpio_get_level(GPIO_OF_CON)){
		StartTm = esp_timer_get_time();
	}
	else{
		TheControl.Onoff.UpTm = esp_timer_get_time() - StartTm;
	}
}
static void IRAM_ATTR XX_isr_handler(void* arg)
{
	static uint64_t StartTm=0;
	if(gpio_get_level(GPIO_XX_CON)){
		StartTm = esp_timer_get_time();
	}
	else{
		TheControl.XX.UpTm = esp_timer_get_time() - StartTm;
	}
}


static void IRAM_ATTR BA_isr_handler(void* arg)
{
	static uint64_t StartTm=0;
	if(gpio_get_level(GPIO_BA_CON)){
		StartTm = esp_timer_get_time();
	}
	else{
		TheControl.BAfter.UpTm = esp_timer_get_time() - StartTm + BA_COR;
	}
}
static void IRAM_ATTR LR_isr_handler(void* arg)
{
	static uint64_t StartTm=0;
	if(gpio_get_level(GPIO_LR_CON)){
		StartTm = esp_timer_get_time();
	}
	else{
		TheControl.LRight.UpTm = esp_timer_get_time() - StartTm + LR_COR;
	}

}
static void IRAM_ATTR UD_isr_handler(void* arg)
{
	static uint64_t StartTm=0;
	if(gpio_get_level(GPIO_UD_CON)){
		StartTm = esp_timer_get_time();
	}
	else{
		TheControl.UDown.UpTm = esp_timer_get_time() - StartTm + UD_COR;
	}

}
static void IRAM_ATTR RO_isr_handler(void* arg)
{
	static uint64_t StartTm=0;
	if(gpio_get_level(GPIO_RO_CON)){
		StartTm = esp_timer_get_time();
	}
	else{
		TheControl.Rotating.UpTm = esp_timer_get_time() - StartTm + RO_COR;
	}

}


void GPIO_init(void){
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_SEL;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
	UpdatePower();

    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = GPIO_CON_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_BA_CON, BA_isr_handler, (void*) GPIO_BA_CON);
    gpio_isr_handler_add(GPIO_LR_CON, LR_isr_handler, (void*) GPIO_LR_CON);
    gpio_isr_handler_add(GPIO_UD_CON, UD_isr_handler, (void*) GPIO_UD_CON);
    gpio_isr_handler_add(GPIO_RO_CON, RO_isr_handler, (void*) GPIO_RO_CON);
    gpio_isr_handler_add(GPIO_OF_CON, OF_isr_handler, (void*) GPIO_OF_CON);
    gpio_isr_handler_add(GPIO_XX_CON, XX_isr_handler, (void*) GPIO_XX_CON);
	
    int ch;

    /*
     * Prepare and set configuration of timers
     * that will be used by LED Controller
     */
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
        .freq_hz = PWM_FREQ,                      // frequency of PWM signal
        .speed_mode = LEDC_HIGH_SPEED_MODE,           // timer mode
        .timer_num = LEDC_TIMER_0            // timer index
    };
    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);

	ledc_channel_config_t ledc_channel[PWM_CH_NUM] = {
		{
			.channel	= LEDC_CHANNEL_0,
			.duty		= 0,
			.gpio_num	= GPIO_LF_PWM,
			.speed_mode = LEDC_HIGH_SPEED_MODE,
			.timer_sel	= LEDC_TIMER_0
		},
		{
			.channel	= LEDC_CHANNEL_1,
			.duty		= 0,
			.gpio_num	= GPIO_RF_PWM,
			.speed_mode = LEDC_HIGH_SPEED_MODE,
			.timer_sel	= LEDC_TIMER_0
		},
		{
			.channel	= LEDC_CHANNEL_2,
			.duty		= 0,
			.gpio_num	= GPIO_LR_PWM,
			.speed_mode = LEDC_HIGH_SPEED_MODE,
			.timer_sel	= LEDC_TIMER_0
		},
		{
			.channel	= LEDC_CHANNEL_3,
			.duty		= 0,
			.gpio_num	= GPIO_RR_PWM,
			.speed_mode = LEDC_HIGH_SPEED_MODE,
			.timer_sel	= LEDC_TIMER_0
		},
	};
	for (ch = 0; ch < PWM_CH_NUM; ch++) {
        ledc_channel_config(&ledc_channel[ch]);
    }
	ledc_fade_func_install(0);
}

/*
 * Timer group0 ISR handler
 */
void IRAM_ATTR timer_group0_isr(void *para)
{
	int timer_idx = (int) para;

	/* Retrieve the interrupt status and the counter value
	   from the timer that reported the interrupt */
	uint32_t intr_status = TIMERG0.int_st_timers.val;
	TIMERG0.hw_timer[timer_idx].update = 1;
	uint64_t timer_counter_value = 
		((uint64_t) TIMERG0.hw_timer[timer_idx].cnt_high) << 32
		| TIMERG0.hw_timer[timer_idx].cnt_low;

	/* Clear the interrupt
	   and update the alarm time for the timer with without reload */
	if (timer_idx == TIMER_0) {
		TIMERG0.int_clr_timers.t0 = 1;
		timer_counter_value += TheCar.LFront.ClkCnt;
		if(TheCar.LFront.Value){
			gpio_set_level(GPIO_LF_PWM, 1);
			TheCar.LFront.Value = 0;
		}
		else{
			gpio_set_level(GPIO_LF_PWM, 0);
			TheCar.LFront.Value = 1;
		}

	} else if (timer_idx == TIMER_1) {
		TIMERG0.int_clr_timers.t1 = 1;
		timer_counter_value += TheCar.RFront.ClkCnt;
		if(TheCar.RFront.Value){
			gpio_set_level(GPIO_RF_PWM, 1);
			TheCar.RFront.Value = 0;
		}
		else{
			gpio_set_level(GPIO_RF_PWM, 0);
			TheCar.RFront.Value = 1;
		}

	} else {
		timer_counter_value += (uint64_t) (1 * TIMER_SCALE);
	}
	TIMERG0.hw_timer[timer_idx].alarm_high = (uint32_t) (timer_counter_value >> 32);
	TIMERG0.hw_timer[timer_idx].alarm_low = (uint32_t) timer_counter_value;

	/* After the alarm has been triggered
	  we need enable it again, so it is triggered the next time */
	TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

	/* Now just send the event data back to the main program task */
}


/*
 * Timer group1 ISR handler
 */
void IRAM_ATTR timer_group1_isr(void *para)
{
	int timer_idx = (int) para;

	/* Retrieve the interrupt status and the counter value
	   from the timer that reported the interrupt */
	uint32_t intr_status = TIMERG1.int_st_timers.val;
	TIMERG1.hw_timer[timer_idx].update = 1;
	uint64_t timer_counter_value = 
		((uint64_t) TIMERG1.hw_timer[timer_idx].cnt_high) << 32
		| TIMERG1.hw_timer[timer_idx].cnt_low;
	if (timer_idx == TIMER_0) {
		TIMERG1.int_clr_timers.t0 = 1;
		timer_counter_value += TheCar.LRear.ClkCnt;
		if(TheCar.LRear.Value){
			gpio_set_level(GPIO_LR_PWM, 1);
			TheCar.LRear.Value = 0;
		}
		else{
			gpio_set_level(GPIO_LR_PWM, 0);
			TheCar.LRear.Value = 1;
		}

	}
	else if (timer_idx == TIMER_1) {
		TIMERG1.int_clr_timers.t1 = 1;
		timer_counter_value += TheCar.RRear.ClkCnt;
		if(TheCar.RRear.Value){
			gpio_set_level(GPIO_RR_PWM, 1);
			TheCar.RRear.Value = 0;
		}
		else{
			gpio_set_level(GPIO_RR_PWM, 0);
			TheCar.RRear.Value = 1;
		}
	} else {
		timer_counter_value += (uint64_t) (1 * TIMER_SCALE);
	}
	TIMERG1.hw_timer[timer_idx].alarm_high = (uint32_t) (timer_counter_value >> 32);
	TIMERG1.hw_timer[timer_idx].alarm_low = (uint32_t) timer_counter_value;
	/* After the alarm has been triggered
	  we need enable it again, so it is triggered the next time */
	TIMERG1.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

}

/*
 * Initialize selected timer of the timer group 0
 */
void example_timer_init(int group_idx,int timer_idx, void (*fn)(void*),
	bool auto_reload, double timer_interval_sec)
{
	/* Select and initialize basic parameters of the timer */
	timer_config_t config;
	config.divider = TIMER_DIVIDER;
	config.counter_dir = TIMER_COUNT_UP;
	config.counter_en = TIMER_PAUSE;
	config.alarm_en = TIMER_ALARM_EN;
	config.intr_type = TIMER_INTR_LEVEL;
	config.auto_reload = auto_reload;
	timer_init(group_idx, timer_idx, &config);

	/* Timer's counter will initially start from value below.
	   Also, if auto_reload is set, this value will be automatically reload on alarm */
	timer_set_counter_value(group_idx, timer_idx, 0x00000000ULL);

	/* Configure the alarm value and the interrupt on alarm. */
	timer_set_alarm_value(group_idx, timer_idx, timer_interval_sec * TIMER_SCALE);
	timer_enable_intr(group_idx, timer_idx);
	timer_isr_register(group_idx, timer_idx, fn, 
		(void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);
	timer_start(group_idx, timer_idx);
}

//TIMER_GROUP_0

void inline print_timer_counter(uint64_t counter_value)
{
    printf("Counter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32),
                                    (uint32_t) (counter_value));
}

/*
 * The main task of this example program
 */
void timer_example_evt_task(void *arg)
{
    while (1) {
		GetDutyTm();
        SpeedIntegration();
    	vTaskDelay(pdMS_TO_TICKS(50));
//        print_timer_counter(TheControl.Onoff.UpTm);
//        print_timer_counter(TheControl.XX.UpTm);
        printf("Powerdown = %d\n",TheControl.PowerDown);
		printf("Die = %d\n",TheCar.LFront.Die);
		print_timer_counter(TheCar.LFront.ClkCnt);

    }
}

void SystemInit(void){
    print_timer_counter((uint64_t) (TIMER_INTERVAL0_SEC * TIMER_SCALE));
    print_timer_counter(TheCar.LFront.ClkCnt);
     printf("GPIO_init\n");
    GPIO_init();
    printf("wheel_timer_init0\n");
//    example_timer_init(TIMER_GROUP_0,TIMER_0, timer_group0_isr,TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
//    example_timer_init(TIMER_GROUP_0,TIMER_1, timer_group0_isr,TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
//    example_timer_init(TIMER_GROUP_1,TIMER_0, timer_group1_isr,TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
//    example_timer_init(TIMER_GROUP_1,TIMER_1, timer_group1_isr,TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
}

