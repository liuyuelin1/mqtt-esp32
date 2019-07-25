
import machine
from machine import Pin
from machine import PWM
import utime

#配置版本号
VERSION = "V0.1.0"
ytime=0
gtimemax=0
gtimemin=0

gindex=0
gdifftm=[0]*5
def setdiff(diff):
    global gindex
    global gdifftm
    gindex=gindex+1
    if gindex>4:
        gindex=0
    gdifftm[gindex]=diff
def getdiff():
    global gdifftm
    diff=0
    for i in range(4):
        if diff<gdifftm[i]:
            diff=gdifftm[i]
    return diff
def callback(p):
    global ytime
    global gtimemax
    global gtimemin
    if p.value():
        ytime=utime.ticks_us()
    else:
        setdiff(utime.ticks_diff(utime.ticks_us(),ytime))

input=Pin(4,Pin.IN,Pin.PULL_UP)

input.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=callback)


#状态灯
run_cyc=10
run_index=0
#运行状态灯，0.5HZ闪烁
def runstatus():
    global run_index
    run_index = run_index +1
    if run_index<run_cyc:
        Pin(2, Pin.OUT, value=1)
    elif run_index<(run_cyc*2):
        Pin(2, Pin.OUT, value=0)
    else:
        run_index=0
        ytime=utime.ticks_us()
        utime.sleep_ms(1000)
        print(utime.ticks_diff(utime.ticks_us(),ytime))
def main(): 
    pwm0 = PWM(Pin(2), freq=50, duty=54)
    dutyx=51
    global gtimemax
    #pwm0.duty(dutyx) 
    while True:
        dutyx=dutyx+1
        if  dutyx>60:
            dutyx=51
        pwm0.duty(dutyx) 
        utime.sleep(0.5)
        print(getdiff())
        pass

if __name__ == "__main__":
    main()
