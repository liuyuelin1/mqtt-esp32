from machine import Timer
from libcam  import CAMDriver
import utime
#Timer.ONE_SHOT

def ew(t):
    global cam
    cam.communicate()
    print("laile")

def timinit():
    tim = Timer(-1)
    tim.init(mode=Timer.PERIODIC,period=1000,callback=ew)

def main():
    global cam
    cam=CAMDriver(25)
    
    #timinit()
    text=0x18
    tmp=10
    color=0xf0
    tmpc=1
    while True:
        cam.communicate()
        utime.sleep_ms(100)
        if 1 == cam.moduleTypeInfo(0):
            print(text)
            print(cam.getInputByte(0))
            #cam.setServoColor(0,color)
            #cam.setServoColor(1,color)
            #cam.setServoColor(2,color)
            #cam.setServoColor(3,color)
            cam.setServoPosition(0,text)
            cam.setServoPosition(1,text)
            cam.setServoPosition(2,text)
            cam.setServoPosition(3,text)
            
        text=text+tmp
        if  text<0x18:
            text=0x18
            tmp=1
        elif text>0xe4:
            text=0xe4
            tmp=-1
            
        color=color+tmpc
        if  color<0xf0:
            color=0xf0
            tmpc=1
            cam.setServoColor(0,color)
        elif color>0xf7:
            color=0xf7
            tmpc=-1
            
        pass

if __name__ == "__main__":
    main()