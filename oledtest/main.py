from simple import MQTTClient
from machine import Pin,I2C,Timer
import network 
import ssd1306
import utime
import dht
import urequests

i2c = I2C(scl=Pin(4), sda=Pin(5), freq=100000)
lcd=ssd1306.SSD1306_I2C(128,64,i2c)

def lcd_test():
    lcd.text("HAHAH",0,0)
    for i in range(0,28):
      lcd.pixel(2*i,10,1)

    lcd.line(0,12,54,12,1)              #draw a line from (0,12) to (54,12) color is blue
    lcd.hline(10,32,108,1)              #draw a horizontal line,from (10,32),length 108,color is blue
    lcd.vline(64,0,64,1)                #draw a vertical line,from (64,0),length 64,color is blue
    lcd.fill_rect(59,27,10,10,1)        #draw a rectangle,from (59,27) to (10,10) fill with blue
    lcd.rect(56,24,16,16,1)             #draw a rectangle frame,from (59,27) to (10,10) and color is blue
    lcd.fill_rect(59,27,10,10,1)
    lcd.fill_rect(88,0,40,20,1)
    lcd.line(88,0,128,20,0)             #draw a line from (88,0) to (128,20) color is black
    lcd.line(88,20,128,0,0)
    lcd.show()                          #display pix

def lcd_print(str,x=0,cnt=128,line=0):
    lcd.fill_rect(x, line*8, cnt, 8, col=0)
    lcd.text(str,x,line*8)
    lcd.show()


#配置版本号
VERSION = "V0.1.1"

#输出gpio配置
D0=Pin(16,Pin.OUT)

#命令状态配置
statuson = "ON"
statusoff = "OFF"
statusunkown = "UNKOWN"
command_topic={
"home/bedroom/switch0/set":{"state_topic":"home/bedroom/switch0/state","newstate":statusoff,"oldstate":statusunkown,"switch":D0,"index":0},
"home/bedroom/switch1/set":{"state_topic":"home/bedroom/switch1/state","newstate":statusoff,"oldstate":statusunkown,"switch":D0,"index":1},
"home/bedroom/switch2/set":{"state_topic":"home/bedroom/switch2/state","newstate":statusoff,"oldstate":statusunkown,"switch":D0,"index":2},
"home/bedroom/switch3/set":{"state_topic":"home/bedroom/switch3/state","newstate":statusoff,"oldstate":statusunkown,"switch":D0,"index":3},
"home/bedroom/switch4/set":{"state_topic":"home/bedroom/switch4/state","newstate":statusoff,"oldstate":statusunkown,"switch":D0,"index":4},
"home/bedroom/switch5/set":{"state_topic":"home/bedroom/switch5/state","newstate":statusoff,"oldstate":statusunkown,"switch":D0,"index":5},
"home/bedroom/switch6/set":{"state_topic":"home/bedroom/switch6/state","newstate":statusoff,"oldstate":statusunkown,"switch":D0,"index":6},
"home/bedroom/switch7/set":{"state_topic":"home/bedroom/switch7/state","newstate":statusoff,"oldstate":statusunkown,"switch":D0,"index":7}
}

#连接标志
mqtt_commect_status=False
net_commect_status=False
ping_flag=False

def connectflag(cmd):
    global net_commect_status
    global mqtt_commect_status
    global ping_flag

    if "netoff" in cmd:
        ping_flag=False
        net_commect_status=False
        mqtt_commect_status=False
        lcd_print("WIFI OFF",cnt=64,line=0)

    elif "mqttoff" in cmd:
        ping_flag=False
        mqtt_commect_status=False
        lcd_print("MQTT OFF",x=64,cnt=64,line=0)
        
    elif "neton" in cmd:
        net_commect_status=True
        lcd_print("WIFI ON",cnt=64,line=0)

    elif "mqtton" in cmd:
        ping_flag=False
        mqtt_commect_status=True
        lcd_print("MQTT ON",x=64,cnt=64,line=0)

    elif "getflag" in cmd:
        if net_commect_status and mqtt_commect_status:
            return True
        else:
            return False


#wifi配置
NETID = "smarthome"
NETPWD = "guojing123,./"
sta_if = network.WLAN(network.STA_IF)
#net连接
def netconnect(netid,netpwd):
    global mqtt_retry_cnt
    if(True == sta_if.isconnected()):
        sta_if.disconnect()
        utime.sleep(1)
    while(True != sta_if.isconnected()):
        print ("try netconnect")
        sta_if.active(True) 
        sta_if.scan()
        sta_if.connect(netid, netpwd)
        utime.sleep(3)
    print ("netconnect ok")
    utime.sleep(3)
    connectflag("neton")

#mqtt配置
USER = "xensyz"
PWD = "TEST"
MQTTHOST = "www.eniac.shop"
MQTTPORT = 11883
mqttClient = MQTTClient("switchserver0123", MQTTHOST, MQTTPORT, USER, PWD)
mqtt_retry_cnt=0
mqtt_retry_maxcnt=10
ping_resp=b"\xd0"
#mqtt连接
def mqtt_connect():
    global mqtt_retry_cnt
    try:
        mqttClient.disconnect()
    except OSError:
        pass
    while(True == sta_if.isconnected()):
        if (mqtt_retry_cnt>mqtt_retry_maxcnt):
            connectflag("netoff")
            mqtt_retry_cnt=0
            return
        mqtt_retry_cnt = mqtt_retry_cnt+1
        try:
            print ("try mqtt_connect")
            mqttClient.connect()
        except OSError:
            print ("mqtt_connect OSError!")
            utime.sleep(0.5)
            continue
        except IndexError:
            print ("mqtt_connect IndexError!")
            utime.sleep(0.5)
            continue
        else:
            connectflag("mqtton")
            print ("mqtt_connect ready")
            mqttClient.set_callback(message_come)
            subscribe_settopic()
            break

#发布主题
def publish(topic, payload, qos):
    try:
        mqttClient.publish(topic, payload, qos=qos)
        print("publish to " + topic[17:] + ": " + payload + " qos:" + str(qos))
    except OSError:
        connectflag("mqttoff")
        print ("when publish OSError!")
    except AttributeError:
        connectflag("mqttoff")
        print ("when publish AttributeError!")

#订阅主题
def subscribe (topic, qos):
    try:
        mqttClient.subscribe(topic,qos)
        print("subscribe " + topic[17:] + "qos:" + str(qos))
    except OSError:
        connectflag("mqttoff")
        print ("when subscribe OSError!")

#回调函数
def message_come(topic, msg):
    print("log:"+topic.decode("utf8")[17:]+" "+msg.decode("utf8"))
    if topic.decode("utf8") in command_topic.keys():
        if "ON" in msg.decode("utf8"):
            Newstatus="ON"
            Oldstatus="OFF"
        elif "OFF" in msg.decode("utf8"):
            Newstatus="OFF"
            Oldstatus="ON"
        command_topic[topic.decode("utf8")]["newstate"]=Newstatus
        command_topic[topic.decode("utf8")]["oldstate"]=Oldstatus

#轮询订阅
def check_msg():
    global ping_resp
    try:
        result = mqttClient.check_msg()
        if result == ping_resp:
            global ping_flag
            ping_flag=False
            print("ping ack")
    except OSError:
        connectflag("mqttoff")
        print ("when check_msg OSError!")

#订阅相关主题
def subscribe_settopic():
    for key in command_topic.keys():
        subscribe(key,0)

#wifi/mqtt连接
def connect():
    global mqtt_commect_status
    global net_commect_status
    if not net_commect_status:
        try:
            netconnect(NETID,NETPWD)
        except RuntimeError:
            print("when netconnect RuntimeError")
    if not mqtt_commect_status:
        mqtt_connect()

#ping参数
#mqtt ping发送 20S
def mqttping():
    global ping_flag
    if ping_flag:
        print ("ping time out try reconnect!")
        connectflag("mqttoff")
    else:
        try:
            mqttClient.ping()
            print("ping mqtt server")
            ping_flag=True
        except OSError:
            print("when ping OSError")
            connectflag("mqttoff")
            ping_flag=False
#发送状态
def statereply():
    if not mqtt_commect_status:
        return
    for key in command_topic.keys():
        if command_topic[key]["newstate"] not in command_topic[key]["oldstate"]:
            publish(command_topic[key]["state_topic"], command_topic[key]["newstate"], 1)
            command_topic[key]["oldstate"]=command_topic[key]["newstate"]
            #utime.sleep(0.005)

#dht配置
dht_cycle=50
dht11 = dht.DHT11(Pin(25))
temp_index=0


def display_init():
    connect()
    temp_measure()
    get_time()
    time_init(1000)
    lcd_print((str(H//10)+str(H%10)+":"+str(M//10)+str(M%10)+":"+str(S//10)+str(S%10)),line=4)
    lcd_print ("TEMP: " + str(TEMP)+"C",line=1) # eg. 23 (°C)
    lcd_print ("HUM : " + str(HUM )+"%",line=2)    # eg. 41 (% RH)
    lcd_print(DATE,line=5)

#DHT更新
def temp_measure():
    global TEMP
    global HUM
    try:
        dht11.measure()
        TEMP=dht11.temperature()
        HUM=dht11.humidity()
    except OSError:
        print ("when measure OSError!")
    else:
        if connectflag("getflag"):
            publish("home/bedroom/sensor0/state", str(dht11.temperature()), 0)
            publish("home/bedroom/sensor1/state", str(dht11.humidity()), 0)

H=0
M=0
S=0
TEMP=0
HUM=0

DATE="2019-01-01"
def get_time():
    global H
    global M
    global S
    global DATE

    URL="http://quan.suning.com/getSysTime.do"
    try:
        res = urequests.get(URL).text
        H=int(res[24:26])
        M=int(res[27:29])
        S=int(res[30:32])
        DATE=res[13:23]
    except IndexError:
        print("gettime IndexError")
    except OSError:
        print("gettime OSError")

def timing(tim):
    global H
    global M
    global S
    global DATE
    S=S+1
    if S>=60:
        M=M+1
        S=0
    if M>=60:
        H=H+1
        M=0
    if H>=24:
        H=0
    lcd_print((str(H//10)+str(H%10)+":"+str(M//10)+str(M%10)+":"+str(S//10)+str(S%10)),line=4)
    if S==30:
        temp_measure()
        get_time()
        lcd_print ("TEMP: " + str(TEMP)+"C",line=1) # eg. 23 (°C)
        lcd_print ("HUM : " + str(HUM )+"%",line=2)    # eg. 41 (% RH)
        lcd_print(DATE,line=5)
        mqttping()
tm=None
def time_init(ms):
    tm=Timer(4)
    tm.init(period=1000, mode=Timer.PERIODIC, callback=timing)
def main(): 
    print(VERSION)
    lcd_print(VERSION,line=0)
    display_init()
    while True:
        connect()
        if not connectflag("getflag"):
            continue
        if (sta_if.isconnected()):
            check_msg()
        else:
            print ("net disconnect!")
            connectflag("netoff")
        statereply()
        utime.sleep(0.1)
        pass

if __name__ == "__main__":
    main()
    
