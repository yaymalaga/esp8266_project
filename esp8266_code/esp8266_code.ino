#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

// General parameters
const char* ssid = "infind";
const char* password = "1518wifi";
const char* mqtt_server = "172.16.53.131";

// General objects
WiFiClient espClient;
PubSubClient client(espClient);
DHTesp dht;

// General variables
bool deep_sleep = false;
long nowTime, lastTime = 0;

void setup_wifi() {
  delay(10);
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

void check_connection() {
  // Documentation: int connected ()
  // Documentation: boolean loop ()
  if (!client.connected() || !client.loop()) {
    bool connection_ok = false;
    while (!connection_ok) {
      Serial.print("Attempting MQTT connection...");

      // Create a random client ID
      String clientId = "ESP8266Client-";
      clientId += String(random(0xffff), HEX);

      // TODO: Change

      // Documentation: boolean connect (clientID, username, password, willTopic, willQoS, willRetain, willMessage, cleanSession)
      bool connection_ok = client.connect(clientId.c_str(),"","","GRUPOG/tele/LWT",0,1,"GrupoG Offline");
      if (connection_ok == true) {
        Serial.println("connected");
        // Documentation: int publish (topic, payload, retained)
        connection_ok = client.publish("GRUPOG/tele/LWT", "GrupoG Online", true);
        // Documentation: bool subscribe (topic, [qos])
        if (connection_ok == true) {
          client.subscribe("example",1);
        }
      }

      if (connection_ok == false) {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        // Wait 2 seconds before retrying
        delay(2000);
      }
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

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  dht.setup(5, DHTesp::DHT11);
}

void loop() {
  if (!deep_sleep) {
    deep_sleep = true;

    int tries = 0;
    while (tries <= 1) {
      tries++;

      check_connection();

      // Documentation: subscribe (topic, [qos])
      if (!client.publish("example1","demo")) {
        continue;
      } else {
        break; // Or tries = 2;
      }
    }

    tries = 0;
    while (tries <= 1) {
      tries++;

      check_connection();

      // Documentation: subscribe (topic, [qos])
      if (!client.publish("example2","demo2")) {
        continue;
      } else {
        break; // Or tries = 2;
      }
    }

    // Small wait just in case
    lastTime = millis();
    nowTime = lastTime;
    while(nowTime - lastTime < 1000){
      nowTime = millis();
      client.loop();
    }

    //Deep-sleep for 3s
    int deep_sleep_time = 3; // TODO: Read from json
    ESP.deepSleep(deep_sleep_time*1000000);
  }
  delay(100);
}
