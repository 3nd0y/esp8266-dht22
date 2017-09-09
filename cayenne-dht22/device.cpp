// This example shows how to connect to Cayenne using an ESP8266 and send/receive sample data.
// Make sure you install the ESP8266 Board Package via the Arduino IDE Board Manager and select the correct ESP8266 board before compiling. 

//#define CAYENNE_DEBUG
#define CAYENNE_PRINT Serial
#ifndef D1_MINI
 #define D1_MINI
#endif
#include <CayenneMQTTESP8266.h>
#include <DHT.h>


#define VCC D6
#define GND D8
#define DHTPIN D7     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE); //// Initialize DHT sensor for normal 16mhz Arduino

// WiFi network info.
char ssid[] = "Nexus5";
char wifiPassword[] = "qwertyuiop";

// DHT22 variable
float hum;  //Stores humidity value
float temp; //Stores temperature value

// Cayenne authentication info. This should be obtained from the Cayenne Dashboard.
char username[] = "bf542b40-35d5-11e7-bc16-a7db64aef1f1";
char password[] = "695ccd307bb262c42460c8aa4a6e6c88dacfed9f";
char clientID[] = "2c061c80-909e-11e7-a491-d751ec027e48";

unsigned long lastMillis = 0;

void setup() {
	pinMode(GND, OUTPUT);
    pinMode(VCC, OUTPUT);
    digitalWrite(GND, 0);
    digitalWrite(VCC, 1);
	Serial.begin(9600);
	Cayenne.begin(username, password, clientID, ssid, wifiPassword);
}

void loop() {
	Cayenne.loop();

	//Publish data every 10 seconds (10000 milliseconds). Change this value to publish at a different interval.
	if (millis() - lastMillis > 10000) {
		lastMillis = millis();
		//Read data and store it to variables hum and temp
	    hum = dht.readHumidity();
	    temp= dht.readTemperature();
		Cayenne.celsiusWrite(1, temp);
		//Some examples of other functions you can use to send data.
		//Cayenne.celsiusWrite(1, 22.0);
		//Cayenne.luxWrite(2, 700);
		//Cayenne.virtualWrite(3, 50, TYPE_PROXIMITY, UNIT_CENTIMETER);
	}
}

//Default function for processing actuator commands from the Cayenne Dashboard.
//You can also use functions for specific channels, e.g CAYENNE_IN(1) for channel 1 commands.
CAYENNE_IN_DEFAULT()
{
	CAYENNE_LOG("CAYENNE_IN_DEFAULT(%u) - %s, %s", request.channel, getValue.getId(), getValue.asString());
	//Process message here. If there is an error set an error message using getValue.setError(), e.g getValue.setError("Error message");
}
