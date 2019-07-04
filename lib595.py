from machine import Pin

class ShiftRegister:
    register_type = '74HC595'

    """
    data_pin => pin 14 on the 74HC595
    latch_pin => pin 12 on the 74HC595
    clock_pin => pin 11 on the 74HC595
    """
    def __init__(self, data_pin, latch_pin, clock_pin):
        self.data_pin = data_pin
        self.latch_pin = latch_pin
        self.clock_pin = clock_pin

        Pin(self.data_pin, Pin.OUT)
        Pin(self.latch_pin, Pin.OUT)
        Pin(self.clock_pin, Pin.OUT)

        self.outputs = [0] * 8

    """
    output_number => Value from 0 to 7 pointing to the output pin on the 74HC595
    0 => Q0 pin 15 on the 74HC595
    1 => Q1 pin 1 on the 74HC595
    2 => Q2 pin 2 on the 74HC595
    3 => Q3 pin 3 on the 74HC595
    4 => Q4 pin 4 on the 74HC595
    5 => Q5 pin 5 on the 74HC595
    6 => Q6 pin 6 on the 74HC595
    7 => Q7 pin 7 on the 74HC595
    value => a state to pass to the pin, could be HIGH or LOW
    """
    def setOutput(self, output_number, value):
        try:
            self.outputs[output_number] = value
        except IndexError:
            raise ValueError("Invalid output number. Can be only an int from 0 to 7")

    def setOutputs(self, outputs):
        if 8 != len(outputs):
            raise ValueError("setOutputs must be an array with 8 elements")

        self.outputs = outputs

    def latch(self):
        Pin(self.latch_pin,Pin.OUT, 0)
        for i in range(7, -1, -1):
            Pin(self.clock_pin, 0)
            Pin(self.data_pin,Pin.OUT, self.outputs[i])
            Pin(self.clock_pin,Pin.OUT, 1)
        Pin(self.latch_pin,Pin.OUT, 1)
        print(self.outputs)