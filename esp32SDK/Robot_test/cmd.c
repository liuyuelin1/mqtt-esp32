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



#define GPIO_LF_DIR    13
#define GPIO_RF_DIR    27
#define GPIO_LR_DIR    18//5
#define GPIO_RR_DIR    21
#define GPIO_DIR_SEL  ((1ULL<<GPIO_LF_DIR) | (1ULL<<GPIO_RF_DIR) | (1ULL<<GPIO_LR_DIR) | (1ULL<<GPIO_RR_DIR))

#define GPIO_LF_COL     2//12
#define GPIO_RF_COL     4//26
#define GPIO_LR_COL     5//18
#define GPIO_RR_COL     22
#define GPIO_PWM_SEL  ((1ULL<<GPIO_LF_COL) | (1ULL<<GPIO_RF_COL) | (1ULL<<GPIO_LR_COL) | (1ULL<<GPIO_RR_COL))
#define GPIO_SEL (GPIO_DIR_SEL | GPIO_PWM_SEL)

#define TIMER_DIVIDER         1  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (0.000005) // sample test interval for the first timer
#define TIMER_INTERVAL1_SEC   (5.78)   // sample test interval for the second timer
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload
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
        .ClkCnt=0x190, //速度
        .Group=0,
        .Timer=0,
    },
    .RFront = {
        .Value=0,//当前电平
        .Dir=0,//方向
        .ClkCnt=0x190, //速度
        .Group=0,
        .Timer=1,
    },
    .LRear = {
        .Value=0,//当前电平
        .Dir=0,//方向
        .ClkCnt=0x190, //速度
        .Group=1,
        .Timer=0,
    },
    .RRear = {
        .Value=0,//当前电平
        .Dir=0,//方向
        .ClkCnt=0x190, //速度
        .Group=1,
        .Timer=1,
    },
};
CONTROL_INFO TheControl ={
    .BAfter = 100,
    .LRight = 0,
    .UDown  = 0,
    .Rotating = 0,
    };

void SpeedIntegration(void){
    int32_t ClkCnt[4] = {0};
    ClkCnt[0]  = TheControl.BAfter - TheControl.LRight + TheControl.Rotating;
    ClkCnt[1]  = TheControl.BAfter + TheControl.LRight - TheControl.Rotating;
    ClkCnt[2]  = TheControl.BAfter - TheControl.LRight - TheControl.Rotating;
    ClkCnt[3]  = TheControl.BAfter + TheControl.LRight + TheControl.Rotating;
    if(0 == ClkCnt[0]){
        TheCar.LFront.ClkCnt = TIMER_SCALE;
     }
    else if(0 > ClkCnt[0]){
        TheCar.LFront.ClkCnt = TIMER_SCALE/(-1*ClkCnt[0]);
        TheCar.LFront.Dir = 1;
    }
    else{
        TheCar.LFront.ClkCnt = TIMER_SCALE/ClkCnt[0];
        TheCar.LFront.Dir = 0;
    }
    
    if(0 == ClkCnt[1]){
        TheCar.LFront.ClkCnt = TIMER_SCALE;
     }
    else if(0 > ClkCnt[1]){
        TheCar.RFront.ClkCnt = TIMER_SCALE/(-1*ClkCnt[1]);
        TheCar.RFront.Dir = 1;
    }
    else{
        TheCar.RFront.ClkCnt = TIMER_SCALE/ClkCnt[1];
        TheCar.RFront.Dir = 0;
    }
    
    if(0 == ClkCnt[2]){
        TheCar.LFront.ClkCnt = TIMER_SCALE;
     }
    else if(0 > ClkCnt[2]){
        TheCar.LRear.ClkCnt = TIMER_SCALE/(-1*ClkCnt[2]);
        TheCar.LRear.Dir = 1;
    }
    else{
        TheCar.LRear.ClkCnt = TIMER_SCALE/ClkCnt[2];
        TheCar.LRear.Dir = 0;
    }
    
    if(0 == ClkCnt[3]){
        TheCar.LFront.ClkCnt = TIMER_SCALE;
     }
    else if(0 > ClkCnt[3]){
        TheCar.RRear.ClkCnt = TIMER_SCALE/(-1*ClkCnt[3]);
        TheCar.RRear.Dir = 1;
    }
    else{
        TheCar.RRear.ClkCnt = TIMER_SCALE/ClkCnt[3];
        TheCar.RRear.Dir = 0;
    }
    gpio_set_level(GPIO_LF_DIR, TheCar.LFront.Dir);
    gpio_set_level(GPIO_RF_DIR, TheCar.RFront.Dir);
    gpio_set_level(GPIO_LR_DIR, TheCar.LRear.Dir);
    gpio_set_level(GPIO_RR_DIR, TheCar.RRear.Dir);
}

void GPIO_init(void){
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_SEL;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
}
/*
 * Timer group0 ISR handler
 */
void IRAM_ATTR timer_group0_isr(void *para)
{
    int timer_idx = (int) para;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    timer_intr_t timer_intr = timer_group_intr_get_in_isr(TIMER_GROUP_0);
    uint64_t timer_counter_value = timer_group_get_counter_value_in_isr(TIMER_GROUP_0, timer_idx);

    /* Prepare basic event data
       that will be then sent back to the main program task */
    timer_event_t evt;
    evt.timer_group = 0;
    evt.timer_idx = timer_idx;
    evt.timer_counter_value = timer_counter_value;

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if (timer_intr & TIMER_INTR_T0) {
        evt.type = TEST_WITHOUT_RELOAD;
        timer_group_intr_clr_in_isr(TIMER_GROUP_0, TIMER_0);
        timer_counter_value += (uint64_t) (TIMER_INTERVAL0_SEC * TIMER_SCALE);
        timer_group_set_alarm_value_in_isr(TIMER_GROUP_0, timer_idx, timer_counter_value);
    } else if (timer_intr & TIMER_INTR_T1) {
        evt.type = TEST_WITH_RELOAD;
        timer_group_intr_clr_in_isr(TIMER_GROUP_0, TIMER_1);
    } else {
        evt.type = -1; // not supported even type
    }
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, timer_idx);
}


/*
 * Timer group0 ISR handler
 */
void IRAM_ATTR timer_group1_isr(void *para)
{
    int timer_idx = (int) para;
    TIMERG1.hw_timer[timer_idx].update = 1;
    TIMERG1.int_clr_timers.t0 = 1;
    uint64_t timer_counter_value = 
        ((uint64_t) TIMERG1.hw_timer[timer_idx].cnt_high) << 32
        | TIMERG1.hw_timer[timer_idx].cnt_low;
    if(TheCar.RRear.Timer == timer_idx){
        timer_counter_value += (uint64_t)TheCar.RRear.ClkCnt;
        if(TheCar.RRear.Value){
            TheCar.RRear.Value = 0;
        }
        else{
            TheCar.RRear.Value = 1;
        }
        gpio_set_level(GPIO_RR_COL, TheCar.RRear.Value);

    }
    else{
        timer_counter_value += (uint64_t)TheCar.LRear.ClkCnt;
        if(TheCar.LRear.Value){
            TheCar.LRear.Value = 0;
        }
        else{
            TheCar.LRear.Value = 1;
        }
        gpio_set_level(GPIO_LR_COL, TheCar.LRear.Value);
    }
    TIMERG1.hw_timer[timer_idx].alarm_high = (uint32_t) (timer_counter_value >> 32);
    TIMERG1.hw_timer[timer_idx].alarm_low = (uint32_t) timer_counter_value;
    TIMERG1.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;
}

/*
 * Initialize selected timer of the timer group 0
 */
void example_tg0_timer_init(int timer_idx,
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
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr,
        (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}


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
        SpeedIntegration();
    	vTaskDelay(pdMS_TO_TICKS(1000));
        print_timer_counter(TheCar.LFront.ClkCnt);
        print_timer_counter(TheCar.RFront.ClkCnt);
        print_timer_counter(TheCar.LRear.ClkCnt);
        print_timer_counter(TheCar.RRear.ClkCnt);
    }
}

void SystemInit(void){
    print_timer_counter((uint64_t) (TIMER_INTERVAL0_SEC * TIMER_SCALE));
    print_timer_counter(TheCar.LFront.ClkCnt);
     printf("GPIO_init\n");
    GPIO_init();
    printf("wheel_timer_init0\n");
    example_tg0_timer_init(TIMER_0, TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
    //wheel_timer_init(TheCar.RFront.Group,TheCar.RFront.Timer, TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
    printf("wheel_timer_init1\n");
    //wheel_timer_init(TheCar.LFront.Group,TheCar.LFront.Timer, TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
    printf("wheel_timer_init2\n");
    //wheel_timer_init(TheCar.LRear.Group,TheCar.LRear.Timer, TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
    printf("wheel_timer_init3\n");
    //wheel_timer_init(TheCar.RRear.Group,TheCar.RRear.Timer, TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
}

