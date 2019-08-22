from machine import Pin
import utime
class CAMDriver:
    register_type = 'CAM'
    def __init__(self, data_pin):
        self.us417         = 317
        self.us400         = 400
        self.usTMout       = 1200
        self.data_pin      = data_pin
        self.type_null     = 0x00
        self.type_servo    = 0x01
        self.type_LEDs     = 0x02
        self.type_no_reply = 0xff
        self.cmd_LIM       = 0xfa
        self.cmd_IDAsk     = 0xfc
        self.cmd_null      = 0xfe
        self.new_exist     = 0xfe
        self.start_byte    = 0xff
        self.color_black   = 0xf0 #rgb off
        self.color_red     = 0xf1 #r   on
        self.color_green   = 0xf2 #g   on
        self.color_magenta = 0xf3 #rg  on
        self.color_blue    = 0xf4 #b   on
        self.color_yellow  = 0xf5 #rb  on
        self.color_cyan    = 0xf6 #gb  on
        self.color_white   = 0xf7 #rgb on
        self.outputByte=[self.cmd_null]*4      #需要发送的
        self.moduleType=[self.type_null]*4     #设备类型
        self.inputBytes=[0]*4             #所有传感器状态
        self.inputByte=0                  #最近传感器返回值
        self.checkSum=0                   #数据发送的校验和
        self.nowSite=0                  #当前发送位置

    #设置实际返回值
    def setInputByte(self,num,data):
        try:
            self.inputBytes[num]=data
        except IndexError:
            raise ValueError("inputBytes must be an array with 0-3 elements")
    #查看num位置设备类型
    def getInputByte(self,num):
        try:
            return self.inputBytes[num]
        except IndexError:
            raise ValueError("inputBytes must be an array with 0-3 elements")
    #设置设备类型
    def setModuleType(self,num,type):
        try:
            self.moduleType[num]=type
        except IndexError:
            raise ValueError("moduleType must be an array with 0-3 elements")
    #查看num位置设备类型
    def moduleTypeInfo(self,num):
        try:
            return self.moduleType[num]
        except IndexError:
            raise ValueError("moduleType must be an array with 0-3 elements")
    #设置num位设备值
    def setOutputByte(self,num,data):
        try:
            self.outputByte[num]=data
        except IndexError:
            raise ValueError("outputByte must be an array with 0-3 elements")
    #查看num设备设置值
    def getOutputByte(self,num):
        try:
            return self.outputByte[num]
        except IndexError:
            raise ValueError("outputByte must be an array with 0-3 elements")
    #设置num设备角度
    def setServoPosition(self,num,pos):
        if self.moduleType[num] == self.type_servo:
            if pos<0x18:
                pos=0x18
            elif pos>0xe8:
                pos=0xe8
            self.setOutputByte(num,pos)
        else:
            print ("when setServoPosition no servos!please check!")
    #设置num位灯颜色
    def setServoColor(self,num,color):
        if self.moduleType[num] == self.type_servo:
            if color<self.color_black:
                color=self.color_black
            elif color>self.color_white:
                color=self.color_white
            self.setOutputByte(num,color)
        else:
            print ("when setServoColor no servos!please check!")
    #设置num设备学习模式
    def setServotoLIM(self,num):
        if self.moduleType[num] == self.type_servo:
            self.setOutputByte(num,self.cmd_LIM)
        else:
            print ("when setServotoLIM no servos!please check!")

    def dataoutLow(self,outputPin):
        outputPin.off()
        utime.sleep_us(self.us417)
    def dataoutHigh(self,outputPin):
        outputPin.on()
        utime.sleep_us(self.us417)
    def dataoutStart(self,outputPin):
        self.dataoutLow(outputPin)
    def dataoutStop(self,outputPin):#2*self.us417
        self.dataoutHigh(outputPin)
        self.dataoutHigh(outputPin)
    #发送一个字节
    def sendByte(self,data):
        outputPin = Pin(self.data_pin,Pin.OUT, value=1)
        self.dataoutStart(outputPin)
        for i in range(0,8,1):
            if (data>>i)&0x01:
                self.dataoutHigh(outputPin)
            else:
                self.dataoutLow(outputPin)
        self.dataoutStop(outputPin)
    #获取一个脉宽
    def pulseIn(self,inputPin,timeout):
        time=0
        ytime=utime.ticks_us()
        while not inputPin.value():
            utime.sleep_us(10)
            time = utime.ticks_diff(utime.ticks_us(),ytime)
            if time > timeout:
                break
            pass
        time=0
        ytime=utime.ticks_us()    
        while inputPin.value():
            utime.sleep_us(10)
            time = utime.ticks_diff(utime.ticks_us(),ytime)
            if time > timeout:
                break
            pass
        return time
    #获取一次输入
    def receiveByte(self):
        getData=0
        inputPin = Pin(self.data_pin, Pin.IN, Pin.PULL_UP)
        utime.sleep_us(1000)
        for i in range(0,8,1):
            if (self.pulseIn(inputPin, self.usTMout) > self.us400):
                getData=getData+(1<<i)
        return getData
    #获取校验和
    def CheckSum(self):
        Sum = self.outputByte[0]+self.outputByte[1]+self.outputByte[2]+self.outputByte[3]
        Sum = Sum + (Sum>>8)
        Sum = Sum + (Sum<<4)
        Sum = Sum & 0xf0
        Sum = Sum | self.nowSite
        return Sum
    def communicate(self):
        self.checkSum = self.CheckSum()
        self.sendByte(self.start_byte)
        for i in range(0,4,1):
            self.sendByte(self.outputByte[i])
        self.sendByte(self.checkSum)
        self.inputByte = self.receiveByte()
        if self.inputByte == self.new_exist:
            self.setOutputByte(self.nowSite,self.cmd_IDAsk)
        elif self.inputByte == self.type_servo:
            self.setModuleType(self.nowSite,self.type_servo)
            self.setServoColor(self.nowSite,self.color_cyan)
        elif self.inputByte == self.type_LEDs:
            self.setModuleType(self.nowSite,self.type_LEDs) 
            #设置灯颜色
        elif self.inputByte == self.type_no_reply:
            self.setModuleType(self.nowSite,self.type_null)
            self.setOutputByte(self.nowSite,self.cmd_null)
        elif self.inputByte == self.type_null:    
            for i in range(0,4,1):
                self.setModuleType(i,self.type_null)
                self.setOutputByte(i,self.cmd_null)
        else:
            self.setInputByte(self.nowSite,self.inputByte)
        self.nowSite=self.nowSite+1
        if self.nowSite>3:
            self.nowSite=0