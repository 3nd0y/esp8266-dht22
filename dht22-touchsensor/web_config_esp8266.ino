/* Create a WiFi access point and provide a web server on it. */
#define DHTPIN D5 // D5 get data sensor
#define DHTTYPE DHT22
#define MQTT_VERSION MQTT_VERSION_3_1
#define MQTT_MAX_PACKET_SIZE 256
#define LAMP D2
#define TOUCH D1

#ifndef _TEMPERATURE_
#define _TEMPERATURE_ true
#endif

#ifndef _HUMIDITY_
#define _HUMIDITY_ false
#endif

#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include "FS.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <DHT.h>



/*************************
 * Global variable here... 
**************************/
// Setup variable
const char *ssid = "TELLO";
const char *password = "qwertyuiop";   
const char* mqtt_server = "192.168.0.100";
String DEVICE_ID = "ESP8266-1234567890";

boolean status_wifi = false; // Check wifi is in server mode or client, if server then true
long lastMsg = 0;

// var flipflop
int ledState = LOW;

// Var for touch sensor loop
boolean button = false;
boolean oldButton = false;
boolean state = false;

// Variables for DHT22
int chk;
float hum; // store humidity value
float temp;

DHT dht(DHTPIN,DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);
// Setup server at port 80
ESP8266WebServer server(80);  
// Reserve memory pool JSON
StaticJsonBuffer<512> jsonBuffer;

void listDir();
void endUser();
String formatBytes(size_t bytes);
void handleRoot();
void handleUpdate();
void reconnect_mqtt();
void mqttLoop();
void callback(char* topic, byte* payload, unsigned int length); //receive byte by subscriber
void publish_data(long data, const char *id, boolean temphum); // publish data for sensor
void publish_status(String stat); // publish data status device for switch/outlet
void touchSensorLoop();
/**********************
 * SETUP before looping
 **********************/
void setup() {
  Serial.begin(115200);  
  Serial.setDebugOutput(true);
    
  dht.begin();
  client.setServer(mqtt_server, 1883);
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output for indicator
  pinMode(LAMP, OUTPUT); // to the lamp/relay
  pinMode(TOUCH, INPUT); // initialize for touch sensor
  WiFi.mode(WIFI_STA);  
  Serial.println("Wifi set to STA");
  
  bool result = SPIFFS.begin();
  Serial.print("SPIFFS open status: "+result);
  
  // Check file configuration exist in SPIFFS      
  bool fileExist = SPIFFS.exists("/config.json");
  if(fileExist){
    // if file config exist
    Serial.println("Open file config.json");
    File f = SPIFFS.open("/config.json","r");
    String dataJson = f.readString();
    Serial.println(dataJson);
    // Parse JSON data
    JsonObject& root = jsonBuffer.parseObject(dataJson);      
    const char *ssid = root["ssid"];
    const char *pwd = root["passkey"];

    // Start connecting network
    delay(20);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid,pwd);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      if (ledState==LOW){
        digitalWrite(BUILTIN_LED,HIGH);
        Serial.print(".");
      }else{
        digitalWrite(BUILTIN_LED,LOW);
        Serial.print(" ");
      }
    }
    Serial.println(" ");
    Serial.println("Connected to "+WiFi.localIP());    
    client.setCallback(callback); //MQTT callback when message arrive
  }else{      
    Serial.println("config.json file is not found");
    Serial.print("Configuring access point...");
    endUser();
  }
}

/*
 ******* THE MAIN LOOP 
*/
void loop() {
  // Only handle client when AP mode
  if (status_wifi) server.handleClient();
  else {
    mqttLoop();
    touchSensorLoop(); 
  }
  
}



/**********************************************************************************************
 *  FUNCTION put below here
 **********************************************************************************************/

void listDir(){
  // List file on SPIFFS
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
}

void endUser(){
  //else if file config doesn't exist
      status_wifi = true;
      WiFi.softAP(ssid, password);
      IPAddress myIP = WiFi.softAPIP();
      Serial.print("AP IP address: ");
      Serial.println(myIP);
      server.on("/", handleRoot);
      server.on("/update",handleUpdate);
      server.begin();
      Serial.println("HTTP server started");
}

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

void handleRoot() {
  String option = "";  
  // scan wifi
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if(n==0) {
    Serial.println("no network found");
    option = "No Wifi around here!!";  
  }else{
    Serial.print(n);
    Serial.println(" network found");
    for (int i=0; i<n; i++) {
      option += "<option>"+WiFi.SSID(i)+"</option>";
    }
  }  
  // Send html tag to client
  
  String payload = "";
  payload += "<!DOCTYPE html><html><head><title>Wifi Config</title></head><body><form action=\"/update\" method=\"GET\">SSID<select name=\"ssid\" autofocus>";
  payload += option;
  payload += "</select><br>Passkey<input type=\"password\" name=\"pass\"><br><input type=\"submit\"></form>";
  payload += "</body></html>";
  server.send(200, "text/html", payload);
  Serial.println("Send client request:");
  Serial.println(payload);
}

void handleUpdate(){    
    // write file configuration
    if (SPIFFS.exists("/config.json")) SPIFFS.remove("/config.json");
    File f = SPIFFS.open("/config.json","w");
    String ssid = server.arg("ssid");
    String pwd = server.arg("pass");
    String payload = "";
    payload += "<h1>SSID is "+ssid+" and password is "+pwd+"</h1>";
    server.send(200,"text/html",payload);
    String sjson = "{\"ssid\":\""+ssid+"\",\"passkey\":\""+pwd+"\"}";    
    f.println(sjson);
    f.close();
    
    //ESP restart after wifi turn to ST mode and write config.json
    WiFi.disconnect();
    ESP.reset();
}

void reconnect_mqtt() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("node/switch");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void mqttLoop(){
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();
  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    //++value; 
    temp = dht.readTemperature(); // Convert to String from float
    hum = dht.readHumidity();
    //snprintf (msg, 75, 'Temperature is: %d', temp);

    // Publish Temperature data
    publish_data(temp,"node/temperature",_TEMPERATURE_);
    delay(500); // delay 500ms before re-publish
    // Publish Humidity data
    publish_data(hum,"node/humidity",_HUMIDITY_);
  }  
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] : ");

  // Convert byte/char to string
  String sPayload((char *)payload);
  String str;
  for (int i = 0; i < length; i++) {
    //Serial.print((char)payload[i]);
    //str+=(char *)payload[i];
    str+=sPayload[i];
  }
  Serial.println(str);
  // Parse JSON data => need to convert String to char[] so do not use buffer
  StaticJsonBuffer<100> memAlloc;
  int i = str.length()+1;
  char strToChar[i];
  str.toCharArray(strToChar,i);
  JsonObject& root = memAlloc.parseObject(strToChar);
  
  // Check parsing/////////////////////
  if(root.success()){
    String id_s = root["id"];
    String set = root["set"];
    String stat = root["status"];
      
    if(id_s==DEVICE_ID){   // Conditional if Server ask status
      Serial.println("Device ID sama..");
      if(stat=="ask"){
      // Check device status
      Serial.println("Apps or server is asking status");
      int val = digitalRead(LAMP);
      (val==0)?publish_status("off"):publish_status("on");
      }else if (set=="on"){
        Serial.println("Lamp/Relay is ON");
        digitalWrite(LAMP,HIGH); 
        state = true;
      }else if(set=="off"){
        Serial.println("Lamp/Relay is OFF");
        digitalWrite(LAMP,LOW);
        state = false;
      }
    }else{
      Serial.println("Command not available..or DEVICE ID is not equivalen");
    }
   }else{
    // Conditional command "RESET" and "LS"
    if(str=="reset"){
      // remove file config.json
      if (SPIFFS.exists("/config.json")) {
        SPIFFS.remove("/config.json");
        Serial.println("config.json is removed...");
      }else Serial.println("file config.json not found");    
    }else if(str=="ls"){
      // list file in SPIFFS
      listDir();
    }else{ 
    Serial.print("Parsing not succeed, need convert string or char * to char[]");
    }
  }
}

void publish_data(long data, const char *id, boolean temphum){
  String devFunc;
  temphum?devFunc="Temperature":devFunc="Humidity";
  String value = ""; // store JSON data buffer
    value = "{\"deviceId\":\""+DEVICE_ID+"\",";
    value += "\"deviceType\":\"DHT22 Sensor\",";
    value += "\"deviceFunc\":\""+devFunc+"\",";
    value += "\"data\":{";
    value += "\"value\":"+String(data)+",";
    value += "\"location\":{";
    value += "\"latitude\":-6.983017,\"longitude\":107.637484}}}";
    
    int i = value.length()+1;
    char buffers[i];
    value.toCharArray(buffers, i);
    if (!client.publish(id,buffers)){
      Serial.println("Cek max paket data MQTT");
    }else{
      if (temphum){
        Serial.print("Temperature is: ");
        Serial.print(data);
        Serial.println(" Celcius");
      }else{
        Serial.print("Humidity is: ");
        Serial.print(data);
        Serial.println(" %");
      }
    }  
}

void publish_status(String stat){
  String value={"\"id\":\""+DEVICE_ID+"\",\"status\":\""+stat+"\""};
  int i = value.length()+1;
  char buffers[i];
  value.toCharArray(buffers, i);
  (client.publish("node/switch",buffers))?Serial.println(value):Serial.println("Cek max data");
}

void touchSensorLoop(){
  button = !digitalRead(TOUCH); // button is true when the sensor touched.. use inverse due to sensor is active low
  if (button && !oldButton){
    if (!state){
      digitalWrite(LAMP,HIGH);
      state = true;
    }else{
      digitalWrite(LAMP,LOW);
      state = false;
    }
    oldButton = true;
  }else if(!button && oldButton){
    oldButton = false;
  }
}


