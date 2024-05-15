#include <Arduino.h>
#include <I2S.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <EasyLed.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "EspSmartWifi.h"
#include <WiFiUdp.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS D3

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

int count = 0;

/*audio bug added*/
#define OLED_RESET -1  
#define STATUS_LED  2

EasyLed led(STATUS_LED, EasyLed::ActiveLevel::Low, EasyLed::State::Off);  //Use this for an active-low LED
EspSmartWifi wifi(led);


// MQTT服务器信息
const char* mqttServer = "192.168.8.3";
const int mqttPort = 1883;

// MQTT主题
const char* vStatusTopic = "/esptempholder/data";
const char* vControlTopicSetMin = "/esptempholder/set_min";
const char* vControlTopicSetMax = "/esptempholder/set_max";

WiFiClient espClient;
PubSubClient mqtt(espClient);


float min_temperature = 27.0;
float max_temperature = 31.0;

// 回调函数，用于处理接收到的MQTT消息
void callback(char* topic, byte* payload, unsigned int length) {
  // 仅处理与controlTopic匹配的消息
  if (strcmp(topic, vControlTopicSetMin) == 0) {
    String message;
    for (int i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    Serial.print(message.c_str());
    min_temperature = message.toFloat();
    Serial.printf("min_temperature changed to %f\n", min_temperature);
  }
  if (strcmp(topic, vControlTopicSetMax) == 0) {
    String message;
    for (int i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    Serial.print(message.c_str());
    max_temperature = message.toFloat();
    Serial.printf("max_temperature changed to %f\n", max_temperature);
  }
}


void setup() {
  Serial.begin(9600);
  //Serial.setDebugOutput(true);

    // Start up the library
  sensors.begin();

  pinMode(D7, OUTPUT);

  wifi.initFS();
  wifi.ConnectWifi(); //This may loop forever if wifi is not connected
  
  wifi.DisplayIP();
  
  // 设置MQTT服务器和回调函数
  mqtt.setServer(mqttServer, mqttPort);
  mqtt.setBufferSize(2048);
  mqtt.setCallback(callback);

}

void switchHotAir(float temp)
{
  if (temp < min_temperature)
  {
    Serial.print("Hot Air ON\n");
    digitalWrite(D7, LOW);
  }
  if (temp > max_temperature)
  {
    Serial.print("Hot Air OFF\n");
    digitalWrite(D7, HIGH);
  }
}

// ESP8266 芯片ID
String chipId = String(ESP.getChipId(), HEX);
String ClientId = "EspTempHolder" + chipId;
int loop_count = 0;



void loop() {
  wifi.WiFiWatchDog();

  // 如果WiFi和MQTT都连接成功
  if (WiFi.status() == WL_CONNECTED && mqtt.connected()) 
  {
    // 定期处理MQTT消息
    mqtt.loop();
  } 
  else 
  {
    // 如果WiFi或MQTT连接断开，尝试重新连接
    if (!mqtt.connected()) 
    {
      // 尝试连接到MQTT服务器
      if (mqtt.connect(ClientId.c_str())) 
      {
        Serial.println("Connected to MQTT server!");
        // 订阅MQTT主题
        mqtt.subscribe(vControlTopicSetMin);
        mqtt.subscribe(vControlTopicSetMax);
      } 
      else 
      {
        Serial.print("Failed to connect to MQTT server, rc=");
        Serial.println(mqtt.state());
        delay(100);
        //return;
      }
      delay(100);
    }
  }

  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");

  Serial.print("Temperature for the device 1 (index 0) is: ");
  float temp = sensors.getTempCByIndex(0);
  
  Serial.println(temp);
  char sz[64];
  sprintf(sz, "{\"temp\": %f, \"count\": %d}", temp, count);
  mqtt.publish(vStatusTopic, sz);

  switchHotAir(temp);
  count++;
  sensors.setUserDataByIndex(0, count);
  int x = sensors.getUserDataByIndex(0);
  Serial.println(count);
  
  delay(1000);

  
}