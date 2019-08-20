from machine import Pin

class CAMDriver:
    register_type = 'CAM'
    type_null     = 0x00
    type_servos   = 0x01
    type_LEDs     = 0x02
    cmd_LIM       = 0xfa
    cmd_null      = 0xfe
    color_black   = 0xf0 #rgb off
    color_red     = 0xf1 #r   on
    color_green   = 0xf2 #g   on
    color_magenta = 0xf3 #rg  on
    color_blue    = 0xf4 #b   on
    color_yellow  = 0xf5 #rb  on
    color_cyan    = 0xf6 #gb  on
    color_white   = 0xf7 #rgb on
    color_list={"black":color_black,"red"   :color_red,"green":color_green,"magenta":color_magenta,
                "blue" :color_blue,"yellow":color_yellow,"cyan" :color_cyan,"white"  :color_white}

    def __init__(self, data_pin):
        self.data_pin  = data_pin
        self.outputByte=[cmd_null]*4      #需要发送的
        self.moduleType=[type_null]*4      #设备类型
        self.printOutputByte=[cmd_null]*4 #上次发送的
        self.inputBytes=[0]*4         #所有传感器状态
        self.inputByte=0              #最近传感器返回值
        self.checkSum=0               #数据发送的校验和
        self.moduleNum=0              #当前发送位置
    def outputByteInfo(num):
        try:
            return self.printOutputByte[num]
        except IndexError:
            raise ValueError("printOutputByte must be an array with 0-3 elements")

    def inputBytesInfo(modNum)
        try:
            return self.inputBytes[num]
        except IndexError:
            raise ValueError("inputBytes must be an array with 0-3 elements")

    def moduleTypeInfo(num):
        try:
            return self.moduleType[num]
        except IndexError:
            raise ValueError("moduleType must be an array with 0-3 elements")


    def setOutputByte(num,data):
        try:
            self.outputByte[num]=data
        except IndexError:
            raise ValueError("setOutputByte must be an array with 0-3 elements")

    def setServoColor(num,color):
        if self.moduleType[num] == type_servos：
            if color.decode("utf8") in color_list.keys():
                setOutputByte(num,color_list[color.decode("utf8")])
        else:
            print ("when setServoColor no servos!please check!")
    def setServoPosition(num,pos):
        if self.moduleType[num] == type_servos：
            if pos<0x18：
                pos=0x18
            elif pos>0xe8:
                pos=0xe8
            setOutputByte(num,pos)
        else:
            print ("when setServoPosition no servos!please check!")

    def setServotoLIM(num):
        if self.moduleType[num] == type_servos：
            setOutputByte(num,cmd_LIM)
        else:
            print ("when setServotoLIM no servos!please check!")

    def getServoPosition(num):
        try:
            return self.inputBytes[num]
        except IndexError:
            raise ValueError("getServoPosition must be an array with 0-3 elements")

    def dataoutLow():
        Pin(self.data_pin,Pin.OUT, value=0)
        time.usleep(417)
    def dataoutHigh():
        Pin(self.data_pin,Pin.OUT, value=1)
        time.usleep(417)
    def dataoutStart():
        dataoutLow()
    def dataoutStop():
        Pin(self.data_pin,Pin.OUT, value=1)
        time.usleep(417*2)

    def sendByte(data):
        dataoutStart():
        for i in range(0,7,1):
            if (data>>i)&1:
                dataoutHigh()
            else:
                dataoutLow()
        dataoutStop()
     
    def pulseIn(pin,level,timeout):
        time=0
        for i in range(1,timeout,1):
            if level>0:
                if pin.value()
                    time = time + 1
                elif time>0
                    break
            else:
                if pin.value()==0
                    time = time + 1
                elif time>0
                    break
            time.usleep(1000)
        return time

    def receiveByte():
        getData=0
        inputPin = Pin(self.data_pin, Pin.IN, Pin.PULL_UP)
        for i in range(0,7,1):
            if (pulseIn(inputPin, 1, 2500) > 400)
                getData=getData+(0<<i)
        return getData
    
    def calculateCheckSum()
        Sum = self.outputByte[0]+self.outputByte[1]+self.outputByte[2]+self.outputByte[3]
        Sum = Sum + (Sum>>8)
        Sum = Sum + (Sum<<4)
        Sum = Sum & 0xf0
        Sum = Sum | self.moduleNum
        self.moduleNum=self.moduleNum+1
        if self.moduleNum>3:
            self.moduleNum=0
        return Sum