#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <math.h>

//define sound velocity in cm/uS
#define SOUND_VELOCITY 0.034
#define MIN_LEVEL 20
#define MAX_LEVEL 69
#define NOTIFICATION_STEP_VALUE 2

// Replace with your network credentials
const char* ssid = "####";
const char* password = "###";

const int trigPin = 12;
const int echoPin = 14;

WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266WebServer server(80);   //instantiate server at port 80 (http port)

String page = "";
int LEDPin = 13;

int stepCounter;
long duration;
float distanceCm;
float distanceInch;
float percentage;
float currentLevel;

void setup(void){
    //the HTML of the web page
    page = "<h1>&#127754 SuperSonic Waterlevel Emitter &#127754</h1><div><p>WaterLevel: </p></div>";
    //make the LED pin output and initially turned off
    pinMode(LEDPin, OUTPUT);
    digitalWrite(LEDPin, LOW);

    pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
    pinMode(echoPin, INPUT); // Sets the echoPin as an Input
    currentLevel = -1;
    stepCounter = 0;


    delay(1000);

    Serial.begin(115200);
    WiFi.begin(ssid, password); //begin WiFi connection
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    server.on("/", [](){
        server.send(200, "text/html", page);
    });

    server.on("/LEDOn", [](){
        server.send(200, "text/html", page);
        digitalWrite(LEDPin, HIGH);
        delay(1000);
    });

    server.on("/LEDOff", [](){
        server.send(200, "text/html", page);
        digitalWrite(LEDPin, LOW);
        delay(1000);
    });

    server.begin();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    Serial.println("Web server started!");
}

void loop(void){
    webSocket.loop();
    server.handleClient();
    if (Serial.available() > 0){
        char c[] = {(char)Serial.read()};
        webSocket.broadcastTXT(c, sizeof(c));
    }
    if(stepCounter == 50000){
      Serial.println("Messung");
      int newLevel = readCurrentLevel();
      emitChanges(newLevel);
      stepCounter = 0;
    }
    stepCounter++;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  Serial.println("New Event:");
  if (type == WStype_TEXT){
   for(int i = 0; i < length; i++) Serial.print((char) payload[i]);
   Serial.println();
  }
}

int readCurrentLevel(){
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
  distanceCm = duration * SOUND_VELOCITY/2;

  percentage = 100 / (MAX_LEVEL - MIN_LEVEL) * (distanceCm - MIN_LEVEL);
  if(percentage <= 0){
    percentage = 0;
  }
  if(percentage >= 100){
    percentage = 100;
  }
  //delay(1000);
  return percentage;
}

void emitChanges(float newLevel){
    if(newLevel > currentLevel + NOTIFICATION_STEP_VALUE || newLevel < currentLevel - NOTIFICATION_STEP_VALUE){
        currentLevel = newLevel;
        page = "<h1>&#127754 SuperSonic Waterlevel Emitter &#127754</h1><div><p>WaterLevel: " + String((int) currentLevel) + "% </p></div>";
        String eventMsg = "Event:NewLevel:" + String((int) currentLevel);
        webSocket.broadcastTXT(eventMsg);
        Serial.print("New Value is: ");
        Serial.println(currentLevel);
    }
}