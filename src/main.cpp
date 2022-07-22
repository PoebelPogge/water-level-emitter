#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <math.h>
#include "secrets.h"

//define sound velocity in cm/uS
#define SOUND_VELOCITY 0.034
#define MIN_LEVEL 20
#define MAX_LEVEL 69
#define NOTIFICATION_STEP_VALUE 2

// Replace with your network credentials
const char* ssid = SECRET_WIFI_SSID;
const char* password = SECRET_WIFI_PASS;

const int trigPin = 12;
const int echoPin = 14;

WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266WebServer server(80);   //instantiate server at port 80 (http port)

String version = "0.0.1-alpha";

String page = "";

String localIP = "";

int LEDPin = 13;

int stepCounter;
long duration;
float distanceCm;
float distanceInch;
float percentage;
float currentLevel;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
int readCurrentLevel();
void emitChanges(float newLevel);
void updateHTML(int newLevel);

void setup(void){
    //the HTML of the web page
    //page = "<h1>&#127754 SuperSonic Waterlevel Emitter &#127754</h1><div><p>WaterLevel: </p></div>";
    updateHTML(0);
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
    localIP = WiFi.localIP().toString();
    Serial.println(localIP);

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
      updateHTML((int) newLevel);
      stepCounter = 0;
    }
    stepCounter++;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
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
        String eventMsg = "Event:NewLevel:" + String((int) currentLevel);
        webSocket.broadcastTXT(eventMsg);
        Serial.print("New Value is: ");
        Serial.println(currentLevel);
    }
}

void updateHTML(int newLevel){
  String value = "\
                <!DOCTYPE html>\n\
                <html>\n\
                \n\
                <head>\n\
                    <link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.0.2/dist/css/bootstrap.min.css\" rel=\"stylesheet\"\n\
                        integrity=\"sha384-EVSTQN3/azprG1Anm3QDgpJLIm9Nao0Yz1ztcQTwFspd3yD65VohhpuuCOmLASjC\" crossorigin=\"anonymous\">\n\
                    <title>Smarty42</title>\n\
                    <style>\n\
                        body {\n\
                            background-color: rgb(87, 0, 124);\n\
                            color: white;\n\
                        }\n\
                        \n\
                        .version {\n\
                            position: absolute;\n\
                            bottom: 0;\n\
                            right: 0;\n\
                        }\n\
                    </style>\n\
                    <script>\n\
                      let target = \"ws://IPADRESS:81\";\n\
                      console.log(\"Connecting to: \" + target);\n\
                      webSocket = new WebSocket(target);\n\
                      webSocket.onopen = function(event) {\n\
                        console.log(\"Connection established\");\n\
                      }\n\
                      webSocket.onmessage = function (event) {\n\
                          let message = event.data;\n\
                          newValue = message.split(\":\")[2];\n\
                          console.log(\"New level: \" + newValue)\n\
                          document.getElementById(\"pgbar\").style.width = newValue + \"%\";\n\
                          document.getElementById(\"pgbar\").innerHTML = newValue + \"%\";\n\
                      }\n\
                  </script>\n\
                </head>\n\
                \n\
                <body>\n\
                    <div class=\"container\">\n\
                        <div class=\"d-flex flex-column min-vh-100 justify-content-center align-items-center\">\n\
                            <h1>Smarty42</h1>\n\
                            <p>Water Level Emitter</p>\n\
                            <div class=\"progress\" style=\"width: 200px\">\n\
                                <div id=\"pgbar\" class=\"progress-bar progress-bar-striped progress-bar-animated\" role=\"progressbar\" aria-valuenow=\"LEVEL\" aria-valuemin=\"0\" aria-valuemax=\"100\" style=\"width: LEVEL%\">LEVEL%</div>\n\
                            </div>\n\
                        </div>\n\
                        <div class=\"version\">\n\
                            Version: VERSION\n\
                        </div>\n\
                    </div>\n\
                </body>\n\
                </html>\n\
                ";
  value.replace("LEVEL", String(newLevel));
  value.replace("VERSION", String(version));
  value.replace("IPADRESS",localIP);
  page = value;
}