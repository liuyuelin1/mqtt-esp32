import time
from machine import I2C, Pin,Timer
from mpu9250 import MPU9250

i2c = I2C(scl=Pin(5), sda=Pin(4))
sensor = MPU9250(i2c)

i=0
q=[]

def timing(tim):
	global sensor
	global i
	global q
	i=i+1
	ay = sensor.get_values()
	if i==124:
		print("Pitch:" + str(q[0]))
		print("Roll:" + str(q[1]))
		print("yaw:" + str(q[2]))
		i=0
	q=sensor.AHRSupdate()

def time_init(ms):
	tm=Timer(4)
	tm.init(period=ms, mode=Timer.PERIODIC, callback=timing)

time_init(8)
while 1:
	time.sleep(1000)
