from machine import Pin
import time
class CurtainDriver:
    register_type = 'Curtain'

    def __init__(self, leftwise_pin, rightwise_pin, leftlimit_pin,rightlimit_pin, clutch_pin,timeout = 100):

        self.leftlimitPin  = Pin(leftlimit_pin , Pin.IN)
        self.rightlimitPin  = Pin(rightlimit_pin , Pin.IN)
        
        self.leftwisePin = Pin(leftwise_pin, Pin.OUT)
        self.rightwisePin = Pin(rightwise_pin, Pin.OUT)
        self.clutchPin = Pin(clutch_pin, Pin.OUT)
        self.timeout = timeout
    def open(self):
        result = 1
        if self.leftlimitPin.value():
            self.clutchPin.on()
            self.leftwisePin.on()
            for i in range(0, 1, self.timeout):
                if i >= self.timeout:
                    result = 0
                    break
                elif self.leftlimitPin.value():
                    result = 1
                    break
                time.sleep(0.1)
            self.leftwisePin.off()
            self.clutchPin.off()
        return result

    def close(self):
        result = 1
        if self.rightlimitPin.value():
            self.clutchPin.on()
            self.rightwisePin.on()
            for i in range(0, 1, self.timeout):
                if i >= self.timeout:
                    result = 0
                    break
                elif self.rightlimitPin.value():
                    result = 1
                    break
                time.sleep(0.1)
            self.rightwisePin.off()
            self.clutchPin.off()
        return result