#include "init.h"

//eeprom 保存地址
unsigned char *DataInfo1 = (unsigned char  *)(EP_HEADER_ADDR+EP_OFFSET); //EP_HEADER_ADDR    0x4000
unsigned char *DataInfo2 = (unsigned char  *)(EP_HEADER_ADDR+EP_OFFSET*2); //EP_HEADER_ADDR    0x4000
GET_DATA GetCmdData={{0},0,0};

//GPIO init
void GpioInit(void){
PD_DDR_DDR2 = 1;
PD_CR1_C12  = 1;
PD_CR2_C22  = 0;
PD_ODR_ODR2 = 1;//默认输出高电平

PD_DDR_DDR3 = 1;
PD_CR1_C13  = 1;
PD_CR2_C23  = 0; 
PD_ODR_ODR3 = 1;//默认输出高电平

PC_DDR_DDR5 = 1;
PC_CR1_C15  = 1;
PC_CR2_C25  = 0;
PC_ODR_ODR5 = 1;//默认输出高电平

PC_DDR_DDR6 = 1;
PC_CR1_C16  = 1;
PC_CR2_C26  = 0; 
PC_ODR_ODR6 = 1;//默认输出高电平

PD_DDR_DDR4 = 1;
PD_CR1_C14  = 1;
PD_CR2_C24  = 0; 
PD_ODR_ODR4 = 0;//默认输出高电平
}

void Init_Timer4(void)
{
    TIM4_CNTR=0; //计数器值
    TIM4_ARR=0xFA; //自动重装寄存器 250，产生125次定时1S
    TIM4_PSCR=0x06; //预分频系数为128 
    TIM4_EGR=0x01; //手动产生一个更新事件，用于PSC生效 注意，是手动更新
    TIM4_IER=0x01; //更新事件中断使能
    TIM4_CR1=0x01; //使能计时器，TIM4_CR0停止计时器
}

void InitUART1(void)
{
      UART1_CR1 = 0x00;
      UART1_CR2 = 0x00;
      UART1_CR3 = 0x00;
      UART1_BRR2= 0x00;
      UART1_BRR1= 0x0d;
      UART1_CR2 = 0x2c;
}
void UART1SendChar(unsigned char c)
{
      while((UART1_SR & 0x80)==0x00);
      UART1_DR=c;
}

//debug输出日志
void output(char * c){
#ifdef DEBUG
    char *a;
    a=c;
    while(*a != '\0'){
        UART1SendChar(*a);
        a++;
    }
#endif
}

#pragma vector= UART1_R_OR_vector
__interrupt void UART1_R_OR_IRQHandler(void)
{
    unsigned char ch;
    ch=UART1_DR;
    if (GetCmdData.DataCnt < MAX_DATA_CNT-1){
        GetCmdData.GetData[GetCmdData.DataCnt] = ch;
        GetCmdData.DataCnt++;
    }
    return;
}

#pragma vector=TIM4_OVR_UIF_vector
__interrupt void TIM4_OVR_UIF_IRQHandler(void)
{
    static unsigned char i=0;
    TIM4_SR &=~(0x01);
    i++;

    if(125<=i){
        i=0;
        if(PD_IDR_IDR4){
            PD_ODR_ODR4=0;
        }
        else{
            PD_ODR_ODR4=1;
        }
    }
}

