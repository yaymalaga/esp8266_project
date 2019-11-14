## Set-up

Arduino IDE (^1.6.4) need to be configured in order to support the ESP8266 board.

1. Go to 'Files' => 'Preferences' and add the following url in the 'Additional Boards Manager URLs' field:

```
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

2. Next, go to 'Tools' => 'Board: ..' => 'Boards Manager', filter the list by 'esp8266' and install it.

3. Finally, choose the board '' in 'Tools' => 'Board: ..' and the proper port in 'Tools' => 'Port'
