#ifndef __CMD_H__
#define __CMD_H__
#include "esp_types.h"

typedef struct {
    uint8_t Value;//当前电平
    uint8_t Dir;//方向
    uint8_t Die;//死区
    uint64_t ClkCnt; //速度
} WHEEL_INFO;

typedef struct {
    WHEEL_INFO LFront;//左前
    WHEEL_INFO RFront;//右前
    WHEEL_INFO LRear;//左后
    WHEEL_INFO RRear;//右后
} CAR_INFO;

typedef struct {
    uint64_t UpTm; 
    int32_t Speed;//速度
} CHANNEL_INFO;


typedef struct{
    CHANNEL_INFO XX;//前后油门
    CHANNEL_INFO Onoff; //左右油门
    CHANNEL_INFO BAfter;//前后油门
    CHANNEL_INFO LRight; //左右油门
    CHANNEL_INFO UDown;//云台上下
    CHANNEL_INFO Rotating; //自转
    uint8_t PowerDown;
} CONTROL_INFO;

extern CAR_INFO TheCar;
extern CONTROL_INFO TheControl;

extern void SpeedIntegration(void);
extern void GetDutyTm(void);
extern void timer_example_evt_task(void *arg);
extern void SystemInit(void);

#endif
