#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <MQTT.h>
#include <Hash.h>
#include <math.h>
#include "secrets.h" //<- move secrets.h.back to secrets.h and adjust the credentials

// define sound velocity in cm/uS
#define SOUND_VELOCITY 0.034
#define NOTIFICATION_STEP_VALUE 2
#define MIN_VALUE_ADDR 49
#define MAX_VALUE_ADDR 51

// Replace with your network credentials
const char *ssid = SECRET_WIFI_SSID;
const char *password = SECRET_WIFI_PASS;

const int trigPin = 12;
const int echoPin = 14;

WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266WebServer server(80); // instantiate server at port 80 (http port)
WiFiClient net;
MQTTClient mqttClient;

String page = "";
int LEDPin = 13;

int stepCounter;
long duration;
float distanceCm;
float distanceInch;
float percentage;
float currentLevel;
int maxLevel;
int minLevel;

bool mqttEnabled = false;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
int readCurrentLevel();
void connectMQTT();
void emitChanges(float newLevel);
void writeIntToEEPROM(int addr, int value);
int readIntFromEEPROM(int addr);

void setup(void)
{
  // the HTML of the web page
  page = "<h1>&#127754 SuperSonic Waterlevel Emitter &#127754</h1><div><p>WaterLevel: </p></div>";
  // make the LED pin output and initially turned off
  pinMode(LEDPin, OUTPUT);
  digitalWrite(LEDPin, LOW);

  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);  // Sets the echoPin as an Input
  currentLevel = -1;
  stepCounter = 0;

  delay(1000);

  Serial.begin(115200);
  WiFi.begin(ssid, password); // begin WiFi connection
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", []() { 
    server.send(200, "text/html", page); 
  });

  //update max value
  server.on("/max", HTTP_PUT, []() {
    if(server.arg("value") != ""){
      maxLevel = server.arg("value").toInt();
      writeIntToEEPROM(MAX_VALUE_ADDR, maxLevel);
      Serial.println("Updated MAX VALUE to: " + maxLevel);
    }
  });

  // get max value
  server.on("/max", HTTP_GET, []() {
        server.send(200, "application/json", "{\"maxValue\":\"" + String(maxLevel) + "\"}");
  });

  //update min value
  server.on("/min", HTTP_PUT, []() {
    if(server.arg("value") != ""){
      minLevel = server.arg("value").toInt();
      writeIntToEEPROM(MIN_VALUE_ADDR, minLevel);
      Serial.println("Updated MIN VALUE to: " + minLevel);
    }
  });

  // get min value
  server.on("/min", HTTP_GET, []() {
        server.send(200, "application/json", "{\"minValue\":\"" + String(minLevel) + "\"}");
  });

  minLevel = readIntFromEEPROM(MIN_VALUE_ADDR);
  Serial.println("Init MIN LEVEL value: " + minLevel);
  if(minLevel == 0){
    Serial.println("Due to 0 value, updated MIN LEVEL value to 20");
    minLevel = 20;
  }

  maxLevel = readIntFromEEPROM(MAX_VALUE_ADDR);
  Serial.println("Init MAX LEVEL value: " + maxLevel);
  if(maxLevel == 0){
    Serial.println("Due to 0 value, updated MAX LEVEL value to 69");
    maxLevel = 69;
  }

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  if(strcmp(MQTT_BROKER_ADDR, "") != 0){
    mqttClient.begin(MQTT_BROKER_ADDR, net);
    mqttClient.setWill("water-level-emitter/status", "offline");

    Serial.println("Connecting to MQTT brocker...");
    connectMQTT();
    mqttEnabled = true;
    Serial.println("Connection to MQTT brocker established");
  }

  Serial.println("Device started!");
}

void loop(void)
{
  webSocket.loop();
  server.handleClient();
  if(!mqttClient.connected() && mqttEnabled){
    Serial.println("MQTT Broker connection lost, reconecting..");
    connectMQTT();
  }
  if (Serial.available() > 0)
  {
    char c[] = {(char)Serial.read()};
    webSocket.broadcastTXT(c, sizeof(c));
  }
  if (stepCounter == 50000)
  {
    //Serial.println("Messung");
    int newLevel = readCurrentLevel();
    emitChanges(newLevel);
    stepCounter = 0;
  }
  stepCounter++;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  if (type == WStype_TEXT)
  {
    for (size_t i = 0; i < length; i++)
      Serial.print((char)payload[i]);
    Serial.println();
  }
}

void connectMQTT(){
  while (!mqttClient.connect("water-level-emitter", "public", "public"))
  {
    Serial.print(".");
    delay(1000);
  }
  mqttClient.publish("water-level-emitter/status", "online");
  mqttClient.publish("water-level-emitter/level", String((int) currentLevel));
}

int readCurrentLevel()
{
  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);

  // Calculate the distance
  distanceCm = duration * SOUND_VELOCITY / 2;

  percentage = 100 / (maxLevel - minLevel) * (distanceCm - minLevel);
  if (percentage <= 0)
  {
    percentage = 0;
  }
  if (percentage >= 100)
  {
    percentage = 100;
  }
  return percentage;
}

void emitChanges(float newLevel)
{
  if (newLevel > currentLevel + NOTIFICATION_STEP_VALUE || newLevel < currentLevel - NOTIFICATION_STEP_VALUE)
  {
    currentLevel = newLevel;
    page = "<h1>&#127754 SuperSonic Waterlevel Emitter &#127754</h1><div><p>WaterLevel: " + String((int)currentLevel) + "% </p></div>";
    String eventMsg = "Event:NewLevel:" + String((int)currentLevel);
    webSocket.broadcastTXT(eventMsg);
    if(mqttEnabled){
      mqttClient.publish("water-level-emitter/level", String((int) currentLevel));
    }
    Serial.print("New Value is: ");
    Serial.println(currentLevel);
  }
}

void writeIntToEEPROM(int addr, int value){
  EEPROM.write(addr, value >> 8);
  EEPROM.write(addr + 1, value & 0xFF);
}

int readIntFromEEPROM(int addr) {
  return (EEPROM.read(addr) << 8) + EEPROM.read(addr + 1);
}