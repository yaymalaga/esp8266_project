//Include

#include <Arduino_JSON.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

// Parámetros de la conexión

const char* ssid = "infind";
const char* password = "1518wifi";
const char* mqtt_server = "172.16.53.131";
String CHIP_ID = "mejorArduino";

// Struct types

typedef struct {
  long uptime;
  String chip_id;
} t_ESP8266Data;

typedef struct {
  float deep_sleep;
} t_ESP8266Conf;

typedef struct {
  t_ESP8266Data data;
  t_ESP8266Conf conf;
} t_ESP8266;

typedef struct {
  int32_t ap;
  String bssid; //MAC
  String ip; 
  int32_t rssi;
} t_WifiModuleData;

typedef struct  {
  String ssid;
  String password;
  String mqtt_server;
} t_WifiModuleConf;

typedef struct {
  t_WifiModuleData data;
  t_WifiModuleConf conf;
} t_WifiModule;

typedef struct {
  t_ESP8266 Board;
  t_WifiModule WifiModule;
} t_Device;

typedef struct {
  int light;
} t_LightmeterData;

typedef struct {
  float temperature;
  float humidity;
} t_DH11Data;

typedef struct {
  t_DH11Data DH11;
  t_LightmeterData LightSensor;
} t_Sensor;

//Initialize conection objects

WiFiClient espClient;
PubSubClient client(espClient);

//Initialize sensor objects

DHTesp dht;

//Initialize global variables

char msg[500];
String json = "";
// Variable TOOOWAPA

const String DEVICE_ID = "foo";
const float DEEP_SLEEP = 10;

//Declare serialize functions

String device_serialize_JSON(t_Device data)
{
  JSONVar jsonRoot;
  JSONVar device;
  String jsonString;
  
  device["Board"]["data"]["uptime"] = data.Board.data.uptime;
  device["Board"]["data"]["chip_id"] = data.Board.data.chip_id;
  
  device["Board"]["conf"]["deep_sleep"] = data.Board.conf.deep_sleep;
  
  device["WifiModule"]["data"]["ap"] = data.WifiModule.data.ap;
  device["WifiModule"]["data"]["bssid"] = data.WifiModule.data.bssid;
  device["WifiModule"]["data"]["ip"] = data.WifiModule.data.ip;
  device["WifiModule"]["data"]["rssi"] = data.WifiModule.data.rssi;
  
  device["WifiModule"]["conf"]["ssid"] = data.WifiModule.conf.ssid;
  device["WifiModule"]["conf"]["password"] = data.WifiModule.conf.password;
  device["WifiModule"]["conf"]["mqqt_server"] = data.WifiModule.conf.mqtt_server;
  
  jsonRoot["data"]= device;
  
  return JSON.stringify(jsonRoot);
}


String sensor_serialize_JSON(t_Sensor data)
{
  JSONVar jsonRoot;
  JSONVar sensors;
  String jsonString;

  sensors["DH11"]["temperature"] = data.DH11.temperature;
  sensors["DH11"]["humidity"] = data.DH11.humidity;
  
  sensors["LightSensor"]["light"]= data.LightSensor.light;

  jsonRoot["data"]= sensors;
  
  return JSON.stringify(jsonRoot);
}

t_Device get_device_data(){
  //Gets the information from the device and returns a struct with this data.
  
  t_Device Device;
  
  //Get Board data
  Device.Board.data.uptime = millis();
  Device.Board.data.chip_id = CHIP_ID;
  
  //Get Board conf
  Device.Board.conf.deep_sleep = DEEP_SLEEP;

  //Get WifiModule data
  Device.WifiModule.data.ap = 1;
  Device.WifiModule.data.bssid = WiFi.BSSIDstr();
  Device.WifiModule.data.ip = WiFi.localIP().toString();
  Device.WifiModule.data.rssi =  WiFi.RSSI();
  
  //Get WifiModule conf
  Device.WifiModule.conf.ssid = WiFi.SSID();
  Device.WifiModule.conf.mqtt_server = mqtt_server;

  return Device;
}

t_Sensor get_sensor_data(){
  //Gets the information from the sensors and returns a struct with this data.
  
  t_Sensor Sensor;

  //Get DH11 data
  Sensor.DH11.temperature = dht.getTemperature();
  Sensor.DH11.humidity = dht.getHumidity();

  //Get LightSensor data
  Sensor.LightSensor.light = analogRead(A0);

  return Sensor;
}
void setup() {
  Serial.begin(115200);
}

void loop() {
  //Declare variables to hold the data values
  t_Device Device;
  t_Sensor Sensor;
  String json, topic;
  //Get device data values 
  Device = get_device_data();

  //Serialize and publish device data
  json = device_serialize_JSON(Device);

  //Obtain topic name adding the chip ID
  topic = String("GRUPOG/" + CHIP_ID + "/device");

  client.publish(topic.c_str(), json.c_str());
  Serial.print("Publish message: ");
  Serial.println(json.c_str());
  
  //Get sensor data values
  Sensor = get_sensor_data();

  //Serialize and publish sensor data
  json = sensor_serialize_JSON(Sensor);
  
  //Obtain topic name adding the chip ID
  topic = String("GRUPOG/" + CHIP_ID + "/sensor");
  
  client.publish(topic.c_str(), json.c_str());
  Serial.print("Publish message: ");
  Serial.println(json.c_str());
}
