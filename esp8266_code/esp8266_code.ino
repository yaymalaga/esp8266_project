#include <ESP8266WiFi.h>
#include <MQTT.h>
#include "DHTesp.h"
#include <ESP8266httpUpdate.h>
#include <ESP_EEPROM.h>
#include <Arduino_JSON.h>
#include "Wire.h"
#include "uRTCLib.h"

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

// General objects
WiFiClient espClient;
MQTTClient client(1024);
DHTesp dht;
uRTCLib rtc(0x68);

// General variables
float deep_sleep_time = 10;
const int utcOffsetInSeconds = 0;
const int  daylightOffsetInSeconds = 3600;
String CHIP_ID = "BEST_Arduino"; //TODO: Get real chipID

// Struct types

typedef struct {
  int   reinicios;
} t_EEPROM;

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

  jsonRoot["header"] =  getTimeStamp();
  
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

  jsonRoot["body"] = sensors;

  jsonRoot["header"] =  getTimeStamp(); 
  
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

void setup_wifi() {
  delay(10);
  Serial.print("\nConnecting to ");
  Serial.print(ssid);

  WiFi.begin(ssid, password);

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 10000) {
    timeout += 500;
    delay(500);
    Serial.print(".");
  }

  if (timeout == 10000) {
    Serial.println("\nWifi connection timeout");
    do_deep_sleep();
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void do_deep_sleep() {
  delay(1000);
  Serial.println("Good night!");
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
      client.setWill(String("GRUPOG/" + CHIP_ID + "/status").c_str(), "GrupoG Offline", true, 1);
      client.publish("GRUPOG/" + CHIP_ID + "/status", "GrupoG Online", true, 1);
      client.subscribe("GRUPOG/" + CHIP_ID + "/deep_sleep", 1);
      client.loop();
      delay(10); // Advised for stability
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.lastError());
      Serial.println(" trying again in 3 seconds");
      delay(5000);
    }
  }

  if (tries == 3) {
    Serial.println("Mqtt connection was not possible");
    do_deep_sleep();
  }
}

void callback(String &topic, String &payload) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(payload);

  // TODO: Handle topic logic
  if (topic == "GRUPOG/" + CHIP_ID + "/deep_sleep") {
    // TODO: Revise in future and save to flash memory
    deep_sleep_time = payload.toFloat();
    Serial.println("DeepSleep set to " + (String
    )deep_sleep_time + " minutes");
  } else if (topic == "GRUPOG/" + CHIP_ID + "/date_time") {
    rtc.set(
      payload.substring(17,19).toInt(),
      payload.substring(14,16).toInt(),
      payload.substring(11,13).toInt(),
      rtc.dayOfWeek(),
      payload.substring(8,10).toInt(),
      payload.substring(5,7).toInt()-1,
      payload.substring(0,4).toInt()-1900
    );
    String timestamp = getTimeStamp();
    Serial.println("Time set to " + timestamp);
  }
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

void check_EEPROM() {
  t_EEPROM EEPROM_data;
  bool check_fota = false;
  int counter = 0;
  
  EEPROM.begin(8); // Use just 8 bytes
  if (EEPROM.percentUsed() > 0) {
    EEPROM.get(0, EEPROM_data);
    if (EEPROM_data.reinicios == 5) {
      check_fota = true;
    } else {
      counter = EEPROM_data.reboots + 1;
      EEPROM.put(0, EEPROM_data);
      EEPROM.commit();
    }
  }
  
  EEPROM_data.reboots = counter;
  EEPROM.put(0, EEPROM_data);
  EEPROM.commit(); 
  
  if (check_fota) {
    check_fota();
  }

String getTimeStamp() {
  delay(10);
  rtc.refresh();

  String timestamp = ""; //iso8601 (2019-12-12T14:41:38+0000)
  timestamp.concat(rtc.year()+1900);
  timestamp.concat("-");

  if (rtc.month()+1 < 10) {
    timestamp.concat(0);
  }
  timestamp.concat(rtc.month()+1);
  timestamp.concat("-");

  if (rtc.day() < 10) {
    timestamp.concat(0);
  }
  timestamp.concat(rtc.day());
  timestamp.concat("T");

  timestamp.concat(rtc.hour());
  timestamp.concat(":");

  if (rtc.minute() < 10) {
    timestamp.concat(0);
  }
  timestamp.concat(rtc.minute());
  timestamp.concat(":");

  if (rtc.second() < 10) {
    timestamp.concat(0);
  }
  timestamp.concat(rtc.second());

  timestamp.concat("+0");
  timestamp.concat(utcOffsetInSeconds/3600);
  timestamp.concat("00");

  return timestamp;
}

void check_date_time() {
  // Reset time register
  struct timezone tz={0,0};
  struct timeval tv={0,0};
  settimeofday(&tv, &tz);

  configTime(utcOffsetInSeconds, daylightOffsetInSeconds, "cronos.uma.es");

  int timeout = 0;
  Serial.print("Waiting for time");
  while (!time(nullptr) && timeout < 5000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (timeout == 5000) {
    Serial.print("Time could not be retrieved");
  } else {
    time_t now = time(nullptr);
    struct tm * timeinfo = localtime(&now);
    Serial.println(now);

    Wire.begin(4, 5); // D1 (SCL) and D2 (SDA) on ESP8266
    
    rtc.set(timeinfo->tm_sec, timeinfo->tm_min, timeinfo->tm_hour, timeinfo->tm_wday, timeinfo->tm_mday, timeinfo->tm_mon, timeinfo->tm_year);
    
    String timestamp = getTimeStamp();
    Serial.println(timestamp);
  }
}

void setup() {
  Serial.begin(115200);

  Serial.println("Booting");
  
  setup_wifi();
  
  check_EEPROM();

  check_date_time();
  
  client.begin(mqtt_server, espClient);
  client.onMessage(callback);
  
  dht.setup(2, DHTesp::DHT11);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(10); // Advised for stability
  
  //Get device data values 
  t_Device Device = get_device_data();
  
  //Serialize and publish device data
  String json = device_serialize_JSON(Device);

  client.publish(String("GRUPOG/" + CHIP_ID + "/device"), json, false, 1);
  Serial.print("Publish message: ");
  Serial.println(json.c_str());
  
  //Get sensor data values
  t_Sensor Sensor = get_sensor_data();

  //Serialize and publish sensor data
  json = sensor_serialize_JSON(Sensor);
  
  client.publish(String("GRUPOG/" + CHIP_ID + "/sensor"), json, false, 1);
  Serial.print("Publish message: ");
  Serial.println(json.c_str());

  client.loop();
  delay(10); // Advised for stability

  do_deep_sleep();
}
