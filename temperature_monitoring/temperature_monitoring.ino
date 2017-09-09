//temperature_monitoring.ino
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

//Constants
#define I2C_ADDR    0x3F // <<----- Add your address here.  Find it from I2C Scanner
#define BACKLIGHT_PIN     3
#define En_pin  2
#define Rw_pin  1
#define Rs_pin  0
#define D4_pin  4
#define D5_pin  5
#define D6_pin  6
#define D7_pin  7
// LiquidCrystal_I2C  lcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);
LiquidCrystal_I2C lcd(I2C_ADDR, 16, 2);

#define VCC D6
#define GND D8
#define DHTPIN D7     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE); //// Initialize DHT sensor for normal 16mhz Arduino


WiFiClient espClient;
PubSubClient client(espClient);

//Variables
const char* ssid = "Nexus5";
const char* password = "pass";

const char* mqtt_server = "mqtt.mydevices.com";
uint16_t port = 1883;
const char* clientId = "d503e240-90a3-11e7-b153-197ebdab87be";
const char* username = "pass";
const char* passmqtt = "695ccd307bb262c42460c8aa4a6e6c88dacfed9f";

char strObj[256];

long lastMsg = 0;
char msg[50];
int value = 0;

int chk;
float humi;  //Stores humidity value
float temp; //Stores temperature value
char humi_str[15];//= "hvac_hum,p=";
char temp_str[15];//= "temp,c=";
char tempFtoC[5];
char humiFtoC[5];

void create_json(float humi, float temp){	
	StaticJsonBuffer<200> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	root["deviceId"] = "dht22-esp8266-01";
	root["deviceType"] = "sensor";
	JsonObject& data = root.createNestedObject("data");
	JsonObject& value = data.createNestedObject("value");
	value["temperature"] = temp;
	value["humidity"] = humi;
	JsonObject& location = data.createNestedObject("location");
	location["latitude"] = -6.983017;
	location["longitude"] = 107.637484;
	root.printTo(strObj, sizeof(strObj));
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

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

  lcd.clear();
  lcd.print("Wifi Connected");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());
  delay(2000);  
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientId, username, passmqtt)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      // client.publish("outTopic", "hello world");
      // ... and resubscribe
      // client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
    pinMode(GND, OUTPUT);
    pinMode(VCC, OUTPUT);
    digitalWrite(GND, 0);
    digitalWrite(VCC, 1);

    //Setup LCD i2c
    lcd.init(4,5); // sda:scl - D2:D1 - gpio04:gpio05
    lcd.begin (16,2); //  <<----- LCD was 16x2
    lcd.setBacklight(HIGH);
    lcd.home (); // go home
    lcd.print("Waiting wifi..");

    //Setup Wifi
    setup_wifi();

    //setup mqtt
    client.setServer(mqtt_server,port);
    client.setCallback(callback);

	Serial.begin(9600);
	dht.begin();
}

void loop() {

	if (!client.connected()) {
		reconnect();
	}
	client.loop();

	long now = millis();
	if (now - lastMsg > 10000) { // refresh 10 detik
		lastMsg = now;

	    //Read data and store it to variables hum and temp
	    humi = dht.readHumidity();
	    temp= dht.readTemperature();
	    
	    //Print temp and humidity values to serial monitor
	    Serial.print("Humidity: ");
	    Serial.print(humi);
	    Serial.print(" %, Temp: ");
	    Serial.print(temp);
	    Serial.println(" Celsius");
	    lcd.clear();
	    lcd.setCursor(0,0);
	    lcd.print("Suhu: ");
	    lcd.print(temp);
	    lcd.print("C");
	    lcd.setCursor(0,1);
	    lcd.print("Lembab: ");
	    lcd.print(humi);
	    lcd.print("%");

		Serial.print("Publish message: ");
		// create_json(humi,temp);
		// Serial.println(strObj);
		// client.publish("root/user1", strObj);
    dtostrf(temp,3,2,tempFtoC);
    dtostrf(humi,3,2,humiFtoC);
    // strcat(temp_str,tempFtoC);
    // strcat(humi_str,humiFtoC);
    sprintf(temp_str,"temp,c=%s",tempFtoC);
    sprintf(humi_str,"hvac_hum,p=%s",humiFtoC);
    Serial.print("Publish:\t");
    Serial.println(temp_str);
    Serial.print("Publish:\t");
    Serial.println(humi_str);
    client.publish("v1/bf542b40-35d5-11e7-bc16-a7db64aef1f1/things/d503e240-90a3-11e7-b153-197ebdab87be/data/1", temp_str);
    client.publish("v1/bf542b40-35d5-11e7-bc16-a7db64aef1f1/things/d503e240-90a3-11e7-b153-197ebdab87be/data/2", humi_str);

	}


}