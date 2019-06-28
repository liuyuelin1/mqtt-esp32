import dht
import machine
from simple import MQTTClient
from machine import Pin
import network 
import time
import _thread


#配置版本号
VERSION = "V0.0.5"

#wifi配置
NETID = "smarthome"
NETPWD = "guojing123,./"
sta_if = network.WLAN(network.STA_IF)

#输出gpio配置
D0=Pin(15,Pin.OUT)
D1=Pin( 5,Pin.OUT)
D2=Pin(10,Pin.OUT)
D3=Pin( 0,Pin.OUT)
D4=Pin( 2,Pin.OUT)
D5=Pin(14,Pin.OUT)
D6=Pin(12,Pin.OUT)
D7=Pin(13,Pin.OUT)

#命令状态配置
statuson = "ON"
statusoff = "OFF"
statusunkown = "UNKOWN"
command_topic={
"home/bedroom/switch0/set":{"state_topic":"home/bedroom/switch0/state","newstate":statusoff,"oldstate":statusunkown,"switch":D0},
"home/bedroom/switch1/set":{"state_topic":"home/bedroom/switch1/state","newstate":statusoff,"oldstate":statusunkown,"switch":D1},
"home/bedroom/switch2/set":{"state_topic":"home/bedroom/switch2/state","newstate":statusoff,"oldstate":statusunkown,"switch":D2},
"home/bedroom/switch3/set":{"state_topic":"home/bedroom/switch3/state","newstate":statusoff,"oldstate":statusunkown,"switch":D3},
"home/bedroom/switch4/set":{"state_topic":"home/bedroom/switch4/state","newstate":statusoff,"oldstate":statusunkown,"switch":D4},
"home/bedroom/switch5/set":{"state_topic":"home/bedroom/switch5/state","newstate":statusoff,"oldstate":statusunkown,"switch":D5},
"home/bedroom/switch6/set":{"state_topic":"home/bedroom/switch6/state","newstate":statusoff,"oldstate":statusunkown,"switch":D6},
"home/bedroom/switch7/set":{"state_topic":"home/bedroom/switch7/state","newstate":statusoff,"oldstate":statusunkown,"switch":D7}
}

#mqtt配置
USER = "TEST"
PWD = "TEST"
MQTTHOST = "www.eniac.shop"
MQTTPORT = 11883
mqttClient = MQTTClient("switchserver0", MQTTHOST, MQTTPORT, USER, PWD)
ping_index=0
ping_cycle=100
ping_flag=0

#heartbeat 配置
heartbeat_index=0
heartbeat_cycle=1200

#dht配置
dht_cycle=200
dht11 = dht.DHT11(machine.Pin(4))
temp_index=0

#连接标志
mqtt_connect_flag=0
net_connect_flag=0


def mqttping():
    global ping_index
    global ping_flag
    ping_index = ping_index + 1
    if ping_index>ping_cycle:
        ping_index = 0
        if ping_flag:
            print ("ping time out try reconnect!")
            ping_flag=0
            global mqtt_connect_flag
            mqtt_connect_flag=1
        try:
            mqttClient.ping()
            #print("ping mqtt server")
            ping_flag=1
        except OSError:
            print("ping faild")
            global mqtt_connect_flag
            mqtt_connect_flag=1

def temp_measure():
    global temp_index
    temp_index = temp_index + 1
    if temp_index>dht_cycle:
        temp_index = 0
        try:
            dht11.measure()
            print ("the temperature is: " + str(dht11.temperature())) # eg. 23 (°C)
            print ("the humidity is: " + str(dht11.humidity()))    # eg. 41 (% RH)
            on_publish("home/bedroom/sensor0/state", str(dht11.temperature()), 0)
            on_publish("home/bedroom/sensor1/state", str(dht11.humidity()), 0)
        except OSError:
            print ("DHT11 is not ready!")

def netinit():
    sta_if.active(True) 
    sta_if.scan()
def netconnect(netid,netpwd):
    if(True == sta_if.isconnected()):
        sta_if.disconnect()
    while(True != sta_if.isconnected()):
        print ("try netconnect")
        netinit()
        sta_if.connect(netid, netpwd)
        for num in range(1,10):
            time.sleep(1)
            if(True == sta_if.isconnected()):
                print ("netconnect ready")
                global net_connect_flag
                net_connect_flag=1
                break
def on_mqtt_connect():
    while(True == sta_if.isconnected()):
        try:
            try:
                print ("try mqtt_connect")
                mqttClient.connect()
            except IndexError:
                continue
            global mqtt_connect_flag
            mqtt_connect_flag=1
            print ("mqtt_connect ready")
            break
        except OSError:
            print ("MQTT disconnect!")
            time.sleep(1)
    mqttClient.set_callback(on_message_come)

def on_publish(topic, payload, qos):
    try:
        mqttClient.publish(topic, payload, qos=qos)
        #print("publish to " + topic + ": " + payload + " qos:" + str(qos))
    except OSError:
        global mqtt_connect_flag
        mqtt_connect_flag=0
        print ("when on_publish MQTT disconnect!")

def on_subscribe (topic, qos):
    try:
        mqttClient.subscribe(topic,qos)
        #print("subscribe " + topic + "qos:" + str(qos))
    except OSError:
        global mqtt_connect_flag
        mqtt_connect_flag=0
        print ("when sub topic MQTT disconnect!")

def on_message_come(topic, msg):
    print(topic.decode("utf8"),msg.decode("utf8"))
    if topic.decode("utf8") in command_topic.keys():
        if "ON" in msg.decode("utf8"):
            command_topic[topic.decode("utf8")]["newstate"]="ON"
            command_topic[topic.decode("utf8")]["oldstate"]="OFF"
        elif "OFF" in msg.decode("utf8"):
            command_topic[topic.decode("utf8")]["newstate"]="OFF"
            command_topic[topic.decode("utf8")]["oldstate"]="ON"

def check_msg():
    try:
        result = mqttClient.check_msg()
        if result == 0x30:
            global ping_flag
            ping_flag=0
    except OSError:
        global mqtt_connect_flag
        mqtt_connect_flag=0
        print ("when check_msg mqtt disconnect!")

def subscribe_settopic():
    for key in command_topic.keys():
        on_subscribe(key,0)

def mqtt_connect():
    on_mqtt_connect()
    subscribe_settopic()

def connect():
    global mqtt_connect_flag
    global net_connect_flag
    if 0==net_connect_flag:
        try:
            netconnect(NETID,NETPWD)
        except RuntimeError:
            print("netconnect faild")
    if 0==mqtt_connect_flag:
        try:
            mqtt_connect()
        except OSError:
            print("mqtt_connect faild")

def statereply():
    for key in command_topic.keys():
        if command_topic[key]["newstate"] not in command_topic[key]["oldstate"]:
            on_publish(command_topic[key]["state_topic"], command_topic[key]["newstate"], 1)
            if "ON" in command_topic[key]["newstate"]:
                command_topic[key]["switch"].on()
            else:
                command_topic[key]["switch"].off()
            command_topic[key]["oldstate"]=command_topic[key]["newstate"]
            time.sleep(0.1)

def main(): 
    print(VERSION)
    #_thread.start_new_thread(sem_thread, ())
    while True:
        connect()
        if (True == sta_if.isconnected()):
            check_msg()
        else:
            print ("net disconnect!")
            global net_connect_flag
            net_connect_flag=0
        mqttping()
        statereply()
        temp_measure()
        time.sleep(0.1)
        pass
if __name__ == "__main__":
    main()
