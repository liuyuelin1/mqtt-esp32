# MicroPython SSD1306 OLED driver, I2C and SPI interfaces

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
