/* Timer group-hardware timer example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "cmd.h"

#define BA_COR         7
#define LR_COR         20
#define UD_COR         2
#define RO_COR         20

#define GPIO_EN    12
#define GPIO_X_DIR    27
#define GPIO_X_PWM    25

#define GPIO_Y_DIR    16
#define GPIO_Y_PWM    26



#define GPIO_DIR_SEL    ((1ULL<<GPIO_X_DIR) | (1ULL<<GPIO_Y_DIR))
#define GPIO_PWM_SEL  ((1ULL<<GPIO_X_PWM) | (1ULL<<GPIO_Y_PWM))
#define GPIO_EN_SEL     (1ULL<<GPIO_EN)

#define GPIO_SEL (GPIO_DIR_SEL | GPIO_PWM_SEL | GPIO_EN_SEL)

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (0.0001) // sample test interval for the first timer
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

MotorInfo g_X_motor,g_Y_motor;

/*属兴继承*/
void InheritInfo(MotorInfo *Motor,int index){
    if (index > 0){//其他阶段继承上一属兴
        Motor->StageInfo[index].dir = Motor->StageInfo[index - 1].lastDir;
        Motor->StageInfo[index].index = Motor->StageInfo[index - 1].aimIndex;
        Motor->StageInfo[index].lastDir = Motor->StageInfo[index].dir;//默认下一阶段方向同向
        Motor->StageInfo[index].aimIndex = Motor->StageInfo[index].index;
    }
    else{//第一阶段继承当前运动属兴
        Motor->StageInfo[index].dir = Motor->nowDir;
        Motor->StageInfo[index].index = Motor->cutRes;
        Motor->StageInfo[index].lastDir = Motor->lastDir;
        Motor->StageInfo[index].aimIndex = Motor->StageInfo[index].index;
    }
}

/*属兴继承*/
void SetFlag(MotorInfo *Motor,int index){
    Motor->StageInfo[index].flag = UN_FINISH;
}

void UpdateInfo(MotorInfo *Motor,int ArmPlace){
    int resize;
    Motor->lastDir = 1;
    for(int i=0; i<STAGT_CNT;i++){//初始化各阶段默认值
        Motor->StageInfo[i].res = 0;
        Motor->StageInfo[i].flag = FINISH;
        Motor->StageInfo[i].aimIndex = Motor->cutRes;//剩余减速步数正好对应加减曲线index
    }

    resize = ArmPlace - Motor->nowPlace;//计算调整值
    if (0 > resize){
        Motor->lastDir = 0;
        resize = resize * (-1);
    }
    printf("resize = %d,lastDir = %d\n",resize,Motor->lastDir);
    Motor->totalStep = resize + Motor->cutRes;//全程完成梯形曲线长度,有没有减速一样。

    //减1阶段
    InheritInfo(Motor,STATE_CUT1);
    if (((Motor->StageInfo[STATE_CUT1].lastDir == Motor->StageInfo[STATE_CUT1].dir) && (Motor->cutRes > resize)) || //同向且减速距离>差值（刹不住）
        ((Motor->StageInfo[STATE_CUT1].lastDir != Motor->StageInfo[STATE_CUT1].dir) && (0 != Motor->cutRes))){//异向且速度不是最小值
        Motor->StageInfo[STATE_CUT1].res = Motor->StageInfo[STATE_CUT1].index;
        Motor->StageInfo[STATE_CUT1].aimIndex = 0;//会减速到最低值
        Motor->StageInfo[STATE_CUT1].index = Motor->StageInfo[STATE_CUT1].index - 1;//修正index，比上一步减
        SetFlag(Motor,STATE_CUT1);//更新flag
    }
    printf("STATE_CUT1:flag:%d,index:%d,dir:%d,res:%d\n",Motor->StageInfo[STATE_CUT1].flag,Motor->StageInfo[STATE_CUT1].index,
    Motor->StageInfo[STATE_CUT1].dir,Motor->StageInfo[STATE_CUT1].res);
    //加速阶段
    InheritInfo(Motor,STATE_UP);
    if (Motor->totalStep/2 > Motor->StageInfo[STATE_CUT1].aimIndex){//有加速环节
        if (Motor->totalStep/2 >= INDEX_MAX){//可以到达最高速

            Motor->StageInfo[STATE_UP].res = INDEX_MAX - Motor->StageInfo[STATE_UP].index;
            Motor->StageInfo[STATE_UP].aimIndex = INDEX_MAX;//会加速到最高值
        }
        else{
            Motor->StageInfo[STATE_UP].res = Motor->totalStep/2 - Motor->StageInfo[STATE_UP].index;
            Motor->StageInfo[STATE_UP].aimIndex = Motor->totalStep/2;//会加速到
        }
        Motor->StageInfo[STATE_UP].index = Motor->StageInfo[STATE_UP].index + 1;//修正index，比上一步加
        SetFlag(Motor,STATE_UP);
    }
    printf("STATE_UP:flag:%d,index:%d,dir:%d,res:%d\n",Motor->StageInfo[STATE_UP].flag,Motor->StageInfo[STATE_UP].index,
    Motor->StageInfo[STATE_UP].dir,Motor->StageInfo[STATE_UP].res);
    //匀速阶段
    InheritInfo(Motor,STATE_CON);
    if (INDEX_MAX == Motor->StageInfo[STATE_CON].index){//最高速，说明有匀速阶段
        Motor->StageInfo[STATE_CON].res = Motor->totalStep - 2*INDEX_MAX;
        SetFlag(Motor,STATE_CON);
    }
    printf("STATE_CON:flag:%d,index:%d,dir:%d,res:%d\n",Motor->StageInfo[STATE_CON].flag,Motor->StageInfo[STATE_CON].index,
    Motor->StageInfo[STATE_CON].dir,Motor->StageInfo[STATE_CON].res);

    //减2阶段
    InheritInfo(Motor,STATE_CUT2);
    if (0 != Motor->totalStep){//只要不等于0就有减速阶段
        if (Motor->totalStep/2 >= INDEX_MAX){//可以到达最高速
            Motor->StageInfo[STATE_CUT2].res = INDEX_MAX;
        }
        else{
            Motor->StageInfo[STATE_CUT2].res = Motor->totalStep/2;
        }
        Motor->StageInfo[STATE_CUT2].aimIndex = 0;//会减速到最低值
        Motor->StageInfo[STATE_CUT2].index = Motor->StageInfo[STATE_CUT2].index - 1;//修正index，比上一步减
        SetFlag(Motor,STATE_CUT2);
    }
    printf("STATE_CUT2:flag:%d,index:%d,dir:%d,res:%d\n",Motor->StageInfo[STATE_CUT2].flag,Motor->StageInfo[STATE_CUT2].index,
    Motor->StageInfo[STATE_CUT2].dir,Motor->StageInfo[STATE_CUT2].res);

}

void TimerHandler(MotorInfo *Motor){
    int tempT = 0;
    char Dir = 1;
    int index = 0;
    if (UN_FINISH == Motor->StageInfo[STATE_CUT1].flag){//减速到最低
        Dir = Motor->StageInfo[STATE_CUT1].dir;
        index = Motor->StageInfo[STATE_CUT1].index;
        if (Motor->StageInfo[STATE_CUT2].aimIndex < Motor->StageInfo[STATE_CUT1].index){
            Motor->StageInfo[STATE_CUT1].index --;
        }
        else{
            Motor->StageInfo[STATE_CUT1].flag = FINISH;
        }
    }
    else if (UN_FINISH == Motor->StageInfo[STATE_UP].flag){//加速到最高
        Dir = Motor->StageInfo[STATE_UP].dir;
        index = Motor->StageInfo[STATE_UP].index;
        if (Motor->StageInfo[STATE_UP].aimIndex > Motor->StageInfo[STATE_UP].index){
            Motor->StageInfo[STATE_UP].index ++;
        }
        else{
            Motor->StageInfo[STATE_UP].flag = FINISH;
        }
    }
    else if (UN_FINISH == Motor->StageInfo[STATE_CON].flag){//匀速阶段看剩余步数
        Dir = Motor->StageInfo[STATE_CON].dir;
        index = Motor->StageInfo[STATE_CON].index;
        if (1 < Motor->StageInfo[STATE_CON].res){//有几步，进几次
            Motor->StageInfo[STATE_CON].res --;
        }
        else{
            Motor->StageInfo[STATE_CON].flag = FINISH;
        }
    }
    else if (UN_FINISH == Motor->StageInfo[STATE_CUT2].flag){//减速到最低
        Dir = Motor->StageInfo[STATE_CUT2].dir;
        index = Motor->StageInfo[STATE_CUT2].index;
        if (Motor->StageInfo[STATE_CUT2].aimIndex < Motor->StageInfo[STATE_CUT2].index){
            Motor->StageInfo[STATE_CUT2].index --;
        }
        else{
            Motor->StageInfo[STATE_CUT2].flag = FINISH;
        }
    }
    if(Dir){
        Motor->nowPlace++;
    }
    else{
        Motor->nowPlace--;
    }
    Motor->nowDir = Dir;
    Motor->cutRes = index;
//    printf("Dir = %d,Index = %d,NowPlace = %d\n",Motor->nowDir,Motor->cutRes,Motor->nowPlace);
}

void MotorInit(MotorInfo *Motor,char Dir, int Place, int Res){
    Motor->nowDir = Dir;
    Motor->nowPlace = Place;
    Motor->cutRes = Res;
}

void motor_test(void){
    int x=200;
    MotorInfo X_motor;
    int AimPlace = 2920;
    MotorInit(&X_motor,1,3000,10);
    UpdateInfo(&X_motor,AimPlace);

    while(x--){
        TimerHandler(&X_motor);
        if(X_motor.nowPlace == AimPlace){
            break;
        }
    }
}


void GPIO_init(void){
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_SEL;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_X_PWM, 0);
    gpio_set_level(GPIO_Y_PWM, 0);
    gpio_set_level(GPIO_X_DIR, 0);
    gpio_set_level(GPIO_Y_DIR, 0);
    gpio_set_level(GPIO_EN, 0);
}

/*
 * Timer group0 ISR handler
 */
int Cnt[4]={0};
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
        TimerHandler(&g_X_motor);
		TIMERG0.int_clr_timers.t0 = 1;
		timer_counter_value += AccStep[g_X_motor.cutRes];
        if(1 == Cnt[0]){
            gpio_set_level(GPIO_X_PWM, 0);
            Cnt[0]=0;
        }
        else{
            gpio_set_level(GPIO_X_PWM, 1);
            Cnt[0]=1;
        }


	} 
    else if (timer_idx == TIMER_1) {
        TimerHandler(&g_Y_motor);
		TIMERG0.int_clr_timers.t1 = 1;
		timer_counter_value += AccStep[g_Y_motor.cutRes];
    if(1 == Cnt[1]){
        gpio_set_level(GPIO_Y_PWM, 0);
        Cnt[1]=0;
    }
    else{
        gpio_set_level(GPIO_Y_PWM, 1);
        Cnt[1]=1;
    }


	} 
    else {
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
		timer_counter_value += TIMER_INTERVAL0_SEC*TIMER_SCALE;
		if(gpio_get_level(GPIO_X_PWM)){
			gpio_set_level(GPIO_X_PWM, 0);
		}
		else{
			gpio_set_level(GPIO_X_PWM, 1);
		}
	}
	else if (timer_idx == TIMER_1) {
		TIMERG1.int_clr_timers.t1 = 1;
		timer_counter_value += TIMER_INTERVAL0_SEC*TIMER_SCALE;
		if(gpio_get_level(GPIO_Y_PWM)){
			gpio_set_level(GPIO_Y_PWM, 0);
		}
		else{
			gpio_set_level(GPIO_Y_PWM, 1);
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
    	vTaskDelay(pdMS_TO_TICKS(1000));
        printf("running~%d,%d,%d,%d,\n",Cnt[0],Cnt[1],Cnt[2],Cnt[3]);
    }
}

void SystemInit(void){
    print_timer_counter((uint64_t) (TIMER_INTERVAL0_SEC * TIMER_SCALE));
     printf("GPIO_init\n");
    GPIO_init();
    printf("wheel_timer_init0\n");
    example_timer_init(TIMER_GROUP_0,TIMER_0, timer_group0_isr,TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
    example_timer_init(TIMER_GROUP_0,TIMER_1, timer_group0_isr,TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);

}


void app_main()
{
    motor_test();
    MotorInit(&g_X_motor,1,3000,10);
    MotorInit(&g_Y_motor,1,3000,10);
	SystemInit();
    xTaskCreate(timer_example_evt_task, "timer_evt_task", 2048, NULL, 5, NULL);
}

