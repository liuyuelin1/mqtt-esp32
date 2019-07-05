from machine import Pin

class HC165Driver:
    register_type = '74HC165'

    """
    data_pin => pin 14 on the 74HC165
    latch_pin => pin 12 on the 74HC165
    clock_pin => pin 11 on the 74HC165
    """
    def __init__(self, data_pin, latch_pin, clock_pin, series_num=1):
        self.data_pin  = data_pin
        self.latch_pin = latch_pin
        self.clock_pin = clock_pin
        self.series_num = series_num
        self.DataPin  = Pin(self.data_pin , Pin.IN)
        self.LatchPin = Pin(self.latch_pin, Pin.OUT)
        self.ClkPin   = Pin(self.clock_pin, Pin.OUT)

        self.inputs = [0] * (self.series_num*8)

    """
    input_number => Value from 0 to 7 pointing to the input pin on the 74HC165
    0 => Q0 pin 15 on the 74HC165
    1 => Q1 pin 1 on the 74HC165
    2 => Q2 pin 2 on the 74HC165
    3 => Q3 pin 3 on the 74HC165
    4 => Q4 pin 4 on the 74HC165
    5 => Q5 pin 5 on the 74HC165
    6 => Q6 pin 6 on the 74HC165
    7 => Q7 pin 7 on the 74HC165
    value => a state to pass to the pin, could be HIGH or LOW
    """
    def getInput(self, input_number):
        try:
            return self.inputs[input_number]
        except IndexError:
            raise ValueError("setInputs must be an array with %d elements" %(self.series_num*8-1))

    def getInputs(self):
        return self.inputs

    def latch(self):
        self.LatchPin.off()
        self.LatchPin.on()
        for i in range((self.series_num*8-1), -1, -1):
            self.inputs[i]=self.DataPin.value()
            self.ClkPin.off()
            self.ClkPin.on()
        print(self.inputs)
        