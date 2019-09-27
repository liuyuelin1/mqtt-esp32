#include"iostm8s103F3.h"
#include<string.h>
#include<stdio.h>

#define DEBUG
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


#define PD3_ON   PD_ODR_ODR3 = 1;\
    SelfInfo.NowStatus |= 0x2;
#define PD3_OFF  PD_ODR_ODR3 = 0;\
    SelfInfo.NowStatus &= ~0x2;
#define PD2_ON   PD_ODR_ODR2 = 1;\
    SelfInfo.NowStatus |= 0x1;
#define PD2_OFF  PD_ODR_ODR2 = 0;\
    SelfInfo.NowStatus &= ~0x1;


/*eeprom 中表示前后关系的*/
enum EP_FLAG{
    EP_1 = 1,
    EP_2 = 2,
    EP_3 = 3,
};

/*开关状态*/
enum STATUS_FLAG{
    OFF = 0,
    ON  = 1,
};

/*上电状态 开，关，回复最后一次状态，位图*/
enum POWER_ON_MODE{
    POPEN    = 1,
    POCLOSE = 2,
    PLAST    = 3,
    PMAX     = 3,
};

/*命令字列表*/
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
    SET_ACK_MODE        = 0x0a,
    GET_POWER_STATUS    = 0xe5,
    SET_POWER_STATUS    = 0xe6,
    GET_BUTTON_MODE     = 0xe7,
    SET_BUTTON_MODE     = 0xe8,
    GET_BAUD_RATE       = 0xe9,
    SET_BAUD_RATE       = 0xea,
    GET_ADDR            = 0xeb,
    SET_ADDR            = 0xec,
};

/*命令字与函数映射结构体*/
typedef struct
{
    unsigned char SubCmd;
    void(*FuncAddr)(unsigned char);
} FUNC;

/*错误类型，当前未使用*/
enum ERROR_TYPE{
    CMD_LEN_DOMNFLOW = 1,
    CMD_LEN_OVERFLOW,
    CMD_DATA_ERR,
};

/*串口获取数据结构体*/
typedef struct
{
    unsigned char GetData[MAX_DATA_CNT];
    unsigned char DataCnt;
    int DataErr;
} GET_DATA;

/*全局变量结构体，可掉电保存部分*/
typedef struct
{
    unsigned char EpNum;
    unsigned char MagicNum;
    unsigned char Addr;
    unsigned char LastStatus;
    unsigned char NowStatus;
    unsigned char AckMode;
    unsigned char Ack;
    unsigned char ButtonMode;
    unsigned char PowerOnMode;
    unsigned char InfoChange;
} EQUIPMENT_INFO;

/*初步过滤出的命令字结构体*/
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
                        .LastStatus = OFF,
                        .AckMode = ON,
                        .ButtonMode = OFF,
                        .PowerOnMode = OFF};
//eeprom 保存地址
unsigned char *DataInfo1 = (unsigned char  *)(EP_HEADER_ADDR+EP_OFFSET); //EP_HEADER_ADDR    0x4000
unsigned char *DataInfo2 = (unsigned char  *)(EP_HEADER_ADDR+EP_OFFSET*2); //EP_HEADER_ADDR    0x4000

//GPIO init
void GpioInit(void){
PD_DDR_DDR2 = 1;
PD_DDR_DDR3 = 1;
PD_CR1_C12  = 1;
PD_CR1_C13  = 1;
PD_CR2_C22  = 0;
PD_CR2_C23  = 0; 
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

/*获取需要读eeprom的地址*/
unsigned char * GetReadEpAddr(void){

    unsigned char *DataInfo = 0;
    if((MAGIC_NUM != *(DataInfo1+1))&&(MAGIC_NUM != *(DataInfo2+1))){
        return 0;
    }
    else if((EP_1+EP_3) == (*DataInfo1 + *DataInfo2)){//1和3则1为新的
    if(*DataInfo1 > *DataInfo2){
          DataInfo = DataInfo1;
      }
      else{
          DataInfo = DataInfo2;
      }
    }
    else if(*DataInfo1 > *DataInfo2){
        DataInfo = DataInfo1;
    }
    else{
        DataInfo = DataInfo2;
    }
    UART1SendChar((*DataInfo)+0x30);
    return DataInfo;
}

/*获取需要写eeprom的地址*/
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
    else if(*DataInfo1 < *DataInfo2){
        DataInfo = DataInfo1;
    }
    else{
        DataInfo = DataInfo2;
    }
    return DataInfo;
}

/*data写入eeprom*/
void WriteData(void)
{
    output("EP write data!\n");
    unsigned char *DataInfo = 0;
    DataInfo = GetWriteEpAddr();
    
    /*必须在写入前更新位置信息*/
    if (EP_3 < SelfInfo.EpNum){
        SelfInfo.EpNum = EP_1;
    }
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
}

/*从eeprom读出data*/
void ReadData(void){
    output("EP read data!\n");
    unsigned char *DataInfo = 0;
    DataInfo = GetReadEpAddr();
    if(DataInfo){
        
        memcpy((unsigned char *)(&SelfInfo),DataInfo,sizeof(EQUIPMENT_INFO));
    }
    else{
        WriteData();
    }
    UART1SendChar(SelfInfo.MagicNum);
    SelfInfo.NowStatus = SelfInfo.LastStatus;
}

/*打开XX开关*/
void Open(unsigned char Data){
    output("open cmd!\n");
    if(Data&0x01){
        PD2_ON;
    }
    if(Data&0x02){
        PD3_ON;
    }
    if(PLAST == (SelfInfo.PowerOnMode & PMAX)){
        if(SelfInfo.LastStatus != SelfInfo.NowStatus){
            SelfInfo.LastStatus = SelfInfo.NowStatus;
            SelfInfo.InfoChange = 1;
        }
    }
    SelfInfo.Ack = ON;
}

/*打开关闭开关*/
void Close(unsigned char Data){
    output("close cmd!\n");
    if(Data&0x01){
        PD2_OFF;
    }
    if(Data&0x02){
        PD3_OFF;
    }
    if(PLAST == (SelfInfo.PowerOnMode & PMAX)){
        if(SelfInfo.LastStatus != SelfInfo.NowStatus){
            SelfInfo.LastStatus = SelfInfo.NowStatus;
            SelfInfo.InfoChange = 1;
        }
    }
    SelfInfo.Ack = ON;
}

/*XX开关取反*/
void Flip(unsigned char Data){
    if(Data&0x01){
        if(PD_IDR_IDR2){
            PD2_OFF;
        }
        else{
            PD2_ON;
        }
    }
    if(Data&0x02){
        if(PD_IDR_IDR3){
            PD3_OFF;
        }
        else{
            PD3_ON;
        }
    }
    SelfInfo.Ack = ON;
}
void TmOpen100Ms(unsigned char Data){}
void TmOpenS(unsigned char Data){}
void TmCloseS(unsigned char Data){}
void TmSysS(unsigned char Data){

}
void TmSysMin(unsigned char Data){

}
void GetStatus(unsigned char Data){
    SelfInfo.Ack = SelfInfo.NowStatus;
}
void SetAckMode(unsigned char Data){
    SelfInfo.AckMode = Data;
    SelfInfo.InfoChange = 1;
}
void GetPowerStatus(unsigned char Data){
    SelfInfo.Ack = SelfInfo.PowerOnMode;
}
void SetPowerStatus(unsigned char Data){
    SelfInfo.PowerOnMode = Data;
    SelfInfo.InfoChange = 1;
}
void GetButtonMode(unsigned char Data){
    SelfInfo.Ack =  SelfInfo.ButtonMode;
}
void SetButtonMode(unsigned char Data){
    SelfInfo.ButtonMode = Data;
    SelfInfo.InfoChange = 1;

}

/*波特率这个不用了，挂着*/
void Get_BaudRate(unsigned char Data){}
void Set_BaudRate(unsigned char Data){}

/*获取地址*/
void GetAddr(unsigned char Data){
    SelfInfo.Ack = SelfInfo.Addr;
}

/*设置地址*/
void SetAddr(unsigned char Data){
    SelfInfo.Addr = Data;
    SelfInfo.InfoChange = 1;
}

FUNC FuncList[CMD_CMT]={{OPEN,          Open},
                    {CLOSE,             Close},
                    {FLIP,              Flip},
                    {TM_OPEN_100MS,     TmOpen100Ms},
                    {TM_OPEN_S,         TmOpenS},
                    {TM_CLOSE_S,        TmCloseS},
                    {TM_SYS_S,          TmSysS},
                    {TM_SYS_MIN,        TmSysMin},
                    {GET_STATUS,        GetStatus},
                    {SET_ACK_MODE,           SetAckMode},
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
            if(SelfInfo.AckMode){
                UART1SendChar(SelfInfo.Ack);
            }
            break;
        }
    }
}

void ExeCmd(){
    if(SelfInfo.InfoChange){
        SelfInfo.InfoChange = 0;
        WriteData();
    }
}
void StatusInit(void){
    if(PLAST == (SelfInfo.PowerOnMode & PMAX)){
        Open(SelfInfo.LastStatus);
    }
    else if(POPEN == (SelfInfo.PowerOnMode & PMAX)){
        Open(PMAX);
    }
    else{
    }
}
main()
{
    InitUART1();
    GpioInit();
    PD3_OFF;
    PD2_OFF;
    ReadData();
    StatusInit();
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

