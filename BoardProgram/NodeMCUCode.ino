#include "ThingSpeak.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTTYPE    DHT11
#define DHTPIN 2

DHT_Unified dht(DHTPIN, DHTTYPE);

char ssid[] = "SSID";
char pass[] = "password";
WiFiClient client;

AsyncWebServer server(80);

unsigned long SprinkChannelNumber = X;
unsigned long WeaChannelNumber = X;
const char * myWriteAPIKey1 = "APIKEY1";
const char * myWriteAPIKey2 = "APIKEY2";
int field[7] = {1,2,3,4,5,6,7};
int eStop, LSprink, BSprink, LDelay, BDelay;
int stat;
int initlivStat, initbedStat;
int livStat = 0;
int bedStat = 0;
int err = 0;

int LivSprink = 5;
int BedSprink = 4;


void callback(unsigned char* data, unsigned int length)
{
    data[length] = '\0';
    Serial.println((char*) data);
}

void getValues(){
  stat = ThingSpeak.readMultipleFields(SprinkChannelNumber);
  if(stat == 200){
    LSprink = ThingSpeak.getFieldAsInt(field[1]);
    BSprink = ThingSpeak.getFieldAsInt(field[2]);
    LDelay = ThingSpeak.getFieldAsInt(field[3]);
    BDelay = ThingSpeak.getFieldAsInt(field[4]);
    String statusMessage = ThingSpeak.getStatus();
    WebSerial.println("Status Message, if any: " + statusMessage);
    Serial.println("Status Message, if any: " + statusMessage);
    Serial.println("Read Successful");
    WebSerial.println("Read Successful");
    
  }
}

void updateVal(){
  if(eStop != 1){
    if(livStat == 1){
      ThingSpeak.setField(2, 1);
      ThingSpeak.setField(4, LDelay);
    }
    else{
      ThingSpeak.setField(2, 0);
    }
    if(bedStat == 1){
      ThingSpeak.setField(3, 1);
      ThingSpeak.setField(5, BDelay);
    }
    else{
      ThingSpeak.setField(3, 0);
    }
  }
  stat = ThingSpeak.writeFields(SprinkChannelNumber, myWriteAPIKey1);
  if(err >= 10){
    ESP.reset();
  }
  if(stat == 200){
    Serial.println("Values Updated");
    WebSerial.println("Values Updated");
  }
  else{
    err++;
    delay(1000);
    Serial.println("Error: "+err);
    WebSerial.println("Error: "+err);
    updateVal();
  }
}

void sprinkOff(int num){
  if(num == 1){
    digitalWrite(LivSprink, LOW);
    livStat = 0;
    Serial.println("Living Room Sprinkler: Off");
    WebSerial.println("Living Room Sprinkler: Off");
  }
  if(num == 2){
    digitalWrite(BedSprink, LOW);
    bedStat = 0;
    Serial.println("Bed Room Sprinkler: Off");
    WebSerial.println("Bed Room Sprinkler: Off");
  }
  if(num == 3){
    digitalWrite(LivSprink, LOW);
    digitalWrite(BedSprink, LOW);
    livStat = 0;
    bedStat = 0;
    ThingSpeak.setField(1, 0);
    ThingSpeak.setField(2, 0);
    ThingSpeak.setField(3, 0);
    ThingSpeak.setField(4, 0);
    ThingSpeak.setField(5, 0);
    Serial.println("Living Room Sprinkler: Off");
    WebSerial.println("Living Room Sprinkler: Off");
    Serial.println("Bed Room Sprinkler: Off");
    WebSerial.println("Bed Room Sprinkler: Off");
    delay(15000);
  }
  updateVal();
}

void emerStop(){
  eStop = ThingSpeak.readIntField(SprinkChannelNumber, 1);
  if((livStat == 1 || bedStat == 1) && eStop == 1){
    Serial.println("Emergency Stop: Activated");
    WebSerial.println("Emergency Stop: Activated");
    sprinkOff(3);
  }
}

void sprinkOn(){
  if(livStat == 0 && LSprink == 1){
    digitalWrite(LivSprink, HIGH);
    initlivStat = millis();
    livStat = 1;
  }
  if(bedStat == 0 && BSprink == 1){
    digitalWrite(BedSprink, HIGH);
    initbedStat = millis();
    bedStat = 1;
  }
}

void delTime(){
  if(((millis()-initbedStat) >= BDelay && bedStat == 1)&&((millis()-initlivStat) >= LDelay && livStat == 1)){
    sprinkOff(3);
  }
  if((millis()-initbedStat) >= BDelay && bedStat == 1){
    sprinkOff(2);
  }
  if((millis()-initlivStat) >= LDelay && livStat == 1){
    sprinkOff(1);
  }
  if(livStat == 1 || bedStat == 1){
    emerStop();
    delTime();
  }
}

void weather(){
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println(F("Error reading Temperature!"));
    WebSerial.println(F("Error reading Temperature!"));
  }
  else {
    Serial.print(F("Temperature: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
    ThingSpeak.setField(6, event.temperature);
  }
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading Humidity!"));
    WebSerial.println(F("Error reading Humidity!"));
  }
  else {
    Serial.print(F("Humidity: "));
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
    ThingSpeak.setField(7, event.relative_humidity);
  }
  ThingSpeak.setField(8, (long)(millis()/1000));
  stat = ThingSpeak.writeFields(WeaChannelNumber, myWriteAPIKey2);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(client);
  pinMode(LivSprink, OUTPUT);
  pinMode(BedSprink, OUTPUT);
  digitalWrite(LivSprink, LOW);
  digitalWrite(BedSprink, LOW);
  dht.begin();
}

void loop() {
  
  int statusCode = 0;
  
  if(WiFi.status() != WL_CONNECTED){
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, pass);
      Serial.print(".");
      delay(5000);     
    } 
    Serial.println("\nConnected");
    Serial.println(WiFi.localIP());
    WebSerial.begin(&server);
    WebSerial.msgCallback(callback);
    server.begin();
    WebSerial.print("Successfully Connected to: ");
    WebSerial.println(ssid);
  }
  getValues();
  sprinkOn();
  delTime();
  emerStop();
  weather();
  Serial.println(eStop);
  Serial.println(LSprink);
  Serial.println(BSprink);
  Serial.println(LDelay);
  Serial.println(BDelay);
  Serial.println(livStat);
  Serial.println(bedStat);
  WebSerial.println(eStop);
  WebSerial.println(LSprink);
  WebSerial.println(BSprink);
  WebSerial.println(LDelay);
  WebSerial.println(BDelay);
  WebSerial.println(livStat);
  WebSerial.println(bedStat);
  delay(1000);
}
