#include <ESP8266WiFi.h>
#include <MQTT.h>
#include "DHTesp.h"
#include <ESP8266httpUpdate.h>
#include <ESP_EEPROM.h>
#include <Arduino_JSON.h>
#include "Wire.h"
#include "uRTCLib.h"
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>
#include <Adafruit_ADS1015.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//Define macros
#define HTTP_OTA_ADDRESS      F("172.16.53.132")       //Address of OTA update server
#define HTTP_OTA_PATH         F("/esp8266-ota/update") // Path to update firmware
#define HTTP_OTA_PORT         1880                     // Port of update server
                                                       // Name of firmware
#define HTTP_OTA_VERSION      String(__FILE__).substring(String(__FILE__).lastIndexOf('\\')+1) + ".nodemcu"

Adafruit_ADS1015 ads1015; // Construct an ads1015 at the default address: 0x48 (GROUND)

#define ONE_WIRE_BUS 0 // Data wire is plugged into digital pin 3 on the Arduino
OneWire oneWire(ONE_WIRE_BUS); // Setup a oneWire instance to communicate with any OneWire device
DallasTemperature sensors(&oneWire); // Pass oneWire reference to DallasTemperature library

// Connection parameters
String ssid = "infind";
String password = "1518wifi";
const char* mqtt_server = "172.16.53.131";

// General objects
WiFiManager wifiManager;
WiFiClient espClient;
MQTTClient client(1024);
DHTesp dht;
uRTCLib rtc(0x68);

// General variables
float deep_sleep_time = 10;
int reboots_fota = 5;
const int utcOffsetInSeconds = 0;
const int  daylightOffsetInSeconds = 3600;
const char* ntpServer = "cronos.uma.es";
String CHIP_ID;

// Struct types
typedef struct {
  int   reboot_counter;
} t_EEPROM;

typedef struct {
  String timestamp;
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
  int state;
} t_LightmeterData;

typedef struct {
  float temperature;
  float humidity;
  int state;
} t_DH11Data;

typedef struct {
  float ground_temperature;
  int state;
} t_ds18b20Data;

typedef struct {
  float ground_humidity;
  int state;
} t_hl69Data;

typedef struct {
  int code;
}t_error;

typedef struct {
  t_DH11Data DH11;
  t_LightmeterData LightSensor;
  t_ds18b20Data DS18B20;
  t_hl69Data HL_69;
  t_error Error_Code;
} t_Sensor;

//Declare serialize functions
String data_serialize_JSON(t_Sensor &sensor_data, t_Device &device_data) {
  JSONVar jsonRoot;
  JSONVar device;
  JSONVar sensors;

  // Set sensors data as body
  sensors["DH11"]["temperature"] = sensor_data.DH11.temperature;
  sensors["DH11"]["humidity"] = sensor_data.DH11.humidity;
  sensors["DH11"]["state"] = sensor_data.DH11.state;
  
  sensors["LightSensor"]["light"]= sensor_data.LightSensor.light;
  sensors["LightSensor"]["state"] = sensor_data.LightSensor.state;

  sensors["HL_69"]["ground_humidity"]= sensor_data.HL_69.ground_humidity;
  sensors["HL_69"]["state"]= sensor_data.HL_69.state;
  
  sensors["DS18B20"]["ground_temperature"]= sensor_data.DS18B20.ground_temperature;
  sensors["DS18B20"]["state"]= sensor_data.DS18B20.state;

  sensors["Error_Code"] = sensor_data.Error_Code.code;

  jsonRoot["body"] = sensors;
  
  // Set device data as header
  device["Board"]["data"]["timestamp"] = device_data.Board.data.timestamp;
  device["Board"]["data"]["chip_id"] = device_data.Board.data.chip_id;
  
  device["Board"]["conf"]["deep_sleep_time"] = device_data.Board.conf.deep_sleep_time;
  
  device["WifiModule"]["data"]["ap"] = device_data.WifiModule.data.ap;
  device["WifiModule"]["data"]["bssid"] = device_data.WifiModule.data.bssid;
  device["WifiModule"]["data"]["ip"] = device_data.WifiModule.data.ip;
  device["WifiModule"]["data"]["rssi"] = device_data.WifiModule.data.rssi;
  
  device["WifiModule"]["conf"]["ssid"] = device_data.WifiModule.conf.ssid;
  device["WifiModule"]["conf"]["password"] = device_data.WifiModule.conf.password;
  device["WifiModule"]["conf"]["mqtt_server"] = device_data.WifiModule.conf.mqtt_server;
  
  jsonRoot["header"] = device;
  
  return JSON.stringify(jsonRoot);
}

t_Device get_device_data() {
  //Gets the information from the device and returns a struct with this data.
  t_Device Device;
  
  //Get Board data
  Device.Board.data.timestamp = getTimeStamp();
  Device.Board.data.chip_id = CHIP_ID;
  
  //Get Board conf
  Device.Board.conf.deep_sleep_time = deep_sleep_time;

  //Get WifiModule data
  Device.WifiModule.data.ap = 1;
  Device.WifiModule.data.bssid = WiFi.BSSIDstr();
  Device.WifiModule.data.ip = WiFi.localIP().toString();
  Device.WifiModule.data.rssi =  WiFi.RSSI();
  
  //Get WifiModule conf
  Device.WifiModule.conf.ssid = ssid;
  Device.WifiModule.conf.password = password;
  Device.WifiModule.conf.mqtt_server = mqtt_server;

  return Device;
}

t_Sensor get_sensor_data(){
  //Gets the information from the sensors and returns a struct with this data
  t_Sensor Sensor;

  //Get DH11 data
  Sensor.DH11.temperature = dht.getTemperature();
  Sensor.DH11.humidity = dht.getHumidity();

  //Get LightSensor data
  Sensor.LightSensor.light = analogRead(A0);
  Sensor.LightSensor.light = map(Sensor.LightSensor.light,0,1023,0,1850);//Max value of the light sensor: 1850 W/m2
  
   //Get HL-69 data
  int16_t hl_69 = ads1015.readADC_SingleEnded(0);
  //Max value given by ads1015: 1646. Max value of ground_humidity: 100%
  Sensor.HL_69.ground_humidity = map(hl_69,0,1646,0,100);

  //Get DS18B20 data
  sensors.requestTemperatures();
  Sensor.DS18B20.ground_temperature = sensors.getTempCByIndex(0);
  
  // Error Control
  Sensor = Error_Control(Sensor);

  return Sensor;
}

void setup_wifi() {  
  wifiManager.setConnectTimeout(15); // 15s timeout to connect to wifi
  wifiManager.setConfigPortalTimeout(300); // 5min timeout to configure wifi access

  if(!wifiManager.autoConnect(("ESP8266_" + CHIP_ID).c_str(), "1234")) {
    Serial.println("Wifi configuration or connection timeout");
    do_deep_sleep();
  }

  ssid = WiFi.SSID();
  password = WiFi.psk();

  Serial.println("WiFi connected");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void do_deep_sleep() {
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

    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.setWill(String("GRUPOG/" + CHIP_ID + "/status").c_str(), "GrupoG Offline", true, 1);
      client.publish("GRUPOG/" + CHIP_ID + "/status", "GrupoG Online", true, 1);
      client.subscribe("GRUPOG/" + CHIP_ID + "/deep_sleep", 1); //TODO: Check that the callback is called with retained msg
      client.subscribe("GRUPOG/" + CHIP_ID + "/reboots_fota", 1); //TODO: Check that the callback is called with retained msg
      client.subscribe("GRUPOG/" + CHIP_ID + "/manual_date_time", 1);
      client.subscribe("GRUPOG/" + CHIP_ID + "/check_date_time", 1);
      client.subscribe("GRUPOG/" + CHIP_ID + "/ntp_date_time", 1);
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

  if (topic == "GRUPOG/" + CHIP_ID + "/deep_sleep") {
    deep_sleep_time = payload.toFloat();
    Serial.println("DeepSleep set to " + (String)deep_sleep_time + " minutes");
  } else if (topic == "GRUPOG/" + CHIP_ID + "/reboots_fota") {
    reboots_fota = payload.toFloat();
    Serial.println("FOTA is going to be checked every " + (String)reboots_fota + " reboots");
  } else if (topic == "GRUPOG/" + CHIP_ID + "/manual_date_time") {
    // ISO8601 (2019-12-12T14:41:38+0000)
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
  } else if (topic == "GRUPOG/" + CHIP_ID + "/check_date_time") {
    check_date_time(true);
    Serial.println("Force time set");
  } else if (topic == "GRUPOG/" + CHIP_ID + "/ntp_date_time") {
    ntpServer = payload.c_str();
    check_date_time(true);
    Serial.println("NTP set to " + (String)ntpServer);
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
  bool do_check_fota = false;
  int counter = 0;
  
  EEPROM.begin(128); // Use just 8 bytes
  if (EEPROM.percentUsed() > 0) {
    EEPROM.get(0, EEPROM_data);
    
    if (EEPROM_data.reboot_counter == reboots_fota) {
      do_check_fota = true;
    } else {
      counter = EEPROM_data.reboot_counter + 1;
    }
  }

  Serial.print("Number of reboots: ");
  Serial.println(EEPROM_data.reboot_counter);
  Serial.println(counter);
  EEPROM_data.reboot_counter = counter;
  EEPROM.put(0, EEPROM_data);
  EEPROM.commit(); 
  
  if (do_check_fota) {
    check_fota();
  }
}

t_Sensor Error_Control(t_Sensor S) {
  int num_errors = 0;
  //State of DH11 sensor
  if ((S.DH11.temperature > 0 & S.DH11.temperature <= 50)& (S.DH11.humidity >= 20 & S.DH11.humidity <= 90)) {
    S.DH11.state = 0;
  }
  else {
    // TODO: Comprobar si cuando están desactivados dan un valor 0
    if (S.DH11.temperature == 0 & S.DH11.humidity == 0) {
      S.DH11.state = 1;
    }
    else {
      S.DH11.state = 2;
    }
    S.DH11.temperature = -999;
    S.DH11.humidity = -999;
    num_errors = num_errors + 1;
  }

  //State of Light sensor
  if (S.LightSensor.light > 0 & S.LightSensor.light <= 1850) {
    S.LightSensor.state = 0;
  }
  else {
    // TODO: Comprobar si cuando están desactivados dan un valor 0
    if (S.LightSensor.light == 0) {
      S.LightSensor.state = 1;
    }
    else {
      S.LightSensor.state = 2;
    }
    S.LightSensor.light = -999;
    num_errors = num_errors + 1;
  }
  
  //State of HL_69 sensor
  //TODO: comprobar el valor exacto que marca cuando está desactivado (creo que estaba en torno a 300)
  if (S.HL_69.ground_humidity >= 0 & S.HL_69.ground_humidity <= 100) {
        S.HL_69.state = 0;
  }
  else {
    S.HL_69.state = 1;
    S.HL_69.ground_humidity = -999;
    num_errors = num_errors + 1;
  }

  //State of DS18B20 sensor
  if (S.DS18B20.ground_temperature >= -55 & S.DS18B20.ground_temperature <= 125) {
    S.DS18B20.state = 0;
  }
  else {
    if (S.DS18B20.ground_temperature <= -125) {
      S.DS18B20.state = 1;
    }
    else {
      S.DS18B20.state = 2;
    }
    S.DS18B20.ground_temperature = -999;
    num_errors = num_errors + 1;
  }
  
  if (num_errors == 0){
    S.Error_Code.code = 0;
  } else {
    S.Error_Code.code = -1;
  }
  
  return S;
}

String getTimeStamp() {
  delay(10);
  rtc.refresh();

  String timestamp = ""; // ISO8601 (2019-12-12T14:41:38+0000)
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

bool compare_data_time(tm * online) {
  // Check if +-2 min from online
  if ((rtc.year()+1900 == online->tm_year) &&
    (rtc.month()+1 == online->tm_mon) &&
    (rtc.day() == online->tm_mday) &&
    (rtc.hour() == online->tm_hour) &&
    (rtc.minute() > online->tm_min -2) &&
    (rtc.minute() < online->tm_min +2)) {
    return true;
  } else {
    return false; 
  }   
}

void check_date_time(bool force) {
  // Reset time register
  struct timezone tz={0,0};
  struct timeval tv={0,0};
  settimeofday(&tv, &tz);

  configTime(utcOffsetInSeconds, daylightOffsetInSeconds, ntpServer);

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
    
    // If force is enable, don't compare +-2 with online clock before setting it
    if (force) {
      Serial.println("Forced time set");
      rtc.set(timeinfo->tm_sec, timeinfo->tm_min, timeinfo->tm_hour, timeinfo->tm_wday, timeinfo->tm_mday, timeinfo->tm_mon, timeinfo->tm_year);
    } else {
      if (!compare_data_time(timeinfo)) {
        Serial.println("Local time does not match with online (+-2min). Time set");
        rtc.set(timeinfo->tm_sec, timeinfo->tm_min, timeinfo->tm_hour, timeinfo->tm_wday, timeinfo->tm_mday, timeinfo->tm_mon, timeinfo->tm_year);
      } else {
        Serial.println("Local time matches online one (+-2min)");  
      }
    }
    
    String timestamp = getTimeStamp();
    Serial.println(timestamp);
  }
}

void setup() {
  Serial.begin(115200);

  Serial.println("Booting");

  // Check on booting the settings reset button
  pinMode(0, INPUT_PULLUP);
  if (digitalRead(0) == LOW) {
      wifiManager.resetSettings();
    }

  CHIP_ID = (String) ESP.getFlashChipId();
  Serial.println(CHIP_ID);
  
  setup_wifi();
  
  check_EEPROM();

  check_date_time(false);
  
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
  t_Device device_data = get_device_data();
  
  //Get sensor data values
  t_Sensor sensor_data = get_sensor_data();
  
  //Serialize and publish data
  String json = data_serialize_JSON(sensor_data, device_data);

  client.publish(String("GRUPOG/" + CHIP_ID + "/sensor"), json, false, 1);
  Serial.print("Publish message: ");
  Serial.println(json.c_str());

  // Wait at least 15s for mqtt qos
  unsigned long lastMillis = millis();
  Serial.println("WAITING");
  while (millis() - lastMillis < 15000) {
    client.loop();
    delay(500);
  }
  Serial.println("DONE");

  do_deep_sleep();
}
