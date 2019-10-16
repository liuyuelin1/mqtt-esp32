'''
ESP32串口通信-字符串数据自发实验

接线 将开发板的 13号引脚与12号引脚用杜邦线相连接。

'''
from machine import UART,Pin
import utime

# 初始化一个UART对象
uart = UART(2, baudrate=115200, rx=5,tx=18,timeout=10)

count = 1

while True:
    if uart.any():
        utime.sleep_ms(1)
        bin_data = uart.readline()
        # 将手到的信息打印在终端
        print(bin_data[5])
    utime.sleep_ms(10)