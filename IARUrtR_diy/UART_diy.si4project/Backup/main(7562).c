#include"iostm8s103F3.h"
#include<string.h>

#define MAX_DATA_CNT 10
#define CMD_DATA_CNT 5
#define CMD_TIME_OUT 10
#define CMD_START     0xaf
#define CMD_END       0xdf
#define NEW_CMD       0x01
#define FREE_CMD      0x00
#define DEF_ADDR      0xff
#define CMD_CMT       18
#define EP_HEADER_ADDR 0x4000
#define EP_OFFSET     100
#define MAGIC_NUM    0x5a

#define PD3_ON   (PD_ODR |= 0x20)
#define PD3_OFF (PD_ODR &= ~(0x20))
#define PD2_ON   (PD_ODR |= 0x10)
#define PD2_OFF (PD_ODR |= ~(0x10))

enum EP_FLAG{
    EP_1 = 1,
    EP_2 = 2,
    EP_3 = 3,
};

enum CMD_TYPE{
    OPEN                = 0x01,
    CLOSE               = 0x02,
    FLIP                = 0x03,
    TM_OPEN_100MS       = 0x04,
    TM_OPEN_S           = 0x05,
    TM_CLOSE_S          = 0x06,
    TM_SYS_S            = 0x07,
    TM_SYS_MIN          = 0x08,
    GET_STATUS          = 0x09,
    SET_ACK             = 0x0a,
    GET_POWER_STATUS    = 0xe5,
    SET_POWER_STATUS    = 0xe6,
    GET_BUTTON_MODE     = 0xe7,
    SET_BUTTON_MODE     = 0xe8,
    GET_BAUD_RATE       = 0xe9,
    SET_BAUD_RATE       = 0xea,
    GET_ADDR            = 0xeb,
    SET_ADDR            = 0xec,
};

typedef struct
{
    unsigned char SubCmd;
    void(*FuncAddr)(unsigned char);
} FUNC;

enum ERROR_TYPE{
    CMD_LEN_DOMNFLOW = 1,
    CMD_LEN_OVERFLOW,
    CMD_DATA_ERR,
};
    
typedef struct
{
    unsigned char GetData[MAX_DATA_CNT];
    unsigned char DataCnt;
    int DataErr;
} GET_DATA;


typedef struct
{
    unsigned char EpNum;
    unsigned char MagicNum;
    unsigned char Addr;
    unsigned char LastStatus;
    unsigned char AckMode;
    unsigned char ButtonMode;
    unsigned char PowerOnMode;
} EQUIPMENT_INFO;

typedef struct
{
    unsigned char Cmd[CMD_DATA_CNT];
    unsigned char CmdFlag;
    int CmdErr;
} DATA_ANALYSIS ;

GET_DATA GetCmdData={{0},0,0};
DATA_ANALYSIS CmdData={{0},FREE_CMD,0};
EQUIPMENT_INFO SelfInfo={.EpNum = EP_1,
                        .MagicNum = MAGIC_NUM,
                        .Addr = 0xff,
                        .LastStatus = 0,
                        .AckMode = 0,
                        .ButtonMode = 0,
                        .PowerOnMode = 0};

unsigned char *DataInfo1 = (unsigned char  *)(EP_HEADER_ADDR+EP_OFFSET); //EP_HEADER_ADDR    0x4000
unsigned char *DataInfo2 = (unsigned char  *)(EP_HEADER_ADDR+EP_OFFSET*2); //EP_HEADER_ADDR    0x4000

void GpioInit(void){
PD_DDR |= (0x10+0x20);
PD_CR1 |= (0x10+0x20);
PD_CR2 &= ~(0x10+0x20);
}

unsigned char * GetReadEpAddr(void){

    unsigned char *DataInfo = 0;
    if((MAGIC_NUM != *(DataInfo1+1))&&(MAGIC_NUM != *(DataInfo2+1))){
        return 0;
    }
    else if((EP_1+EP_3) == (*DataInfo1 + *DataInfo2)){//1和3则1为新的
        if(EP_3 == *DataInfo1){
            DataInfo = DataInfo2;
        }
        else{
            DataInfo = DataInfo1;
        }
    }
    else if(*DataInfo1 > *DataInfo2){
        DataInfo = DataInfo1;
    }
    else{
        DataInfo = DataInfo2;
    }
    return DataInfo;
}
unsigned char * GetWriteEpAddr(void){
    unsigned char *DataInfo = 0;
    if(MAGIC_NUM != *(DataInfo1+1)){
        DataInfo = DataInfo1;
    }
    else if (MAGIC_NUM != *(DataInfo2+1)){
        DataInfo = DataInfo2;
    }
    else if((EP_1+EP_3) == (*DataInfo1 + *DataInfo2)){//1和3则3为旧的
        if(EP_3 == *DataInfo1){
            DataInfo = DataInfo1;
        }
        else{
            DataInfo = DataInfo2;
        }
    }
    else if(*DataInfo1 > *DataInfo2){
        DataInfo = DataInfo2;
    }
    else{
        DataInfo = DataInfo1;
    }
    return DataInfo;
}

void WriteData(void)
{
    unsigned char *DataInfo = 0;
    DataInfo = GetWriteEpAddr();
    /*操作EEPROM，需要先进行解锁*/
    FLASH_DUKR=0xAE; //注意这里不能断点调试，否则会造成内部不同步，FLASH解锁失败
    FLASH_DUKR=0x56;
    FLASH_CR2=0x00;
    FLASH_NCR2=0xFF;
    if(!(FLASH_IAPSR & 0x08)) //检测对应的位是否解锁
        return;
    memcpy(DataInfo,(unsigned char  *)(&SelfInfo),sizeof(EQUIPMENT_INFO));
    FLASH_IAPSR=(unsigned char )(~0x08); //重新上锁 
    SelfInfo.EpNum++;
    if (EP_3 < SelfInfo.EpNum){
        SelfInfo.EpNum = EP_1;
    }
}
void ReadData(void){
    unsigned char *DataInfo = 0;
    DataInfo = GetReadEpAddr();
    if(DataInfo){
        
        memcpy((unsigned char *)(&SelfInfo),DataInfo,sizeof(EQUIPMENT_INFO));
    }
    else{
        WriteData();
    }
}

void Open(unsigned char Data){
    if(Data&0x01){

    }
    if(Data&0x02){

    }
}
void Close(unsigned char Data){
    if(Data&0x01){

    }
    if(Data&0x02){

    }
}
void Flip(unsigned char Data){}
void TmOpen100Ms(unsigned char Data){}
void TmOpenS(unsigned char Data){}
void TmCloseS(unsigned char Data){}
void TmSysS(unsigned char Data){}
void TmSysMin(unsigned char Data){}
void GetStatus(unsigned char Data){}
void SetAck(unsigned char Data){}
void GetPowerStatus(unsigned char Data){}
void SetPowerStatus(unsigned char Data){}
void GetButtonMode(unsigned char Data){}
void SetButtonMode(unsigned char Data){}
void Get_BaudRate(unsigned char Data){}
void Set_BaudRate(unsigned char Data){}
void GetAddr(unsigned char Data){}
void SetAddr(unsigned char Data){}


FUNC FuncList[CMD_CMT]={{OPEN,          Open},
                    {CLOSE,             Close},
                    {FLIP,              Flip},
                    {TM_OPEN_100MS,     TmOpen100Ms},
                    {TM_OPEN_S,         TmOpenS},
                    {TM_CLOSE_S,        TmCloseS},
                    {TM_SYS_S,          TmSysS},
                    {TM_SYS_MIN,        TmSysMin},
                    {GET_STATUS,        GetStatus},
                    {SET_ACK,           SetAck},
                    {GET_POWER_STATUS,  GetPowerStatus},
                    {SET_POWER_STATUS,  SetPowerStatus},
                    {GET_BUTTON_MODE,   GetButtonMode},
                    {SET_BUTTON_MODE,   SetButtonMode},
                    {GET_BAUD_RATE,     Get_BaudRate},
                    {SET_BAUD_RATE,     Set_BaudRate},
                    {GET_ADDR,          GetAddr},
                    {SET_ADDR,          SetAddr}};

void DelayMs(unsigned int ms)
{
       unsigned char i;
       while(ms!=0)
       {
              for(i=0;i<250;i++)
              {}
              for(i=0;i<75;i++)
              {}
              ms--;
       }
}

void InitUART1(void)
{
      UART1_CR1=0x00;
      UART1_CR2=0x00;
      UART1_CR3=0x00;
      UART1_BRR2=0x00;
      UART1_BRR1=0x0d;
      UART1_CR2=0x2c;
}
void UART1SendChar(unsigned char c)
{
      while((UART1_SR & 0x80)==0x00);
      UART1_DR=c;
}

void CheckUARTCmd(void){
    if(GetCmdData.DataCnt){
        DelayMs(CMD_TIME_OUT);
        if(CMD_DATA_CNT == GetCmdData.DataCnt){
            if((CMD_START == GetCmdData.GetData[0])&&(CMD_END == GetCmdData.GetData[CMD_DATA_CNT-1])){
                for(char i = 0;i < CMD_DATA_CNT; i++){
                    CmdData.Cmd[i] = GetCmdData.GetData[i];
                }
                CmdData.CmdFlag = NEW_CMD;
            }
            else{
                GetCmdData.DataErr = CMD_DATA_ERR;
            }
        }
        else{
            GetCmdData.DataErr = CMD_LEN_DOMNFLOW;
        }
        GetCmdData.DataCnt = 0;
    }
}
void AnalysysCmd(void){
    if(NEW_CMD != CmdData.CmdFlag){
        return;
    }
    CmdData.CmdFlag = FREE_CMD;
    if((DEF_ADDR != CmdData.Cmd[1])&&(SelfInfo.Addr != CmdData.Cmd[1])){
        return;
    }
    for(int i=0; i<CMD_CMT; i++){
        if(FuncList[i].SubCmd == CmdData.Cmd[2]){
            FuncList[i].FuncAddr(CmdData.Cmd[3]);
        }
    }
}

void ExeCmd(){}

main()
{
    InitUART1();
    GpioInit();
    PD3_ON;
    PD2_OFF;
	asm("rim");//打开全局中断
	while (1){
        CheckUARTCmd();
        AnalysysCmd();
        ExeCmd();
    }
}

#pragma vector= UART1_R_OR_vector//0x19
__interrupt void UART1_R_OR_IRQHandler(void)
{
    unsigned char ch;
    ch=UART1_DR;
    if (GetCmdData.DataCnt < MAX_DATA_CNT-1){
        GetCmdData.GetData[GetCmdData.DataCnt] = ch;
        GetCmdData.DataCnt++;
    }
    else{
        GetCmdData.DataErr = CMD_LEN_OVERFLOW;
    }
    return;
}

