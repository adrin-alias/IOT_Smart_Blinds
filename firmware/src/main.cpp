
#include <Arduino.h>
#include <WiFi.h>

#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "ESP32time.h"
#include <time.h>
#include <stdio.h>
#include <HTTPClient.h>
#include <AccelStepper.h>

//--Wifi Info---------------------------------------------
const char* ssid = "Silence of the LANs";
const char* password = "UpTill500";

//--Time Sync------------------------------------
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -6*60*60; //UTC -6:00
const int   daylightOffset_sec = 3600;
const char *sunsetTime;
const char *sunriseTime;

JsonDocument doc_in;
JsonDocument doc_out;

//--Stepper Setup-------------------------------
#define FULLSTEP 4 // Step constant
#define STEP_PER_REVOLUTION 2048 
#define IN1 19
#define IN2 18
#define IN3 5
#define IN4 17
#define LED 2

#define stepMaxSpeed 500
#define stepAccel 40
#define stepSpeed 40
#define stepPosition 2048

AccelStepper stepper(FULLSTEP, IN1, IN3, IN2, IN4);
// IP Information Sharing

//---------WebSocket------------------------------
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
String JSONtxt;
#include "html_page.h"

//-----Google Sheets------------------------------

String GOOGLE_SCRIPT_ID = "AKfycbx3Q_dHLv4i_coWAssFhswI4JGm6S-k9aLFN0IdUQjaKFiESlyph6qhDhNewuSgmgxtAg";
const char* GOOGLE_LOG_FORM = "https://docs.google.com/forms/d/16pDtK5Na7b-2Hi1e2-_TiwdG583Q4NgWJ2Q2ve6x-AU/formResponse?entry.2079171063=";

//-- Operation------------------------------------
boolean LEDon=false;
int interval = 1000; // in ms
unsigned long timeToSwitchState = 0 ;
boolean isBlindsOpen = true;
unsigned long previousMillis1 = 0;
int i = 0;
char currentTime[100];
int stepperDirection = 0;
int closeLocation = 6.2 * stepPosition; // Negative is move move down
int timeTillSunset;
int timeTllSunrise;

ESP32Time rtc(0);

void sendChatBotMessage(const char *IPaddress) {
  const char *APIstart = "https://api.callmebot.com/whatsapp.php?phone=16823060471&text=";
  const char *APIend = "&apikey=5299213";
  HTTPClient http;
  char finalURL[150];   // array to hold the result.

strcpy(finalURL,APIstart); // copy string one into the result.
strcat(finalURL,IPaddress); // append string two to the result.
strcat(finalURL,APIend);
  // const char *finalURL = strcat(strcat(APIstart, IPaddress), APIend);
  // const char *finalURL = APIstart+IPaddress+APIend);
  http.begin(finalURL);
  // Serial.println(finalURL);
  int httpCode = http.GET();
  if (httpCode > 0) {       //Check for the returning code
    // String payload = http.getString();
    // Serial.println(httpCode);
    // Serial.println(payload);
  }
  else {
    Serial.println("Error on HTTP request for chatbot");
  }
  http.end();
}

void sendGoogleInitiaizationLog(const char *IPaddress) {
  HTTPClient http;
  char finalURL[150];   // array to hold the result.

strcpy(finalURL,GOOGLE_LOG_FORM); // copy string one into the result.
strcat(finalURL,IPaddress); // append string two to the result.


  http.begin(finalURL);
  
  int httpCode = http.GET();
  if (httpCode > 0) {       //Check for the returning code
    // String payload = http.getString();
    // Serial.println(httpCode);
    // Serial.println(payload);
  }
  else {
    Serial.println("Error on HTTP sending Google log");
  }
  http.end();
}

void setLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  const struct tm timeinfo_const = timeinfo;
  rtc.setTimeStruct(timeinfo);
  //const char currentTime_const[100];

  strftime(currentTime, 100, "%d-%b-%Y %H:%M", &timeinfo_const);
  //Serial.printf("The current time is: %s\n", currentTime);
}

void setSunriseSunset() {
  const char *sunserverurl = "https://api.sunrisesunset.io/json?lat=32.73569&lng=-97.10807"; // link to sunrisesunset.io
  HTTPClient http;
  http.begin(sunserverurl);
  int httpCode = http.GET();
  if (httpCode > 0) {       //Check for the returning code
    String payload = http.getString();
    //Serial.println(httpCode);
    //Serial.println(payload);
    JsonDocument sunServerData;
    deserializeJson(sunServerData, payload);
    sunriseTime = sunServerData["results"]["sunrise"];
    sunsetTime = sunServerData["results"]["sunset"];
    Serial.print("\nSunrise is ");
    Serial.println(sunriseTime);
    Serial.print("Sunset is ");
    Serial.println(sunsetTime);
    //timeTillSunset = rtc.getHour()-strtok(sunsetTime,":")

  }
  else {
    Serial.println("Error on HTTP request");
  }
  http.end();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t welength) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("A client disconnected");
      digitalWrite(LED, LOW);
      break;
    case WStype_CONNECTED: 
      Serial.println("A client connected");
      digitalWrite(LED, HIGH);
      break;

    case WStype_TEXT: // resonse from a client...
      DeserializationError error = deserializeJson(doc_in, payload);
      if(error) {
        Serial.print(F("Deserialization Failed"));
        Serial.println(error.f_str());
        return;
      }
      else {
        digitalWrite(LED, !digitalRead(LED));
        delay(500);
        digitalWrite(LED, !digitalRead(LED));
        const char *opentime = doc_in["openTime"];
        const char *closetime = doc_in["closeTime"];
        const int direction = doc_in["rotate"];
        const int setOpenCloseValues = doc_in["savePosition"];
        // Only using the values from open to switch states.

        int OpenHour = (int)(String(opentime)).substring(0, String(opentime).indexOf(':')).toInt();
        int OpenMin = (int)(String(opentime)).substring(String(opentime).indexOf(':')+1).toInt();
        
        if (!(OpenHour == 0 && OpenMin == 0)) {
          Serial.println("Open \t" + String(opentime));
          Serial.println("Close \t" + String(closetime));
          if(isBlindsOpen) {
            stepper.moveTo(closeLocation);
            Serial.printf("\nThe blinds will close in %02iH and %02iM.\n", OpenHour, OpenMin);
          }
          else {
            stepper.moveTo(0);
            Serial.printf("\nThe blinds will open in %02iH and %02iM.\n", OpenHour, OpenMin);
          }
          // setting time to switch states:
          timeToSwitchState = millis() + OpenHour * (3600000) + OpenMin * (60000);
        }

        switch (direction)
        {
        case 1: //closing
          stepperDirection = 1;
          stepper.moveTo(closeLocation);
          Serial.println("Rotating Up - Closing");
          break;
          case -1: //opening
          stepperDirection = -1;
          stepper.moveTo(-closeLocation);
          Serial.println("Rotating Down - Opening");
          break;
        case 2:
          stepperDirection = 0;
          stepper.stop();
          stepper.moveTo(stepper.currentPosition() + 1);
          stepper.disableOutputs();
          Serial.println("Stopping Rotation");
          break;
        default:
          break;
        }
        switch (setOpenCloseValues) {
          case 1:
            stepper.setCurrentPosition(0);
            Serial.println("Set Open Position.");
            isBlindsOpen = true;
            stepper.disableOutputs();

            break;
          case -1:
            closeLocation = stepper.currentPosition();
            Serial.printf("Set %i as Close Position.", closeLocation);
            isBlindsOpen = false;
            stepper.disableOutputs();

            break;
          default:
            break;
        }      
      }
  }
}

void readGoogleSheet() {
  HTTPClient http;
  String url="https://script.google.com/macros/s/"+GOOGLE_SCRIPT_ID+"/exec?read=B1";
  // Serial.print(url);
  // Serial.println("Reading Data From Google Sheet.....");
  http.begin(url.c_str());

  //Removes the error "302 Moved Temporarily Error"
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  //Get the returning HTTP status code

  int httpCode = http.GET();
  // Serial.print("HTTP Status Code: ");
  // Serial.println(httpCode);

  if(httpCode <= 0){Serial.println("Error on HTTP request"); http.end(); return;}


  String payload = http.getString();
  // Serial.println("Payload: "+payload);

  if(payload == "1" && isBlindsOpen) //Close the blinds
  {
    timeToSwitchState = 100;
    stepperDirection = 1;
          stepper.moveTo(closeLocation);
    Serial.println("GS - Closing Blinds");
    isBlindsOpen = false;
  };
  if (payload == "0" && !isBlindsOpen) // Open the blinds
      {
        timeToSwitchState = 100;
        stepperDirection = -1;
          stepper.moveTo(0);
        Serial.println("GS - Opening Blinds");
        isBlindsOpen = true;
      };
}

void setup()
{
  // Serial Setup
  Serial.begin(115200);

  //LED Setup
  pinMode(LED, OUTPUT);
  
  // Wifi Setup
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
    Serial.print("."); delay(500);
    digitalWrite(LED, !digitalRead(LED));
    }
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP()); 
  //sendChatBotMessage(WiFi.localIP().toString().c_str()); //sending IP address
  sendGoogleInitiaizationLog(WiFi.localIP().toString().c_str());

  // Websocket Server stuff
  server.on("/", []()
            {
              server.send(200, "text/html", webpageCode);
            }); // calling the HTML from html_page.h
  server.begin(); //starting server

  webSocket.begin(); //starting websocket
  webSocket.onEvent(webSocketEvent);
  //Serial.print("Server Online\n");

  // Time Sync
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); 
  //setLocalTime();
  // Getting Sunset and Sunrise times
 // setSunriseSunset();
  
  // Stepper Setup
  stepper.setMaxSpeed(stepMaxSpeed);  // set the maximum speed
  stepper.setAcceleration(stepAccel); // set acceleration
  stepper.setSpeed(stepSpeed);        // set initial speed
  stepper.setCurrentPosition(0);      // set position
  stepper.moveTo(0);
  
  // readGoogleSheet();

  Serial.print("Setup Finish\n\n\n\n");
}

void loop()
{
  server.handleClient(); 
  webSocket.loop();

  unsigned long now = millis();
  // Serial.print(rtc.getHour());
  // Serial.print(":");
  // Serial.print(rtc.getMinute());
  // Serial.print(":");
  // Serial.print(rtc.getSecond());
  // Serial.print("\n");
  // if (now - previousMillis1 > interval) {
  //   String jsonString = "";
  //   JsonObject object = doc_out.to<JsonObject>();
  //   object["Status"] = 0;
  //   setLocalTime();
  //   String currentTimeString = currentTime;
  //   object["Time"] = currentTime;
  //   serializeJson(doc_out, jsonString);
  //   //Serial.println(jsonString);
  //   webSocket.broadcastTXT(jsonString);

  //   previousMillis1 = now;
  //   //setLocalTime();
  //   long millisRemaining = (long)(timeToSwitchState - now);
  //   if(millisRemaining>0)
  //   {
  //     //Serial.println((long)(timeToSwitchState - now));
  //     unsigned long seconds = millisRemaining / 1000;
  //     unsigned long minutes = seconds / 60;
  //     unsigned long hours = minutes / 60;
  //     millisRemaining %= 1000;
  //     seconds %= 60;
  //     minutes %= 60;
  //     hours %= 24;
  //     Serial.printf ("Time Left: %02i:%02i:%02i\n", (int) hours, (int) minutes, (int) seconds);
  //     stepper.moveTo(1);
  //   }
  //   else 
  //     Serial.printf ("Time Left: 00:00:00\n");
  // }

  if (stepperDirection !=0) {
    stepper.run();
  }

  if (stepper.distanceToGo() == 0) {
    if((now - previousMillis1) > interval) {
      // Serial.print("Reading GS... at ");
      // Serial.println(now);
      readGoogleSheet();
      previousMillis1 = now;
    }
    stepper.disableOutputs();
  }

  if (((long)(timeToSwitchState-now) == 0 && stepper.distanceToGo() != 0)) {
    if(!isBlindsOpen) { //If it's not open
          Serial.println("Opening");
          
        }
        else {
          Serial.println("Closing");
          stepper.moveTo(closeLocation); 
        }

    do {
      stepper.run();
    } while (stepper.distanceToGo() != 0);

    isBlindsOpen = !isBlindsOpen;
    //Preventing Overheating
    stepper.disableOutputs();
  }
}

