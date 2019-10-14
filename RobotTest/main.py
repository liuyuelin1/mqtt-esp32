import machine
from machine import Pin
from machine import PWM
import utime

#配置版本号
VERSION = "V0.1.0"

FilterScale=10
FilterIndex=0
DutyTmLF=[0]*FilterScale
DutyTmRF=[0]*FilterScale
DutyTmLR=[0]*FilterScale
DutyTmRR=[0]*FilterScale

PwmInfo={
"LF":{"PinOutput":12,"freq":50},
"RF":{"PinOutput":13,"freq":50},
"LR":{"PinOutput":14,"freq":50},
"RR":{"PinOutput":27,"freq":50}
}

ControlInfo={
"FR":{"PinControl":5 ,"TempTM":0,"FilterIndex":0,"DutyTm":DutyTmLF},
"LR":{"PinControl":18,"TempTM":0,"FilterIndex":0,"DutyTm":DutyTmRF},
"RT":{"PinControl":19,"TempTM":0,"FilterIndex":0,"DutyTm":DutyTmLR}
}


def SetDutyTm(diff,Member):#记录一次占空比
    global ControlInfo
    ControlInfo[Member]["FilterIndex"]=ControlInfo[Member]["FilterIndex"]+1
    if ControlInfo[Member]["FilterIndex"]>=FilterScale:
        ControlInfo[Member]["FilterIndex"]=0
    ControlInfo[Member]["DutyTm"][ControlInfo[Member]["FilterIndex"]]=diff

def GetDutyTm(Member):#获取一次滤波后的占空比
    global ControlInfo
    diff=0
    for i in range(1,FilterScale):
        if diff<ControlInfo[Member]["DutyTm"][i-1]:
            diff=ControlInfo[Member]["DutyTm"][i-1]
    print(Member)
    print(diff)
    return diff

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

def RTcallback(p):
    global ControlInfo
    if p.value():
        ControlInfo["RT"]["TempTM"]=utime.ticks_us()
    else:
        SetDutyTm(utime.ticks_diff(utime.ticks_us(),ControlInfo["RT"]["TempTM"]),"RT")

def main(): 
    global ControlInfo
    #遥控输入脚，中断方式记录数据
    InPutFR=Pin(ControlInfo["FR"]["PinControl"],Pin.IN,Pin.PULL_UP)
    InPutLR=Pin(ControlInfo["LR"]["PinControl"],Pin.IN,Pin.PULL_UP)
    InPutRT=Pin(ControlInfo["RT"]["PinControl"],Pin.IN,Pin.PULL_UP)

    InPutFR.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=FRcallback)
    InPutLR.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=LRcallback)
    InPutRT.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=RTcallback)
    
    LFront = PWM(Pin(PwmInfo["LF"]["PinOutput"]), freq=PwmInfo["LF"]["freq"], duty=2)#左前
    RFront = PWM(Pin(PwmInfo["RF"]["PinOutput"]), freq=PwmInfo["LF"]["freq"], duty=2)#右前
    LRear  = PWM(Pin(PwmInfo["LR"]["PinOutput"]), freq=PwmInfo["LF"]["freq"], duty=2)#左后
    RRear  = PWM(Pin(PwmInfo["RR"]["PinOutput"]), freq=PwmInfo["LF"]["freq"], duty=2)#右后
    while True:
        GetDutyTm("FR")#更新遥控值 前后
        GetDutyTm("LR")#更新遥控值 左右
        GetDutyTm("RT")#更新遥控值 自旋
        #更新周期
        utime.sleep_ms(1000)
        pass

if __name__ == "__main__":
    main()

