#include "cmd.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

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
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_ANYEDGE;//边沿触发
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_FR_COL, FR_isr_handler, (void*) GPIO_FR_COL);
    gpio_isr_handler_add(GPIO_LR_COL, LR_isr_handler, (void*) GPIO_LR_COL);
    gpio_isr_handler_add(GPIO_UD_COL, UD_isr_handler, (void*) GPIO_UD_COL);
    gpio_isr_handler_add(GPIO_RT_COL, RT_isr_handler, (void*) GPIO_RT_COL);
}

static void IRAM_ATTR FR_isr_handler(void* arg)
{
    static uint64_t start=0
    if(rtc_gpio_get_level(GPIO_FR_COL)){
        //获取系统时间
    }
    else{
        TheControl.BAfter.CTime=//获取时间
    }
}
static void IRAM_ATTR LR_isr_handler(void* arg)
{
    static uint64_t start=0
    if(rtc_gpio_get_level(GPIO_FR_COL)){
        //获取系统时间
    }
    else{
        TheControl.LRight.CTime=//获取时间
    }
}
static void IRAM_ATTR UD_isr_handler(void* arg)
{
    static uint64_t start=0
    if(rtc_gpio_get_level(GPIO_FR_COL)){
        //获取系统时间
    }
    else{
        TheControl.UDown.CTime=//获取时间
    }
}
static void IRAM_ATTR RT_isr_handler(void* arg)
{
    static uint64_t start=0
    if(rtc_gpio_get_level(GPIO_FR_COL)){
        //获取系统时间
    }
    else{
        TheControl.Rotating.CTime=//获取时间
    }
}

//还缺一个遥控信号转速度的。
void GetDutyTm(void){//获取占空比
    uint32_t SignalMin = 1000;//#信号最小值
    uint32_t SignalMiddle = 1500;//#信号中值
    uint32_t SignalMax = 2000;//#信号最大值
    uint32_t Precision = 500;//#理解为范围（SignalMiddle-SignalMin）
    uint32_t SpeedMax = 2;//#最大速度m/s
    double AngleSpeedMax = 6.28;//#最大角速度1转 弧度值 2*Pi
    uint32_t AngleMax = 40;//#最大角度
    uint32_t Signal = 0;
    
#前后
    Signal=TheControl.BAfter.CTime;
    if(Signal >SignalMiddle){
        TheControl.BAfter.Speed = (Signal - SignalMiddle)/Precision * SpeedMax;
    }
    else{
        TheControl.BAfter.Speed = -((SignalMiddle - Signal)/Precision * SpeedMax);
    }
#左右
    Signal=TheControl.LRight.CTime;
    if(Signal >SignalMiddle){
        TheControl.LRight.Speed = (Signal - SignalMiddle)/Precision * SpeedMax;
    }
    else{
        TheControl.LRight.Speed = -((SignalMiddle - Signal)/Precision * SpeedMax);
    }
#上下
    Signal=TheControl.UDown.CTime;
    TheControl.UDown.Speed = (Signal - SignalMin)/(Precision*2) * AngleMax;

#自转
    Signal=TheControl.Rotating.CTime;
    if(Signal >SignalMiddle){
        TheControl.Rotating.Speed = (Signal - SignalMiddle)/Precision * AngleSpeedMax;
    }
    else{
        TheControl.Rotating.Speed = -((SignalMiddle - Signal)/Precision * AngleSpeedMax);
    }
}
