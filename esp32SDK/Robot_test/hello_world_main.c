#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "cmd.h"

uint64_t Cnt = 0;
int Grep =1000;

/*
 * In this example, we will test hardware timer0 and timer1 of timer group0.
 */
 
void app_main()
{
    SystemInit();
    xTaskCreate(timer_example_evt_task, "timer_evt_task", 2048, NULL, 5, NULL);
}
