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

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (0.000005) // sample test interval for the first timer
#define TIMER_INTERVAL1_SEC   (5.78)   // sample test interval for the second timer
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload


uint64_t Cnt = 0;
int Grep =1000;

static void inline print_timer_counter(uint64_t counter_value)
{
    printf("Counter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32),
                                    (uint32_t) (counter_value));
}

/*
 * Timer group0 ISR handler
 */
void IRAM_ATTR timer_group0_isr(void *para)
{
    Cnt++;
    TIMERG0.hw_timer[TIMER_0].update = 1;
    TIMERG0.int_clr_timers.t0 = 1;
    uint64_t timer_counter_value = 
        ((uint64_t) TIMERG0.hw_timer[TIMER_0].cnt_high) << 32
        | TIMERG0.hw_timer[TIMER_0].cnt_low;
    timer_counter_value += (uint64_t) (TIMER_INTERVAL0_SEC * TIMER_SCALE);
    TIMERG0.hw_timer[TIMER_0].alarm_high = (uint32_t) (timer_counter_value >> 32);
    TIMERG0.hw_timer[TIMER_0].alarm_low = (uint32_t) timer_counter_value;
    TIMERG0.hw_timer[TIMER_0].config.alarm_en = TIMER_ALARM_EN;
}

/*
 * Initialize selected timer of the timer group 0
 */
    static void example_tg0_timer_init(int Group_idx,int timer_idx, 
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
        timer_init(Group_idx, timer_idx, &config);
        timer_set_counter_value(Group_idx, timer_idx, 0x00000000ULL);
        timer_set_alarm_value(Group_idx, timer_idx, timer_interval_sec * TIMER_SCALE);
        timer_enable_intr(Group_idx, timer_idx);
        timer_isr_register(Group_idx, timer_idx, timer_group0_isr, 
            (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);
    
        timer_start(Group_idx, timer_idx);
    }
/*
 * The main task of this example program
 */
static void timer_example_evt_task(void *arg)
{
    while (1) {
    	vTaskDelay(pdMS_TO_TICKS(1000));
		print_timer_counter(Cnt);
		Cnt=0;
    }
}

/*
 * In this example, we will test hardware timer0 and timer1 of timer group0.
 */
 
void app_main()
{
    example_tg0_timer_init(TIMER_GROUP_0,TIMER_0, TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
    xTaskCreate(timer_example_evt_task, "timer_evt_task", 2048, NULL, 5, NULL);
}
