#include <ESP8266WiFi.h>
#include <MQTT.h>
#include "DHTesp.h"
#include <ESP8266httpUpdate.h>
#include <Arduino_JSON.h>
#include <NTPClient.h>
#include "time.h"

//Define macros
#define HTTP_OTA_ADDRESS      F("172.16.53.132")       //TODO arrange this as configurable info //Address of OTA update server
#define HTTP_OTA_PATH         F("/esp8266-ota/update") // Path to update firmware
#define HTTP_OTA_PORT         1880                     // Port of update server
                                                       // Name of firmware
#define HTTP_OTA_VERSION      String(__FILE__).substring(String(__FILE__).lastIndexOf('\\')+1) + ".nodemcu"

// Connection parameters
const char* ssid = "infind";
const char* password = "1518wifi";
const char* mqtt_server = "172.16.53.131";
const char* ntpServer = "cronos.uma.es";
const long utcOffsetInSeconds = 3600;
const int  daylightOffsetInSeconds = 3600;
String CHIP_ID = "BEST_Arduino"; //TODO: Get real chipID

// Struct types
typedef struct {
  long uptime;
  String chip_id;
} t_ESP8266Data;

typedef struct {
  float deep_sleep_time;
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

// General objects
WiFiClient espClient;
MQTTClient client(1024);
DHTesp dht;
struct tm * timeinfo;

// General variables
float deep_sleep_time = 10;
bool deep_sleep = false;
long nowTime, lastTime = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(ssid);

  WiFi.begin(ssid, password);

  int timeout = 0; // TODO: Check if 5s is enough
  while (WiFi.status() != WL_CONNECTED && timeout < 5000) {
    timeout += 500;
    delay(500);
    Serial.print(".");
  }

  if (timeout == 5000) {
    do_deep_sleep();
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//Declare serialize functions
String device_serialize_JSON(t_Device &body)
{
  JSONVar jsonRoot;
  JSONVar device;
  String jsonString;
  
  device["Board"]["data"]["uptime"] = body.Board.data.uptime;
  device["Board"]["data"]["chip_id"] = body.Board.data.chip_id;
  
  device["Board"]["conf"]["deep_sleep_time"] = body.Board.conf.deep_sleep_time;
  
  device["WifiModule"]["data"]["ap"] = body.WifiModule.data.ap;
  device["WifiModule"]["data"]["bssid"] = body.WifiModule.data.bssid;
  device["WifiModule"]["data"]["ip"] = body.WifiModule.data.ip;
  device["WifiModule"]["data"]["rssi"] = body.WifiModule.data.rssi;
  
  device["WifiModule"]["conf"]["ssid"] = body.WifiModule.conf.ssid;
  device["WifiModule"]["conf"]["password"] = body.WifiModule.conf.password;
  device["WifiModule"]["conf"]["mqtt_server"] = body.WifiModule.conf.mqtt_server;
  
  jsonRoot["body"]= device;
  
  return JSON.stringify(jsonRoot);
}


String sensor_serialize_JSON(t_Sensor &body)
{
  JSONVar jsonRoot;
  JSONVar sensors;
  String jsonString;

  sensors["DH11"]["temperature"] = body.DH11.temperature;
  sensors["DH11"]["humidity"] = body.DH11.humidity;
  
  sensors["LightSensor"]["light"]= body.LightSensor.light;

  jsonRoot["body"]= sensors;
  
  return JSON.stringify(jsonRoot);
}

//Declare get data functions
t_Device get_device_data(){
  //Gets the information from the device and returns a struct with this data.
  
  t_Device Device;
  
  //Get Board data
  Device.Board.data.uptime = millis();
  Device.Board.data.chip_id = CHIP_ID;
  
  //Get Board conf
  Device.Board.conf.deep_sleep_time = deep_sleep_time;

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

void do_deep_sleep() {
  ESP.deepSleep(deep_sleep_time*1000000);
  delay(5000);
}

void reconnect() {
  int tries = 0;
  while (!client.connected() && tries < 3) {
    tries += 1;
    
    Serial.print("Attempting MQTT connection...");

    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    // TODO: Change
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.setWill("GRUPOG/tele/LWT", "GrupoG Offline", true, 1);
      client.publish("GRUPOG/tele/LWT", "GrupoG Online", true, 1);
      client.subscribe("GRUPOG/cmnd/power", 1); 
      client.loop();
      delay(10); // Advised for stability
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.lastError());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

  if (tries == 3) {
    do_deep_sleep();
  }
}

void callback(String &topic, String &payload) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(payload);

  // TODO: Handle topic logic
}

void check_fota() {
  Serial.println( "Preparing to update." );
  
  switch(ESPhttpUpdate.update(HTTP_OTA_ADDRESS, HTTP_OTA_PORT, HTTP_OTA_PATH, HTTP_OTA_VERSION)) {
  case HTTP_UPDATE_FAILED:
    Serial.printf("HTTP update failed: Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
    break;
  case HTTP_UPDATE_NO_UPDATES:
    Serial.println(F("No updates"));
    break;
  case HTTP_UPDATE_OK:
    Serial.println(F("Update OK"));
    break;
  }
}

void check_date_time() {
  configTime(utcOffsetInSeconds, daylightOffsetInSeconds, ntpServer);
  time_t t_now;
  //struct tm * timeinfo;
  //tm_sec  int seconds after the minute  0-60*
  //tm_min  int minutes after the hour  0-59
  //tm_hour int hours since midnight  0-23
  //tm_mday int day of the month  1-31
  //tm_mon  int months since January  0-11
  //tm_year int years since 1900  
  //tm_wday int days since Sunday 0-6
  //tm_yday int days since January 1  0-365
  //tm_isdst  int Daylight Saving Time flag 
  while (!t_now) {
    time(&t_now);
    if (t_now) {
      timeinfo = localtime(&t_now);
      Serial.println(timeinfo->tm_mday);
    } else {
      Serial.println("Failed to obtain time");
    }
  }
}

void setup() {
  Serial.begin(115200);

  Serial.println("Booting");
  
  setup_wifi();

  check_fota();

  client.begin(mqtt_server, espClient);
  client.onMessage(callback);

  check_date_time();
  
  dht.setup(5, DHTesp::DHT11);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(10); // Advised for stability

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

  //Deep-sleep for 3s. Small wait just in case
  lastTime = millis();
  nowTime = lastTime;
  while(nowTime - lastTime < 1000){
    nowTime = millis();
    client.loop();
    delay(10); // Advised for stability
  }

  do_deep_sleep();
}
