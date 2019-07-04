import dht
import machine
from simple import MQTTClient
from machine import Pin
import network 
import time
from lib595 import ShiftRegister

#配置版本号
VERSION = "V0.0.6"

#wifi配置
NETID = "smarthome"
NETPWD = "guojing123,./"
sta_if = network.WLAN(network.STA_IF)

#输出gpio配置
D0=Pin(15,Pin.OUT)
D1=Pin( 5,Pin.OUT)
D2=Pin(10,Pin.OUT)
D3=Pin( 0,Pin.OUT)
D4=Pin( 4,Pin.OUT)
D5=Pin(14,Pin.OUT)
D6=Pin(12,Pin.OUT)
D7=Pin(13,Pin.OUT)

#命令状态配置
statuson = "ON"
statusoff = "OFF"
statusunkown = "UNKOWN"
command_topic={
"home/bedroom/switch0/set":{"state_topic":"home/bedroom/switch0/state","newstate":statusoff,"oldstate":statusunkown,"switch":D0,"index":0},
"home/bedroom/switch1/set":{"state_topic":"home/bedroom/switch1/state","newstate":statusoff,"oldstate":statusunkown,"switch":D1,"index":1},
"home/bedroom/switch2/set":{"state_topic":"home/bedroom/switch2/state","newstate":statusoff,"oldstate":statusunkown,"switch":D2,"index":2},
"home/bedroom/switch3/set":{"state_topic":"home/bedroom/switch3/state","newstate":statusoff,"oldstate":statusunkown,"switch":D3,"index":3},
"home/bedroom/switch4/set":{"state_topic":"home/bedroom/switch4/state","newstate":statusoff,"oldstate":statusunkown,"switch":D4,"index":4},
"home/bedroom/switch5/set":{"state_topic":"home/bedroom/switch5/state","newstate":statusoff,"oldstate":statusunkown,"switch":D5,"index":5},
"home/bedroom/switch6/set":{"state_topic":"home/bedroom/switch6/state","newstate":statusoff,"oldstate":statusunkown,"switch":D6,"index":6},
"home/bedroom/switch7/set":{"state_topic":"home/bedroom/switch7/state","newstate":statusoff,"oldstate":statusunkown,"switch":D7,"index":7}
}

#595配置
hc595 = ShiftRegister(17,18,19)

#mqtt配置
USER = "xensyz"
PWD = "TEST"
MQTTHOST = "www.eniac.shop"
MQTTPORT = 11883
mqttClient = MQTTClient("switchserver0123", MQTTHOST, MQTTPORT, USER, PWD)
mqtt_retry_cnt=0
mqtt_retry_maxcnt=10
ping_resp=b"\xd0"

#ping参数
ping_index=0
ping_cycle=200
ping_flag=0

#dht配置
dht_cycle=200
dht11 = dht.DHT11(machine.Pin(4))
temp_index=0

#连接标志
mqtt_commect_status=statusoff
net_commect_status=statusoff

#状态灯
run_cyc=10
run_index=0
def runstatus():
    global run_index
    run_index = run_index +1
    if run_index<run_cyc:
        Pin(2, Pin.OUT, value=1)
    elif run_index<(run_cyc*2):
        Pin(2, Pin.OUT, value=0)
    else:
        run_index=0
        
#置位retry
def mqtt_need_retry():
    global ping_flag
    global mqtt_commect_status
    ping_flag=statusoff
    mqtt_commect_status=statusoff
def net_need_retry():
    global net_commect_status
    net_commect_status=statusoff
    mqtt_need_retry()

#置位OK
def net_set_flag():
    global net_commect_status
    net_commect_status=statuson
def mqtt_set_flag():
    global ping_flag
    global mqtt_commect_status
    ping_flag=statusoff
    mqtt_commect_status=statuson

#check状态
def net_mqtt_state_check():
    global net_commect_status
    global mqtt_commect_status
    if statusoff in net_commect_status:
        return statusoff
    if statusoff in mqtt_commect_status:
        return statusoff
    return statuson

def mqttping():
    global ping_index
    global ping_flag
    ping_index = ping_index + 1
    if ping_index>ping_cycle:
        ping_index = 0
        if ping_flag in statuson:
            print ("ping time out try reconnect!")
            ping_flag=statusoff
            mqtt_need_retry()
        else:
            try:
                mqttClient.ping()
                print("ping mqtt server")
                ping_flag=statuson
            except OSError:
                print("when ping OSError")
                mqtt_need_retry()
                ping_flag=statusoff

def temp_measure():
    global temp_index
    temp_index = temp_index + 1
    if temp_index>dht_cycle:
        temp_index = 0
        try:
            dht11.measure()
            print ("the temperature is: " + str(dht11.temperature())) # eg. 23 (°C)
            print ("the humidity is: " + str(dht11.humidity()))    # eg. 41 (% RH)
        except OSError:
            print ("when measure OSError!")
        else:
            publish("home/bedroom/sensor0/state", str(dht11.temperature()), 0)
            publish("home/bedroom/sensor1/state", str(dht11.humidity()), 0)

def netinit():
    sta_if.active(True) 
    sta_if.scan()
def netconnect(netid,netpwd):
    global mqtt_retry_cnt
    if(True == sta_if.isconnected()):
        sta_if.disconnect()
        time.sleep(1)
    while(True != sta_if.isconnected()):
        print ("try netconnect")
        netinit()
        sta_if.connect(netid, netpwd)
        time.sleep(1)
    print ("netconnect ok")
    time.sleep(3)
    net_set_flag()

def mqtt_connect():
    global mqtt_retry_cnt
    try:
        mqttClient.disconnect()
    except OSError:
        pass
    while(True == sta_if.isconnected()):
        if (mqtt_retry_cnt>mqtt_retry_maxcnt):
            net_need_retry()
            mqtt_retry_cnt=0
            return
        mqtt_retry_cnt = mqtt_retry_cnt+1
        try:
            print ("try mqtt_connect")
            mqttClient.connect()
        except OSError:
            print ("mqtt_connect OSError!")
            time.sleep(0.5)
            continue
        except IndexError:
            print ("mqtt_connect IndexError!")
            time.sleep(0.5)
            continue
        else:
            mqtt_set_flag()
            print ("mqtt_connect ready")
            mqttClient.set_callback(message_come)
            subscribe_settopic()
            break

def publish(topic, payload, qos):
    try:
        mqttClient.publish(topic, payload, qos=qos)
        #print("publish to " + topic + ": " + payload + " qos:" + str(qos))
    except OSError:
        mqtt_need_retry()
        print ("when publish OSError!")

def subscribe (topic, qos):
    try:
        mqttClient.subscribe(topic,qos)
        #print("subscribe " + topic + "qos:" + str(qos))
    except OSError:
        mqtt_need_retry()
        print ("when subscribe OSError!")

def message_come(topic, msg):
    print(topic.decode("utf8"),msg.decode("utf8"))
    if topic.decode("utf8") in command_topic.keys():
        if "ON" in msg.decode("utf8"):
            command_topic[topic.decode("utf8")]["newstate"]="ON"
            command_topic[topic.decode("utf8")]["oldstate"]="OFF"
        elif "OFF" in msg.decode("utf8"):
            command_topic[topic.decode("utf8")]["newstate"]="OFF"
            command_topic[topic.decode("utf8")]["oldstate"]="ON"

def check_msg():
    global ping_resp
    try:
        result = mqttClient.check_msg()
        if result == ping_resp:
            global ping_flag
            ping_flag=statusoff
            print("ping ack")
    except OSError:
        mqtt_need_retry()
        print ("when check_msg OSError!")

def subscribe_settopic():
    for key in command_topic.keys():
        subscribe(key,0)

def connect():
    global mqtt_commect_status
    global net_commect_status
    if statusoff in net_commect_status:
        try:
            netconnect(NETID,NETPWD)
        except RuntimeError:
            print("when netconnect RuntimeError")
    if statusoff in mqtt_commect_status:
        mqtt_connect()


def statereply():
    refresh595=0
    if statusoff in mqtt_commect_status:
        return
    for key in command_topic.keys():
        if command_topic[key]["newstate"] not in command_topic[key]["oldstate"]:
            refresh595=1
            publish(command_topic[key]["state_topic"], command_topic[key]["newstate"], 1)
            if "ON" in command_topic[key]["newstate"]:
                hc595.setOutput(command_topic[key]["index"],1)
            else:
                hc595.setOutput(command_topic[key]["index"],0)
            command_topic[key]["oldstate"]=command_topic[key]["newstate"]
            time.sleep(0.1)
    if refresh595:
        hc595.latch()

def main(): 
    print(VERSION)
    while True:
        connect()
        if statusoff in net_mqtt_state_check():
            continue
        if (True == sta_if.isconnected()):
            check_msg()
        else:
            print ("net disconnect!")
            net_need_retry()
        mqttping()
        statereply()
        temp_measure()
        runstatus()
        time.sleep(0.1)
        pass
if __name__ == "__main__":
    main()
