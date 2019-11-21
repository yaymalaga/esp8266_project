#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Parámetros de la conexión
const char* ssid = "infind";
const char* password = "1518wifi";
const char* mqtt_server = "172.16.53.131";
WiFiClient espClient;
PubSubClient client(espClient);

// Parámetros generales
long nowTime, lastTime = 0;
int time_DeepSleep;

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
      client.subscribe("GRUPOG/deep-sleep");
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

  //Deep-Sleep time configuration according to topic
  float param_DeepSleep;
  param_DeepSleep = payload.toFloat();
  if (param_DeepSleep >= 1){ //The minimum time for DeepSleep is 1min
    time_DeepSleep = param_DeepSleep * 60000000;
  }
  else {
    time_DeepSleep = 60000000 //The default value if the msg does not give a valid number is 1min
  }
  
}

void setup() {
  Serial.begin(115200);
  
  setup_wifi();
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
  
    //Deep-sleep for x min
    lastTime = millis();
    nowTime = lastTime;
    while(nowTime - lastTime < 1000){
      nowTime = millis();
      client.loop();
    }
    ESP.deepSleep(time_DeepSleep);

}
