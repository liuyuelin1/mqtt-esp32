#ifndef __INIT_H_
#define __INIT_H_

#include"iostm8s103F3.h"
#define EP_HEADER_ADDR 0x4000
#define EP_OFFSET        100
#define MAX_DATA_CNT     10

#define PD3_ON   PD_ODR_ODR3 = 1;\
    SelfInfo.NowStatus |= 0x2;
#define PD3_OFF  PD_ODR_ODR3 = 0;\
    SelfInfo.NowStatus &= ~0x2;
#define PD2_ON   PD_ODR_ODR2 = 1;\
    SelfInfo.NowStatus |= 0x1;
#define PD2_OFF  PD_ODR_ODR2 = 0;\
    SelfInfo.NowStatus &= ~0x1;

#define PC5_OFF   PC_ODR_ODR5 = 1;\
        SelfInfo.NowStatus |= 0x2;
#define PC5_ON  PC_ODR_ODR5 = 0;\
        SelfInfo.NowStatus &= ~0x2;
#define PC6_OFF   PC_ODR_ODR6 = 1;\
        SelfInfo.NowStatus |= 0x1;
#define PC6_ON  PC_ODR_ODR6 = 0;\
        SelfInfo.NowStatus &= ~0x1;

/*串口获取数据结构体*/
typedef struct
{
    unsigned char GetData[MAX_DATA_CNT];
    unsigned char DataCnt;
    int DataErr;
} GET_DATA;

extern unsigned char *DataInfo1;
extern unsigned char *DataInfo2;
extern GET_DATA GetCmdData;


void GpioInit(void);
void Init_Timer4(void);
void InitUART1(void);
void UART1SendChar(unsigned char c);
void output(char * c);
#endif

