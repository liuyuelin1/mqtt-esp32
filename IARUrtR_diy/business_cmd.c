#include<string.h>
#include<stdio.h>
#include"init.h"
#include"business_cmd.h"

DATA_ANALYSIS CmdData={{0},FREE_CMD,0};
EQUIPMENT_INFO SelfInfo={.EpNum = EP_1,
                        .MagicNum = MAGIC_NUM,
                        .Addr = 0xff,
                        .LastStatus = OFF,
                        .AckMode = ON,
                        .ButtonMode = OFF,
                        .PowerOnMode = OFF};

/*获取需要读eeprom的地址*/
unsigned char * GetReadEpAddr(void){

    unsigned char *DataInfo = 0;
    if((MAGIC_NUM != *(DataInfo1+1))&&(MAGIC_NUM != *(DataInfo2+1))){
        return 0;
    }
    else if((EP_1+EP_3) == (*DataInfo1 + *DataInfo2)){//1和3则1为新的
        if(*DataInfo1 < *DataInfo2){//取小的，也就是1
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
    SelfInfo.EpNum++;
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
    SelfInfo.NowStatus = SelfInfo.LastStatus;
}

/*打开XX开关*/
void Open(unsigned char Data){
    output("open cmd!\n");
    if(Data&0x01){
        PC5_ON;
    }
    if(Data&0x02){
        PC6_ON;
    }
    SelfInfo.NowStatus |=  Data & PMAX;
    if(PLAST == (SelfInfo.PowerOnMode & PMAX)){
        if(SelfInfo.LastStatus != SelfInfo.NowStatus){
            SelfInfo.LastStatus = SelfInfo.NowStatus;
            SelfInfo.InfoChange = 1;
        }
    }
}

/*打开关闭开关*/
void Close(unsigned char Data){
    output("close cmd!\n");
    if(Data&0x01){
        PC5_OFF;
    }
    if(Data&0x02){
        PC6_OFF;
    }
    SelfInfo.NowStatus &= ~(Data & PMAX);
    if(PLAST == (SelfInfo.PowerOnMode & PMAX)){
        if(SelfInfo.LastStatus != SelfInfo.NowStatus){
            SelfInfo.LastStatus = SelfInfo.NowStatus;
            SelfInfo.InfoChange = 1;
        }
    }
}

/*XX开关取反*/
void Flip(unsigned char Data){
    if(Data&0x01){
        if(PC_IDR_IDR5){
            PC5_ON;
        }
        else{
            PC5_OFF;
        }
    }
    if(Data&0x02){
        if(PC_IDR_IDR6){
            PC6_ON;
        }
        else{
            PC6_OFF;
        }
    }
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
    if(0x1 == Data){
        SelfInfo.AckMode = ON;
    }
    else{
        SelfInfo.AckMode = OFF;
    }
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
                    {SET_ACK_MODE,      SetAckMode},
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
            SelfInfo.Ack = CmdData.Cmd[3];//默认回复设置值，需要改的，函数中改动
            FuncList[i].FuncAddr(CmdData.Cmd[3]);
            if(ON == SelfInfo.AckMode){
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
