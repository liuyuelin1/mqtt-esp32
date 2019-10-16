from machine import Pin,PWM
import utime
from libKalman import KalmanFiter

#配置版本号
VERSION = "V0.1.0"
ytime=[0]*4

FRFiter = KalmanFiter(TestPool = 10,PredictPool = 5)
LRFiter = KalmanFiter(TestPool = 10,PredictPool = 5)
UDFiter = KalmanFiter(TestPool = 10,PredictPool = 5)
RTFiter = KalmanFiter(TestPool = 10,PredictPool = 5)

ControlInfo={
"FR":{"DutyTm":0,"Fiter":FRFiter},
"LR":{"DutyTm":0,"Fiter":LRFiter},
"UD":{"DutyTm":0,"Fiter":UDFiter},
"RT":{"DutyTm":0,"Fiter":RTFiter}
}

def callback1(p):
    global ControlInfo
    global ytime
    if p.value():
        ytime[0]=utime.ticks_us()
    else:
        ControlInfo["FR"]["DutyTm"] = ControlInfo["FR"]["Fiter"].GetNewData(utime.ticks_diff(utime.ticks_us(),ytime[0]))
def callback2(p):
    global ControlInfo
    global ytime
    if p.value():
        ytime[1]=utime.ticks_us()
    else:
        ControlInfo["LR"]["DutyTm"] = ControlInfo["LR"]["Fiter"].GetNewData(utime.ticks_diff(utime.ticks_us(),ytime[1]))
def callback3(p):
    global ControlInfo
    global ytime
    if p.value():
        ytime[2]=utime.ticks_us()
    else:
        ControlInfo["UD"]["DutyTm"] = ControlInfo["UD"]["Fiter"].GetNewData(utime.ticks_diff(utime.ticks_us(),ytime[2]))
def callback4(p):
    global ControlInfo
    global ytime
    if p.value():
        ytime[3]=utime.ticks_us()
    else:
        ControlInfo["RT"]["DutyTm"] = ControlInfo["RT"]["Fiter"].GetNewData(utime.ticks_diff(utime.ticks_us(),ytime[3]))

input=Pin(5,Pin.IN,Pin.PULL_UP)
input.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=callback1)
input=Pin(18,Pin.IN,Pin.PULL_UP)
input.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=callback2)
input=Pin(19,Pin.IN,Pin.PULL_UP)
input.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=callback3)
input=Pin(21,Pin.IN,Pin.PULL_UP)
input.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=callback4)

def main(): 
    global ControlInfo
    while True:

        utime.sleep_ms(100)
        pass

if __name__ == "__main__":
    main()
