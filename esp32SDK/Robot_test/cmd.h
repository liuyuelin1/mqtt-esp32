#ifndef __CMD_H__
#define __CMD_H__
#include "esp_types.h"

typedef struct {
    uint8_t Value;//当前电平
    uint8_t Dir;//方向
    uint64_t ClkCnt; //速度
    int32_t Group;
    int32_t Timer;
} WHEEL_INFO;

typedef struct {
    WHEEL_INFO LFront;//左前
    WHEEL_INFO RFront;//右前
    WHEEL_INFO LRear;//左后
    WHEEL_INFO RRear;//右后
} CAR_INFO;

typedef struct{
    int32_t BAfter;//前后油门
    int32_t LRight; //左右油门
    int32_t UDown;//云台上下
    int32_t Rotating; //自转
} CONTROL_INFO;

extern CAR_INFO TheCar;
extern CONTROL_INFO TheControl;

extern void SpeedIntegration(void);
extern void GetDutyTm(void);
extern void timer_example_evt_task(void *arg);
extern void SystemInit(void);

#endif
