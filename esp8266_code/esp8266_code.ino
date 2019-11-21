#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266httpUpdate.h>

#define HTTP_OTA_ADDRESS      F("172.16.53.132")       //TODO arrange this as configurable info //Address of OTA update server
#define HTTP_OTA_PATH         F("/esp8266-ota/update") // Path to update firmware
#define HTTP_OTA_PORT         1880                     // Port of update server
                                                       // Name of firmware
#define HTTP_OTA_VERSION      String(__FILE__).substring(String(__FILE__).lastIndexOf('\\')+1) + ".nodemcu" 


// Parámetros de la conexión
const char* ssid = "infind";
const char* password = "1518wifi";
const char* mqtt_server = "172.16.53.131";
WiFiClient espClient;
PubSubClient client(espClient);

// Parámetros generales
bool deep_sleep = false;
long nowTime, lastTime = 0;

void setup_wifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    // TODO: Change
    // Attempt to connect and set the LWT message
    if (client.connect(clientId.c_str(),"","","GRUPOG/tele/LWT",0,1,"GrupoG Offline")) {
      Serial.println("connected");
      // Once connected, publish an retained announcement
      client.publish("GRUPOG/tele/LWT", "GrupoG Online", true);
      // ... and resubscribe
      client.subscribe("GRUPOG/cmnd/power");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // TODO: Handle topic logic
}

void setup() {
  Serial.begin(115200);

  Serial.println("Booting");
  
  setup_wifi();
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
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!deep_sleep) {
    deep_sleep = true;
    
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
  
    //Deep-sleep for 3s. Small wait just in case
    lastTime = millis();
    nowTime = lastTime;
    while(nowTime - lastTime < 1000){
      nowTime = millis();
      client.loop();
    }
    
    //TODO add debug part for final testing of OTA update
    
    ESP.deepSleep(3000000);
  }
}
