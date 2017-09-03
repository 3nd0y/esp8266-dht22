Temperature Monitoring - IoT Project base ESP8266 - 12
======================================================

It's create to support my project irigation system in my home. One of the project need a monitoring temperature and humidity system. 

To use this firmaware you will need hardware below:
* Wemos Mini D1
* LCD I2C 16x2
* DHT-22 temperature & humidity sensor
* breadboard


Wiring the schematic is simple enough:
* DHT22 pin + goen to D6_pin
* DHT22 pin out goes to D7_pin
* DHT22 pin - goes to D8_pin
* LCD + goes to VCC5+ -> Since we use i2c just one way to LCD so 5V will not harm your ESP8266 
* LCD - goes to GND
* LCD SDA goes to D2
* LCD SCL goes to D1 
* SDA and SCL use pull up resistor 2kOhm
