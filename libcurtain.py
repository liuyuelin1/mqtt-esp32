from machine import Pin
import time
class CurtainDriver:
    register_type = 'Curtain'

    def __init__(self, motorLeft, motorRight, leftLimit, rightLimit, leftTouch, rightTouch, timeout = 100):

        self.leftLimit  = Pin(leftLimit , Pin.IN)
        self.rightLimit = Pin(rightLimit, Pin.IN)

        self.motorLeft  = Pin(motorLeft, Pin.OUT)
        self.motorRight = Pin(motorRight, Pin.OUT)
        self.clutchPin  = Pin(clutch_pin, Pin.OUT)
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