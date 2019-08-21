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
    while True:
        cam.communicate()
        utime.sleep_ms(100)
        if 1 == cam.moduleTypeInfo(0):
            print(text)
            print(cam.getInputByte(0))
            cam.setServoPosition(0,text)
        text=text+tmp
        if  text<0x18:
            text=0x18
            tmp=10
        elif text>0xe8:
            text=0xe8
            tmp=-10
        pass

if __name__ == "__main__":
    main()