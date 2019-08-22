import network
ap = network.WLAN(network.AP_IF) # 创建一个AP
ap.active(True)         # 激活这个AP
ap.config(essid='好的') # 设置这个AP的SSID

def main():
    while True:
        pass

if __name__ == "__main__":
    main()