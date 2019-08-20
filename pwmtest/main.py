import machine
from machine import Pin
from machine import PWM
import utime

#配置版本号
VERSION = "V0.1.0"
ytime=0
gtimemax=0
gtimemin=0

averageindex=10
gindex=0
gdifftm=[0]*averageindex
def setdiff(diff):
    global gindex
    global gdifftm
    gindex=gindex+1
    if gindex>=averageindex:
        gindex=0
    gdifftm[gindex]=diff
def getdiff():
    global gdifftm
    diff=0
    for i in range(1,averageindex):
        if diff<gdifftm[i-1]:
            diff=gdifftm[i-1]
    return diff
def callback(p):
    global ytime
    global gtimemax
    global gtimemin
    if p.value():
        ytime=utime.ticks_cpu()
    else:
        setdiff(utime.ticks_diff(utime.ticks_cpu(),ytime))
        print(getdiff())
input=Pin(4,Pin.IN,Pin.PULL_UP)

input.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=callback,hard=True)


#状态灯
run_cyc=10
run_index=0
#运行状态灯，0.5HZ闪烁
def runstatus():
    global run_index
    run_index = run_index +1
    if run_index<run_cyc:
        Pin(5, Pin.OUT, value=1)
    elif run_index<(run_cyc*2):
        Pin(5, Pin.OUT, value=0)
    else:
        run_index=0
        ytime=utime.ticks_us()
        utime.sleep_ms(1000)
        print(utime.ticks_diff(utime.ticks_us(),ytime))
def main(): 
    pwm0 = PWM(Pin(2), freq=50, duty=50)
    dutyx=51
    tmp=10
    global gtimemax
    while True:
        #runstatus()
        dutyx=dutyx+tmp
        if  dutyx<1:
            dutyx=1
            tmp=10
        elif dutyx>1022:
            dutyx=1022
            tmp=-10
        pwm0.duty(dutyx) 
        utime.sleep_ms(10)
        #print(dutyx)
        print(getdiff())
        #print(utime.ticks_add(0, -1))
        pass

if __name__ == "__main__":
    main()





