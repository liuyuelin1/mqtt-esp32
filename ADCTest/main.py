import machine
from machine import Pin,ADC,Timer,UART
from libKalman import KalmanFiter
import utime

#配置版本号
VERSION = "V0.1.0"

FRadc = ADC(Pin(34)) 
FRFiter = KalmanFiter(TestPool = 10,PredictPool = 5)
LRadc = ADC(Pin(35)) 
LRFiter = KalmanFiter(TestPool = 10,PredictPool = 5)
UDadc = ADC(Pin(36)) 
UDFiter = KalmanFiter(TestPool = 10,PredictPool = 5)
RTadc = ADC(Pin(39)) 
RTFiter = KalmanFiter(TestPool = 10,PredictPool = 5)
uart = UART(2, baudrate=115200, rx=5,tx=18,timeout=10)

ControlInfo={
"FR":{"Voltage":0,"ADC":FRadc,"Fiter":FRFiter,"Bytes":bytes([])},
"LR":{"Voltage":0,"ADC":LRadc,"Fiter":LRFiter,"Bytes":bytes([])},
"UD":{"Voltage":0,"ADC":UDadc,"Fiter":UDFiter,"Bytes":bytes([])},
"RT":{"Voltage":0,"ADC":RTadc,"Fiter":RTFiter,"Bytes":bytes([])}
}

def timing(tim):
    start = bytes([0xaf])
    end   = bytes([0xbf])
    global ControlInfo
    for key in ControlInfo.keys():
        ControlInfo[key]["Voltage"] = int(ControlInfo[key]["Fiter"].GetNewData(ControlInfo[key]["ADC"].read()))+1
    ControlInfo["FR"]["Bytes"] = ControlInfo["FR"]["Voltage"].to_bytes(2,'big')
    ControlInfo["LR"]["Bytes"] = ControlInfo["LR"]["Voltage"].to_bytes(2,'big')
    ControlInfo["UD"]["Bytes"] = ControlInfo["UD"]["Voltage"].to_bytes(2,'big')
    ControlInfo["RT"]["Bytes"] = ControlInfo["RT"]["Voltage"].to_bytes(2,'big')
    data = start+ControlInfo["FR"]["Bytes"]+ControlInfo["LR"]["Bytes"]+ControlInfo["UD"]["Bytes"]+ControlInfo["RT"]["Bytes"]+end
    uart.write(data)

def time_init(ms):
    tm=Timer(4)
    tm.init(period=ms, mode=Timer.PERIODIC, callback=timing)

def main(): 
    global ControlInfo
    for key in ControlInfo.keys():
        ControlInfo[key]["ADC"].atten(ADC.ATTN_11DB)
    time_init(20)
    while True:
        utime.sleep_ms(1000)
        pass

if __name__ == "__main__":
    main()





