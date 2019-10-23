from micropython import const
from mpu6500 import MPU6500
from ak8963 import AK8963
import math
__version__ = "0.2.1"

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

class MPU9250:
    """Class which provides interface to MPU9250 9-axis motion tracking device."""
    def __init__(self, i2c, mpu6500 = None, ak8963 = None):
        self.Kp=100 #比例增益支配率收敛到加速度计/磁强计
        self.Ki=100 #积分增益支配率的陀螺仪偏见的衔接
        self.halfT=0.004 #采样周期的一半
        self.q0=1
        self.q1=0
        self.q2=0
        self.q3=0 #四元数的元素 ,代表估计方向
        self.HalfEXInt=0
        self.HalfEYInt=0
        self.HalfEZInt=0 #按比例缩小积分误差
        self.vals = {}
        self.AX = KalmanFiter(TestPool = 100)#滤波，加速度计，需要稳定性
        self.AY = KalmanFiter(TestPool = 100)
        self.AZ = KalmanFiter(TestPool = 100)
        self.GX = KalmanFiter(PredictPool = 100)#角速度，需要及时
        self.GY = KalmanFiter(PredictPool = 100)
        self.GZ = KalmanFiter(PredictPool = 100)
        self.MX = KalmanFiter(TestPool = 100)#滤波，加速度计，需要稳定性
        self.MY = KalmanFiter(TestPool = 100)
        self.MZ = KalmanFiter(TestPool = 100)
        if mpu6500 is None:
            self.mpu6500 = MPU6500(i2c)
        else:
            self.mpu6500 = mpu6500

        if ak8963 is None:
            self.ak8963 = AK8963(i2c)
        else:
            self.ak8963 = ak8963

    def acceleration(self):
        return self.mpu6500.acceleration()

    def gyro(self):
        return self.mpu6500.gyro()

    def magnetic(self):
        return self.ak8963.magnetic()

    def whoami(self):
        return self.mpu6500.whoami()

    def __enter__(self):
        return self

    def get_values(self):
        a = self.acceleration()
        g = self.gyro()
        m = self.magnetic()
        self.vals["AcX"] = self.AX.GetNewData(a[0])
        self.vals["AcY"] = self.AY.GetNewData(a[1])
        self.vals["AcZ"] = self.AZ.GetNewData(a[2])
        self.vals["GyX"] = self.GX.GetNewData(g[0])
        self.vals["GyY"] = self.GY.GetNewData(g[1])
        self.vals["GyZ"] = self.GZ.GetNewData(g[2])
        self.vals["MaX"] = self.GX.GetNewData(m[0])
        self.vals["MaY"] = self.GY.GetNewData(m[1])
        self.vals["MaZ"] = self.GZ.GetNewData(m[2])
        return self.vals  # returned in range of Int16
        # -32768 to 32767
    def IMUupdate(self):
        #单位统一
        gx = self.vals["GyX"]/65.5*0.0174533#陀螺仪量程 +-500度/S
        gy = self.vals["GyY"]/65.5*0.0174533#转换关系65.5LSB/度
        gz = self.vals["GyZ"]/65.5*0.0174533#每度的弧度值约0.01745
        ax = self.vals["AcX"]#加速度量程 +-4g/S
        ay = self.vals["AcY"]#转换关系8192LSB/g
        az = self.vals["AcZ"]
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
        if -2*self.q1*self.q1-2*self.q2*self.q2+1!=0:
            temp=math.atan2((2*self.q2*self.q3+2*self.q0*self.q1),(-2*self.q1*self.q1-2*self.q2*self.q2+1))
        if math.fabs(temp) <math.pi/2:
            Pitch = math.asin(-2*self.q1*self.q3+2*self.q0*self.q2)
        else:
            Pitch = (math.fabs(self.q0* self.q2 - self.q1 * self.q3)/(self.q0* self.q2 - self.q1 * self.q3))*(math.pi-math.fabs(math.asin(-2*self.q1*self.q3+2*self.q0*self.q2)))
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
    def AHRSupdate(self):
        #单位统一
        gx = self.vals["GyX"]/65.5*0.0174533#陀螺仪量程 +-500度/S
        gy = self.vals["GyY"]/65.5*0.0174533#转换关系65.5LSB/度
        gz = self.vals["GyZ"]/65.5*0.0174533#每度的弧度值约0.01745
        ax = self.vals["AcX"]#加速度量程 +-4g/S
        ay = self.vals["AcY"]#转换关系8192LSB/g
        az = self.vals["AcZ"]
        mx = self.vals["MaX"]#加速度量程 +-4g/S
        my = self.vals["MaY"]#转换关系8192LSB/g
        mz = self.vals["MaZ"]
        a=[0,0,0,0,0,0,0,0]
        norm = 1
        q0q0 = self.q0 * self.q0
        q0q1 = self.q0 * self.q1
        q0q2 = self.q0 * self.q2
        q0q3 = self.q0 * self.q3
        q1q1 = self.q1 * self.q1
        q1q2 = self.q1 * self.q2
        q1q3 = self.q1 * self.q3
        q2q2 = self.q2 * self.q2
        q2q3 = self.q2 * self.q3
        q3q3 = self.q3 * self.q3
        if ax!=0 or ay!=0 or az!=0: 
            norm=math.sqrt(ax*ax+ay*ay+az*az)
        SinXa=ax/norm #在这里，已经成为sin值
        SinYa=ay/norm
        SinZa=az/norm

        norm = 1
        if mx!=0 or my!=0 or mz!=0: 
            norm=math.sqrt(mx*mx+my*my+mz*mz)
        else:
            return self.IMUupdate()
        SinXm=mx/norm #在这里，已经成为sin值
        SinYm=my/norm
        SinZm=mz/norm

        # Reference direction of Earth's magnetic field
        hx = 2 * (SinXm * (0.5 - q2q2 - q3q3) + SinXm * (q1q2 - q0q3) + SinZm * (q1q3 + q0q2))
        hy = 2 * (SinXm * (q1q2 + q0q3) + SinXm * (0.5 - q1q1 - q3q3) + SinZm * (q2q3 - q0q1))
        bx = math.sqrt(hx * hx + hy * hy)
        bz = 2 * (SinXm * (q1q3 - q0q2) + SinXm * (q2q3 + q0q1) + SinZm * (0.5 - q1q1 - q2q2))

        #垂直于磁通量的重力和矢量的估计方向
        HalfVx=2* (q1q3-q0q2 )
        HalfVy=2* (q0q1+q2q3 )
        HalfVz=q0q0-q1q1-q2q2+q3q3
        
        halfWx = bx * (0.5 - q2q2 - q3q3) + bz * (q1q3 - q0q2)
        halfWy = bx * (q1q2 - q0q3) + bz * (q0q1 + q2q3)
        halfWz = bx * (q0q2 + q1q3) + bz * (0.5 - q1q1 - q2q2)

        #误差是估计重力方向和测量重力方向叉乘的和
        HalfEX= (SinYa*HalfVz-SinZa*HalfVy ) + (SinYm * halfWz - SinZm * halfWy)
        HalfEY= (SinZa*HalfVx-SinXa*HalfVz ) + (SinZm * halfWx - SinXm * halfWz)
        HalfEZ= (SinXa*HalfVy-SinYa*HalfVx ) + (SinXm * halfWy - SinYm * halfWx)
        #积分误差比例积分增益
        self.HalfEXInt=self.HalfEXInt+HalfEX*self.Ki
        self.HalfEYInt=self.HalfEYInt+HalfEY*self.Ki
        self.HalfEZInt=self.HalfEZInt+HalfEZ*self.Ki
        #调整后的陀螺仪测量
        gx=gx+self.Kp*HalfEX+self.HalfEXInt
        gy=gy+self.Kp*HalfEY+self.HalfEYInt
        gz=gz+self.Kp*HalfEZ+self.HalfEZInt
        #整合四元数率和正常化
        self.q0=self.q0 + (-self.q1*gx-self.q2*gy-self.q3*gz )*self.halfT
        self.q1=self.q1 + ( self.q0*gx+self.q2*gz-self.q3*gy )*self.halfT
        self.q2=self.q2 + ( self.q0*gy-self.q1*gz+self.q3*gx )*self.halfT
        self.q3=self.q3 + ( self.q0*gz+self.q1*gy-self.q2*gx )*self.halfT
        #正常化四元
        norm=math.sqrt(self.q0*self.q0+self.q1*self.q1+self.q2*self.q2+self.q3*self.q3 )
        self.q0=self.q0/norm
        self.q1=self.q1/norm
        self.q2=self.q2/norm
        self.q3=self.q3/norm
        
        
        
        if -2*self.q1*self.q1-2*self.q2*self.q2+1!=0:
            temp=math.atan2((2*self.q2*self.q3+2*self.q0*self.q1),(-2*self.q1*self.q1-2*self.q2*self.q2+1))
        if math.fabs(temp) <math.pi/2:
            Pitch = math.asin(-2*self.q1*self.q3+2*self.q0*self.q2)
        else:
            Pitch = (math.fabs(self.q0* self.q2 - self.q1 * self.q3)/(self.q0* self.q2 - self.q1 * self.q3))*(math.pi-math.fabs(math.asin(-2*self.q1*self.q3+2*self.q0*self.q2)))
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

    def __exit__(self, exception_type, exception_value, traceback):
        pass
