import machine
from machine import Pin,Timer,PWM
import utime

#配置版本号
VERSION = "V0.1.0"

FilterScale=10
FilterIndex=0
DutyTmLF=[0]*FilterScale
DutyTmRF=[0]*FilterScale
DutyTmLR=[0]*FilterScale
DutyTmRR=[0]*FilterScale

#输出信息结构体：引脚，当前值，目标值，目标方向，当前方向,目标方向
PwmInfo={
"LF":{"PinOutput":12,"NowSpeed":50,"AimSpeed":0,"NowDir":0,"AimDir":0},
"RF":{"PinOutput":13,"NowSpeed":50,"AimSpeed":0,"NowDir":0,"AimDir":0},
"LR":{"PinOutput":14,"NowSpeed":50,"AimSpeed":0,"NowDir":0,"AimDir":0},
"RR":{"PinOutput":27,"NowSpeed":50,"AimSpeed":0,"NowDir":0,"AimDir":0}
}
#遥控信息 引脚，开始时间，滤波位置，滤波buffer,更新计数，更新标志，速度:m/s
ControlInfo={
"FR":{"PinControl":5 ,"TempTM":0,"FilterIndex":0,"DutyTm":DutyTmLF,"Refresh":0,"RefreshFlag":0,"Speed":1},
"LR":{"PinControl":18,"TempTM":0,"FilterIndex":0,"DutyTm":DutyTmRF,"Refresh":0,"RefreshFlag":0,"Speed":1},
"UD":{"PinControl":19,"TempTM":0,"FilterIndex":0,"DutyTm":DutyTmRF,"Refresh":0,"RefreshFlag":0,"Speed":0},
"RT":{"PinControl":21,"TempTM":0,"FilterIndex":0,"DutyTm":DutyTmLR,"Refresh":0,"RefreshFlag":0,"Speed":1}
}

def SetDutyTm(diff,Member):#记录一次占空比
    global ControlInfo
    ControlInfo[Member]["FilterIndex"]=ControlInfo[Member]["FilterIndex"]+1
    if ControlInfo[Member]["FilterIndex"]>=FilterScale:
        ControlInfo[Member]["FilterIndex"]=0
    ControlInfo[Member]["DutyTm"][ControlInfo[Member]["FilterIndex"]]=diff
    ControlInfo[Member]["Refresh"]=1

def GetDutyTm(Member):#获取一次滤波后的占空比
    global ControlInfo
    diff=0
    SignalMin = 1000#信号最小值
    SignalMiddle = 1500#信号中值
    SignalMax = 2000#信号最大值
    Precision = 500#理解为范围（1500-2000）
    SpeedMax = 2#m/s
    AngleSpeedMax = 6.28#弧度值 2*Pi
    AngleMax = 40
    for i in range(1,FilterScale):
        if diff<ControlInfo[Member]["DutyTm"][i-1]:
            diff=ControlInfo[Member]["DutyTm"][i-1]
    if ControlInfo[Member]["RefreshFlag"]:
        if Member == "RT":#自转
            if diff >SignalMiddle:
                ControlInfo[Member]["Speed"] = (diff - SignalMiddle)/Precision * AngleSpeedMax
            else:
                ControlInfo[Member]["Speed"] = -((SignalMiddle - diff)/Precision * AngleSpeedMax)
        elif Member == "UD":#上下表示抬头
                ControlInfo[Member]["Speed"] = (diff - SignalMin)/(Precision*2) * AngleMax
        else:#平面移动
            if diff >SignalMiddle:
                ControlInfo[Member]["Speed"] = (diff - SignalMiddle)/Precision * SpeedMax
            else:
                ControlInfo[Member]["Speed"] = -((SignalMiddle - diff)/Precision * SpeedMax)

def FRcallback(p):
    global ControlInfo
    if p.value():
        ControlInfo["FR"]["TempTM"]=utime.ticks_us()
    else:
        SetDutyTm(utime.ticks_diff(utime.ticks_us(),ControlInfo["FR"]["TempTM"]),"FR")

def LRcallback(p):
    global ControlInfo
    if p.value():
        ControlInfo["LR"]["TempTM"]=utime.ticks_us()
    else:
        SetDutyTm(utime.ticks_diff(utime.ticks_us(),ControlInfo["LR"]["TempTM"]),"LR")

def UDcallback(p):
    global ControlInfo
    if p.value():
        ControlInfo["UD"]["TempTM"]=utime.ticks_us()
    else:
        SetDutyTm(utime.ticks_diff(utime.ticks_us(),ControlInfo["UD"]["TempTM"]),"UD")

def RTcallback(p):
    global ControlInfo
    if p.value():
        ControlInfo["RT"]["TempTM"]=utime.ticks_us()
    else:
        SetDutyTm(utime.ticks_diff(utime.ticks_us(),ControlInfo["RT"]["TempTM"]),"RT")

Cnt=1
def timing(tim):
    global Cnt
    global ControlInfo
    if Cnt:
        Cnt = 0
        for key in ControlInfo.keys():
            if ControlInfo[key]["Refresh"]:
                ControlInfo[key]["RefreshFlag"]=1
                ControlInfo[key]["Refresh"] = 0
            else:
                ControlInfo[key]["RefreshFlag"]=0
            GetDutyTm(key)#更新遥控值 前后
    else:
        Cnt = 1
        SpeedIntegration()#更新四轮速度
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
    global ControlInfo
    global PwmInfo
    #遥控输入脚，中断方式记录数据
    InPutFR=Pin(ControlInfo["FR"]["PinControl"],Pin.IN,Pin.PULL_UP)
    InPutLR=Pin(ControlInfo["LR"]["PinControl"],Pin.IN,Pin.PULL_UP)
    InPutUD=Pin(ControlInfo["UD"]["PinControl"],Pin.IN,Pin.PULL_UP)
    InPutRT=Pin(ControlInfo["RT"]["PinControl"],Pin.IN,Pin.PULL_UP)

    InPutFR.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=FRcallback)
    InPutLR.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=LRcallback)
    InPutUD.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=UDcallback)
    InPutRT.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=RTcallback)
    
    LFront = PWM(Pin(PwmInfo["LF"]["PinOutput"]), freq=PwmInfo["LF"]["NowSpeed"], duty=2500)#左前
    RFront = PWM(Pin(PwmInfo["RF"]["PinOutput"]), freq=PwmInfo["LF"]["NowSpeed"], duty=2500)#右前
    LRear  = PWM(Pin(PwmInfo["LR"]["PinOutput"]), freq=PwmInfo["LF"]["NowSpeed"], duty=2500)#左后
    RRear  = PWM(Pin(PwmInfo["RR"]["PinOutput"]), freq=PwmInfo["LF"]["NowSpeed"], duty=2500)#右后
    time_init(25)
    while True:
        #更新周期
        for key in PwmInfo.keys():
            print(key)
            print(PwmInfo[key]["AimSpeed"])
            print(PwmInfo[key]["AimDir"])
        utime.sleep_ms(1000)
        pass

if __name__ == "__main__":
    main()

