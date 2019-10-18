#include "cmd.h"
#include <math.h>

#define GPIO_LF_DIR    13
#define GPIO_RF_DIR    27
#define GPIO_LR_DIR    5
#define GPIO_RR_DIR    21
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_LF_DIR) | (1ULL<<GPIO_RF_DIR) | (1ULL<<GPIO_LR_DIR) | (1ULL<<GPIO_RR_DIR))


#define GPIO_FR_COL     34
#define GPIO_LR_COL     35
#define GPIO_UD_COL     36
#define GPIO_RT_COL     39
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_FR_COL) | (1ULL<<GPIO_LR_COL) | (1ULL<<GPIO_UD_COL) | (1ULL<<GPIO_RT_COL))
#define ESP_INTR_FLAG_DEFAULT 0


CAR_INFO TheCar;
CONTROL_INFO TheControl;

//卡尔曼滤波器
uint32_t KalmanFiter(uint32_t Data,uint8_t index){
    double TestPool = 5; 
    double PredictPool =1;
    double NowPool = 0;
    double Credibility;
    double Optimal;
    static double LastOptimal[4] = {1,1,1,1};
    static double LastOptimalPool[4] = {1,1,1,1};
    NowPool = PredictPool + LastOptimalPool[index];
    Credibility = sqrt(NowPool/(NowPool + TestPool));
    Optimal = LastOptimal[index] + Credibility*(Data - LastOptimal[index]);
    OptimalPool = NowPool*(1-Credibility);
    LastOptimal[index] = Optimal;
    LastOptimalPool[index] = OptimalPool;
    return (uint32_t)Optimal;
}

//计算速度值及方向，并置位方向
void SpeedIntegration(){
    double A = 0.15;//左右轮间距/2
    double B = 0.20;//前后轮间距/2
    uint32_t StepPorM = 1000;//步数/S
    global PwmInfo;
    TheCar.LFront.ClkCnt = (TheControl.BAfter.Speed - TheControl.LRight.Speed + TheControl.Rotating.Speed*(A+B))*StepPorM;
    TheCar.RFront.ClkCnt = (TheControl.BAfter.Speed + TheControl.LRight.Speed - TheControl.Rotating.Speed*(A+B))*StepPorM;
    TheCar.LRear.ClkCnt  = (TheControl.BAfter.Speed - TheControl.LRight.Speed - TheControl.Rotating.Speed*(A+B))*StepPorM;
    TheCar.RRear.ClkCnt  = (TheControl.BAfter.Speed + TheControl.LRight.Speed + TheControl.Rotating.Speed*(A+B))*StepPorM;
    if(0 < TheCar.LFront.ClkCnt){
        TheCar.LFront.ClkCnt = -1*TheCar.LFront.ClkCnt;
        TheCar.LFront.Dir = 1;
    }
    else{
        TheCar.LFront.Dir = 0;
    }
    if(0 < TheCar.RFront.ClkCnt){
        TheCar.RFront.ClkCnt = -1*TheCar.RFront.ClkCnt;
        TheCar.RFront.Dir = 1;
    }
    else{
        TheCar.RFront.Dir = 0;
    }
    if(0 < TheCar.LRear.ClkCnt){
        TheCar.LRear.ClkCnt = -1*TheCar.LRear.ClkCnt;
        TheCar.LRear.Dir = 1;
    }
    else{
        TheCar.LRear.Dir = 0;
    }
    if(0 < TheCar.RRear.ClkCnt){
        TheCar.RRear.ClkCnt = -1*TheCar.RRear.ClkCnt;
        TheCar.RRear.Dir = 1;
    }
    else{
        TheCar.RRear.Dir = 0;
    }
    gpio_set_level(GPIO_LF_DIR, TheCar.LFront.Dir);
    gpio_set_level(GPIO_RF_DIR, TheCar.RFront.Dir);
    gpio_set_level(GPIO_LR_DIR, TheCar.LRront.Dir);
    gpio_set_level(GPIO_RR_DIR, TheCar.RRront.Dir);
}

void GPIO_init(void){
    
    
}



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

/**
 * Brief:
 * This test code shows how to configure gpio and how to use gpio interrupt.
 *
 * GPIO status:
 * GPIO18: output
 * GPIO19: output
 * GPIO4:  input, pulled up, interrupt from rising edge and falling edge
 * GPIO5:  input, pulled up, interrupt from rising edge.
 *
 * Test:
 * Connect GPIO18 with GPIO4
 * Connect GPIO19 with GPIO5
 * Generate pulses on GPIO18/19, that triggers interrupt on GPIO4/5
 *
 */

#define GPIO_OUTPUT_IO_0    18
#define GPIO_OUTPUT_IO_1    19
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))
#define GPIO_INPUT_IO_0     4
#define GPIO_INPUT_IO_1     5
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT 0

static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

void app_main(void)
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //change gpio intrrupt type for one pin
    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);

    //remove isr handler for gpio number.
    gpio_isr_handler_remove(GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin again
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);

    int cnt = 0;
    while(1) {
        printf("cnt: %d\n", cnt++);
        vTaskDelay(1000 / portTICK_RATE_MS);
        gpio_set_level(GPIO_OUTPUT_IO_0, cnt % 2);
        gpio_set_level(GPIO_OUTPUT_IO_1, cnt % 2);
    }
}


