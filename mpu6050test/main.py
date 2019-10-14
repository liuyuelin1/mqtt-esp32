from machine import Pin,I2C,Timer
import time,math
from mpu6050 import Mpu6050
i2c = I2C(scl=Pin(5), sda=Pin(4))
MpuClient = Mpu6050(i2c)
MpuClient.error_gy()

i=0
gyx=0
gyy=0
r=[]
q=[]

def timing(tim):
	global MpuClient
	global i
	global q
	i=i+1
	ay = MpuClient.get_values()
	if i==124:
		print("Pitch:" + str(q[0]))
		print("Roll:" + str(q[1]))
		print("yaw:" + str(q[2]))
#		print("Pitch:" + str(ay["GyX"]))
#		print("Roll:" + str(ay["GyY"]))
#		print("yaw:" + str(ay["GyZ"]))
		i=0
	q=MpuClient.IMUupdate(ay["GyX"],ay["GyY"],ay["GyZ"],ay["AcX"],ay["AcY"],ay["AcZ"])

def time_init(ms):
	tm=Timer(4)
	tm.init(period=ms, mode=Timer.PERIODIC, callback=timing)
time_init(8)
while 1:
	time.sleep(0.003)
