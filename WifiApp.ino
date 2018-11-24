#include <SoftwareSerial.h>
#include <EEPROM.h>

SoftwareSerial esp8266(2,3);

#define DEBUG true
#define STORAGE_LUX_ADDR 0

int ledPin = 10;
int relayPin1 = 12, relayPin2 = 8;
char okCode [] = "OK", ipdCode [] = "+IPD,", qMark [] = "?";
const unsigned int MAX_BRIGHTNESS = 255;

int ledBrightnessLevel = 0;
int light = 0;
int eeAddress = 0; 

//Setup code for the pin-outs/pin-ins
void setup()
{
  //Setup the pin as Output
  pinMode(ledPin, OUTPUT);
  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(relayPin1, HIGH); 
  digitalWrite(relayPin2, HIGH); 

  EEPROM.get(eeAddress, light);
  if(light > 0)
  {
    ledBrightnessLevel = changeBrightness(light, true);
  } else {
    changeBrightness(1, true);
  } 
  
  Serial.begin(9600);
  esp8266.begin(9600);  
  
  //reset module
  esp8266Data("AT+RST\r\n", 2000, DEBUG);

  //set station mode
  esp8266Data("AT+CWMODE=1\r\n", 1000, DEBUG);

  //connect wifi network
  esp8266Data("AT+CWJAP=\"Gigabyte Z97\",\"\"\r\n", 2000, DEBUG);

  //wait for connection
  while(!esp8266.find(okCode)) {} 

  //Setup the wifi module
  esp8266Data("AT+CIFSR\r\n", 1000, DEBUG); 
  esp8266Data("AT+CIPMUX=1\r\n", 1000, DEBUG); 
  esp8266Data("AT+CIPSERVER=1,80\r\n", 1000, DEBUG);

  digitalWrite(LED_BUILTIN, HIGH);  
}


void loop() 
{
  if (esp8266.available()) {
    if (esp8266.find(ipdCode)) {
      String msg;
      esp8266.find(qMark); 
      msg = esp8266.readStringUntil(' ');
      Serial.println(msg);
      String command = msg.substring(0, 3); 
      String valueStr = msg.substring(4);   
      int value = valueStr.toInt();      
      
      delay(100);
      
      //move servo1 to desired angle
      if(command == "led") {
        if(value == 0) {
           changeBrightness(0, true);
        }
        
        if(value == 1) {           
           changeBrightness(MAX_BRIGHTNESS, true);
        }                          
      }

      if(command == "rl1" && (value == 0 || value == 1)) {
        digitalWrite(relayPin1, value == 1 ? HIGH : LOW);
      }

      if(command == "rl2" && (value == 0 || value == 1)) {
        digitalWrite(relayPin2, value == 1 ? HIGH : LOW);
      }

      //Brightness as percentage
      if(command == "pct" && (value >= 0 && value <= 100)) {        
        changeBrightness(brightness(value), true);
      }

      //Brightness as direct analog
      if(command == "lux" && (value >= 0 && value <= MAX_BRIGHTNESS)) {
        changeBrightness(value, true);
      }
    }
  }
}


String esp8266Data(String command, const int timeout, boolean debug)
{
  String response = "";
  esp8266.print(command);
  long int time = millis();
  while ( (time + timeout) > millis())
  {
    while (esp8266.available())
    {
      char c = esp8266.read();
      response += c;
    }
  }
  if (debug)
  {
    Serial.print(response);
  }
  return response;
}

int brightness(int value) {
    if(value < 0)
      return 0;

    float b = (value * MAX_BRIGHTNESS) / 100;    
    return (int) b;
}

void changePWMLed()
{
    if(ledBrightnessLevel >= 0 && ledBrightnessLevel <= MAX_BRIGHTNESS) {
      analogWrite(ledPin, ledBrightnessLevel);
      delay(50); 
    }    
}

int changeBrightness(int newBrightnessLevel, bool store)
{
  if(newBrightnessLevel == ledBrightnessLevel) {
    return newBrightnessLevel;
  }
  
  if(newBrightnessLevel > ledBrightnessLevel) {
    return increaseBrightness(newBrightnessLevel, store);
  }

  if(ledBrightnessLevel > newBrightnessLevel) {
    return decreaseBrightness(newBrightnessLevel, store);
  }
}

int increaseBrightness(int newBrightness, bool store)
{
  for(ledBrightnessLevel; ledBrightnessLevel <= newBrightness; ledBrightnessLevel++) {
    changePWMLed();
  }

  if(store)
    storeLux();

  return ledBrightnessLevel;
}

int decreaseBrightness(int newBrightness, bool store)
{
  for(ledBrightnessLevel; ledBrightnessLevel >= newBrightness; ledBrightnessLevel--) {
    changePWMLed();
  }

  if(store)
    storeLux();
    
  return ledBrightnessLevel;
}

void storeLux()
{
  EEPROM.put(eeAddress, ledBrightnessLevel);
}
