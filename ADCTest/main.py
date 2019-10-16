import machine
from machine import Pin,ADC
from libKalman import KalmanFiter
import utime

#配置版本号
VERSION = "V0.1.0"
adc = ADC(Pin(36)) 
LR = KalmanFiter(TestPool = 10,PredictPool = 5)
adc1 = ADC(Pin(39)) 
RF = KalmanFiter(TestPool = 10,PredictPool = 5)
def main(): 
    adc.atten(ADC.ATTN_11DB)
    adc1.atten(ADC.ATTN_11DB)
    while True:
        print("ADC36")
        print(LR.GetNewData(adc.read()))
        print("ADC39")
        print(RF.GetNewData(adc1.read()))
        utime.sleep_ms(100)
        pass

if __name__ == "__main__":
    main()





