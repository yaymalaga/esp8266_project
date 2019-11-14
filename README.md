## Set-up

### ESP8266

Arduino IDE (^1.6.4) need to be configured in order to support the ESP8266 board.

1. Go to 'Files' => 'Preferences' and add the following url in the 'Additional Boards Manager URLs' field:

```
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

2. Next, go to 'Tools' => 'Board: ..' => 'Boards Manager', filter the list by 'esp8266' and install it.

3. Finally, choose the board '' in 'Tools' => 'Board: ..' and the proper port in 'Tools' => 'Port'

### Dependencies

The following dependencies need to be manually in 'Tools' => 'Manage libraries':

- DHT sensor library for ESPx

- PubSubClient

- Arduino_JSON

### External dependencies

- Time

Download the library from github, https://github.com/PaulStoffregen/Time/archive/master.zip, and go to 'Sketch' => 'Include Library' => 'Add .zip library' to install it

### PubSubClient config

The library has a packet size limit that we need to modify in order to be able to work with it.

Once installed, the library file should appear in the Arduino folder created by Arduino IDE in your computer:

```
\Arduino\libraries\PubSubClient\src\PubSubClient.h
```

The line we need to change is the following one, where '128' must be changed to at least '1024':

```
#define MQTT_MAX_PACKET_SIZE 128
```
