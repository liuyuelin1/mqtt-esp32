import machine
from machine import Pin,Timer,PWM,UART
import utime

#配置版本号
VERSION = "V0.1.0"

FilterScale=10
FilterIndex=0
#输出信息结构体：引脚，当前值，目标值，目标方向，当前方向,目标方向
PwmInfo={
"LF":{"PinOutput":2 ,"NowSpeed":50,"AimSpeed":0,"NowDir":0,"AimDir":0},
"RF":{"PinOutput":14,"NowSpeed":50,"AimSpeed":0,"NowDir":0,"AimDir":0},
"LR":{"PinOutput":12,"NowSpeed":50,"AimSpeed":0,"NowDir":0,"AimDir":0},
"RR":{"PinOutput":13,"NowSpeed":50,"AimSpeed":0,"NowDir":0,"AimDir":0}
}

# 初始化一个UART对象
uart = UART(2, baudrate=115200, rx=5,tx=4,timeout=10)

ControlInfo={
"FR":{"DutyTm":0,"Refresh":0,"RefreshFlag":0,"Speed":0},
"LR":{"DutyTm":0,"Refresh":0,"RefreshFlag":0,"Speed":0},
"UD":{"DutyTm":0,"Refresh":0,"RefreshFlag":0,"Speed":0},
"RT":{"DutyTm":0,"Refresh":0,"RefreshFlag":0,"Speed":0}
}

def bytes_toint(firstbyte, secondbyte):
    if not firstbyte & 0x80:
        return firstbyte << 8 | secondbyte
    return - (((firstbyte ^ 255) << 8) | (secondbyte ^ 255) + 1)

def CmdParsing(Data):
    global ControlInfo
    if len(Data)<10:
        return
    if 0xaf == Data[0]:
        ControlInfo["FR"]["DutyTm"] = bytes_toint(Data[1],Data[2])
        ControlInfo["LR"]["DutyTm"] = bytes_toint(Data[3],Data[4])
        ControlInfo["UD"]["DutyTm"] = bytes_toint(Data[5],Data[6])
        ControlInfo["RT"]["DutyTm"] = bytes_toint(Data[7],Data[8])

def GetDutyTm():#获取占空比
    global ControlInfo
    SignalMin = 0#信号最小值
    SignalMiddle = 2048#信号中值
    SignalMax = 4096#信号最大值
    Precision = 2048#理解为范围（SignalMiddle-SignalMin）
    SpeedMax = 2#最大速度m/s
    AngleSpeedMax = 6.28#最大角速度1转 弧度值 2*Pi
    AngleMax = 40#最大角度
    
    #前后
    Signal=ControlInfo["FR"]["DutyTm"]
    if Signal >SignalMiddle:
        ControlInfo["FR"]["Speed"] = (Signal - SignalMiddle)/Precision * SpeedMax
    else:
        ControlInfo["FR"]["Speed"] = -((SignalMiddle - Signal)/Precision * SpeedMax)

    #左右
    Signal=ControlInfo["LR"]["DutyTm"]
    if Signal >SignalMiddle:
        ControlInfo["LR"]["Speed"] = (Signal - SignalMiddle)/Precision * SpeedMax
    else:
        ControlInfo["LR"]["Speed"] = -((SignalMiddle - Signal)/Precision * SpeedMax)

    #上下
    Signal=ControlInfo["UD"]["DutyTm"]
    ControlInfo["UD"]["Speed"] = (Signal - SignalMin)/(Precision*2) * AngleMax

    #自转
    Signal=ControlInfo["RT"]["DutyTm"]
    if Signal >SignalMiddle:
        ControlInfo["RT"]["Speed"] = (Signal - SignalMiddle)/Precision * AngleSpeedMax
    else:
        ControlInfo["RT"]["Speed"] = -((SignalMiddle - Signal)/Precision * AngleSpeedMax)


Cnt=1
InputFlag = 0;
def timing(tim):
    global Cnt
    global InputFlag
    global PwmInfo
    if uart.any():
        InputFlag = 1
        utime.sleep_ms(1)
        Data = uart.read()
        CmdParsing(Data)
    GetDutyTm()#更新遥控值 前后
    SpeedIntegration()#更新四轮速度
    Cnt = Cnt + 1
    if(Cnt > 10):#如果200ms内没更新，认为是失控，停止
        if not InputFlag:
            Cnt=0#急停
        Cnt=0
        InputFlag=0

def time_init(ms):
    tm=Timer(4)
    tm.init(period=ms, mode=Timer.PERIODIC, callback=timing)

def SpeedIntegration():
    A = 0.15#左右轮间距/2
    B = 0.20#前后轮间距/2
    StepPorM = 1000#步数/S
    global ControlInfo
    global PwmInfo
    PwmInfo["LF"]["AimSpeed"] = (ControlInfo["FR"]["Speed"] - ControlInfo["LR"]["Speed"] + ControlInfo["RT"]["Speed"]*(A+B))*StepPorM
    PwmInfo["RF"]["AimSpeed"] = (ControlInfo["FR"]["Speed"] + ControlInfo["LR"]["Speed"] - ControlInfo["RT"]["Speed"]*(A+B))*StepPorM
    PwmInfo["LR"]["AimSpeed"] = (ControlInfo["FR"]["Speed"] - ControlInfo["LR"]["Speed"] - ControlInfo["RT"]["Speed"]*(A+B))*StepPorM
    PwmInfo["RR"]["AimSpeed"] = (ControlInfo["FR"]["Speed"] + ControlInfo["LR"]["Speed"] + ControlInfo["RT"]["Speed"]*(A+B))*StepPorM
    for key in PwmInfo.keys():
        if 0 > PwmInfo[key]["AimSpeed"]:
            PwmInfo[key]["AimSpeed"] = -(PwmInfo[key]["AimSpeed"])
            PwmInfo[key]["AimDir"] = 0
        else:
            PwmInfo[key]["AimDir"] = 1

def main(): 
    global uart
    global ControlInfo
    global PwmInfo
    global FilterScale
    #初始化输出pwm
    LFront = PWM(Pin(PwmInfo["LF"]["PinOutput"]), freq=PwmInfo["LF"]["NowSpeed"], duty=5)#左前
    RFront = PWM(Pin(PwmInfo["RF"]["PinOutput"]), freq=PwmInfo["LF"]["NowSpeed"], duty=5)#右前
    LRear  = PWM(Pin(PwmInfo["LR"]["PinOutput"]), freq=PwmInfo["LF"]["NowSpeed"], duty=5)#左后
    RRear  = PWM(Pin(PwmInfo["RR"]["PinOutput"]), freq=PwmInfo["LF"]["NowSpeed"], duty=5)#右后
    time_init(20)
    while True:
        print(ControlInfo["FR"]["DutyTm"])
        utime.sleep_ms(1000)
        pass

if __name__ == "__main__":
    main()

