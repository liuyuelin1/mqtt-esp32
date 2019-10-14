import machine
from time import sleep,sleep_ms
import math
class KalmanFiter:
	def __init__(self, LastOptimal=1, LastOptimalPool=1,TestPool=0.001,PredictPool=0.001):
		self.LastOptimal = LastOptimal
		self.LastOptimalPool = LastOptimalPool
		self.TestPool = TestPool#越小越跟随
		self.PredictPool = PredictPool#越小越平滑

	def GetNewData(self,temp):
		Test = temp
		Predict = self.LastOptimal
		NowPool = self.PredictPool + self.LastOptimalPool
		Credibility = math.sqrt(NowPool/(NowPool + self.TestPool))
		Optimal = Predict + Credibility*(Test - Predict)
		OptimalPool = NowPool*(1-Credibility)
		self.LastOptimal = Optimal
		self.LastOptimalPool = OptimalPool
		return Optimal

class Mpu6050():

	def __init__(self, i2c, addr=0x68):
		self.error=[0,0,0]
		self.Kp=120 #比例增益支配率收敛到加速度计/磁强计
		self.Ki=0.5 #积分增益支配率的陀螺仪偏见的衔接
		self.halfT=0.004 #采样周期的一半
		self.q0=1
		self.q1=0
		self.q2=0
		self.q3=0; #四元数的元素 ,代表估计方向
		self.HalfEXInt=0
		self.HalfEYInt=0
		self.HalfEZInt=0 #按比例缩小积分误差
		self.iic = i2c
		self.addr = addr
		self.AX = KalmanFiter(TestPool = 100)#滤波，加速度计，需要稳定性
		self.AY = KalmanFiter(TestPool = 100)
		self.AZ = KalmanFiter(TestPool = 100)
		self.GX = KalmanFiter(PredictPool = 100)#角速度，需要及时
		self.GY = KalmanFiter(PredictPool = 100)
		self.GZ = KalmanFiter(PredictPool = 100)
		self.iic.start()
		self.iic.writeto_mem(self.addr,0x6B,b'\x80') #设备复位
		sleep_ms(1)
		self.iic.stop()
		sleep_ms(100)
		self.iic.start()
		self.iic.writeto_mem(self.addr,0x6B,b'\x00')  #low filter 21hz
		sleep_ms(1)
		self.iic.stop()
		sleep_ms(100)
		self.iic.start()
		sleep_ms(1)
		self.iic.writeto(self.addr, bytearray([107, 0]))
		sleep_ms(1)
		self.iic.writeto_mem(self.addr,0x19,b'\x07') #gyro 125hz
		sleep_ms(1)
		self.iic.writeto_mem(self.addr,0x1a,b'\x04')  #low filter 21hz
		sleep_ms(1)
		self.iic.writeto_mem(self.addr,0x1b,b'\x08') #gryo 500/s 65.5lsb/g
		sleep_ms(1)
		self.iic.writeto_mem(self.addr,0x1c,b'\x08') #acceler 4g ,8192lsb.g
		sleep_ms(1)
		self.iic.stop()

		sleep_ms(100)
		self.iic.start()
		self.iic.writeto_mem(self.addr,0x6B,b'\x01')  #read init 6050
		sleep_ms(1)
		self.iic.stop()

	def get_raw_values(self):
		self.iic.start()
		a = self.iic.readfrom_mem(self.addr, 0x3B, 14)
		self.iic.stop()
		return a

	def get_ints(self):
		b = self.get_raw_values()
		c = []
		for i in b:
			c.append(i)
		return c

	def bytes_toint(self, firstbyte, secondbyte):
		if not firstbyte & 0x80:
			return firstbyte << 8 | secondbyte
		return - (((firstbyte ^ 255) << 8) | (secondbyte ^ 255) + 1)

	def error_gy(self):
		sleep(3)
		for i in range(0,10):
				raw_ints = self.get_raw_values()
				self.error[0] = self.GX.GetNewData(self.bytes_toint(raw_ints[8], raw_ints[9]))
				self.error[1] = self.GY.GetNewData(self.bytes_toint(raw_ints[10], raw_ints[11]))
				self.error[2] = self.GZ.GetNewData(self.bytes_toint(raw_ints[12], raw_ints[13]))
				sleep_ms(8)

	def get_values(self):
		vals = {}
		raw_ints = self.get_raw_values()
		vals["AcX"] = self.AX.GetNewData(self.bytes_toint(raw_ints[0], raw_ints[1]))
		vals["AcY"] = self.AY.GetNewData(self.bytes_toint(raw_ints[2], raw_ints[3]))
		vals["AcZ"] = self.AZ.GetNewData(self.bytes_toint(raw_ints[4], raw_ints[5]))
		vals["Tmp"] = self.bytes_toint(raw_ints[6], raw_ints[7]) / 340.00 + 36.53
		vals["GyX"] = self.GX.GetNewData(self.bytes_toint(raw_ints[8], raw_ints[9])-self.error[0])
		vals["GyY"] = self.GY.GetNewData(self.bytes_toint(raw_ints[10], raw_ints[11])-self.error[1])
		vals["GyZ"] = self.GZ.GetNewData(self.bytes_toint(raw_ints[12], raw_ints[13])-self.error[2])
		return vals  # returned in range of Int16
		# -32768 to 32767

	def IMUupdate(self,gx,gy,gz,ax,ay,az):
		#单位统一
		gx = gx/65.5*0.0174533#陀螺仪量程 +-500度/S
		gy = gy/65.5*0.0174533#转换关系65.5LSB/度
		gz = gz/65.5*0.0174533#每度的弧度值约0.01745
		ax = ax/8192#加速度量程 +-4g/S
		ay = ay/8192#转换关系8192LSB/g
		az = az/8192
		a=[0,0,0,0,0,0,0,0]
		norm = 1
		if ax!=0 or ay!=0 or az!=0: 
			norm=math.sqrt(ax*ax+ay*ay+az*az);
		SinX=ax/norm; #在这里，已经成为sin值
		SinY=ay/norm;
		SinZ=az/norm;
		#垂直于磁通量的重力和矢量的估计方向
		HalfVx=2* (self.q1*self.q3-self.q0*self.q2 );
		HalfVy=2* (self.q0*self.q1+self.q2*self.q3 );
		HalfVz=self.q0*self.q0-self.q1*self.q1-self.q2*self.q2+self.q3*self.q3;
		#误差是估计重力方向和测量重力方向叉乘的和
		HalfEX= (SinY*HalfVz-SinZ*HalfVy );
		HalfEY= (SinZ*HalfVx-SinX*HalfVz );
		HalfEZ= (SinX*HalfVy-SinY*HalfVx );
		#积分误差比例积分增益
		self.HalfEXInt=self.HalfEXInt+HalfEX*self.Ki;
		self.HalfEYInt=self.HalfEYInt+HalfEY*self.Ki;
		self.HalfEZInt=self.HalfEZInt+HalfEZ*self.Ki;
		#调整后的陀螺仪测量
		gx=gx+self.Kp*HalfEX+self.HalfEXInt;
		gy=gy+self.Kp*HalfEY+self.HalfEYInt;
		gz=gz+self.Kp*HalfEZ+self.HalfEZInt;
		#整合四元数率和正常化
		self.q0=self.q0+ (-self.q1*gx-self.q2*gy-self.q3*gz )*self.halfT;
		self.q1=self.q1+ (self.q0*gx+self.q2*gz-self.q3*gy )*self.halfT;
		self.q2=self.q2+ (self.q0*gy-self.q1*gz+self.q3*gx )*self.halfT;
		self.q3=self.q3+ (self.q0*gz+self.q1*gy-self.q2*gx )*self.halfT;
		#正常化四元
		norm=math.sqrt(self.q0*self.q0+self.q1*self.q1+self.q2*self.q2+self.q3*self.q3 );
		self.q0=self.q0/norm;
		self.q1=self.q1/norm;
		self.q2=self.q2/norm;
		self.q3=self.q3/norm;
		Pitch = math.asin(-2*self.q1*self.q3+2*self.q0*self.q2);
		if 1-2*self.q1*self.q1-2*self.q2*self.q2!=0:
			Roll=math.atan2 ((2*self.q2*self.q3+2*self.q0*self.q1),(1-2*self.q1*self.q1-2*self.q2*self.q2)) #rollv
		if 1-2*self.q2*self.q2-2*self.q3*self.q3!=0:
			yaw = math.atan2 ((2*self.q1*self.q2+2*self.q0*self.q3), (1-2*self.q2*self.q2-2*self.q3*self.q3))
		a[0]=Pitch*57.29578#弧度转换为角度
		a[1]=Roll*57.29578
		a[2]=yaw*57.29578
		a[4]=gx
		a[5]=gy
		a[6]=gz
		return a
