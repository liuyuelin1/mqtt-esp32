#ifndef __BUSINESS_CMD_H_
#define __BUSINESS_CMD_H_

#define DEBUG
#define CMD_DATA_CNT     5
#define CMD_TIME_OUT     10
#define CMD_START        0xaf
#define CMD_END          0xdf
#define NEW_CMD          0x01
#define FREE_CMD         0x00
#define DEF_ADDR         0xff
#define CMD_CMT          18
#define MAGIC_NUM        0x5a


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

void ReadData(void);
void CheckUARTCmd(void);
void AnalysysCmd(void);
void ExeCmd(void);
void StatusInit(void);

#endif

