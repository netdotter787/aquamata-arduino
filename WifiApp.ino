#include <SoftwareSerial.h>
#include <EEPROM.h>

//Setup the software serial communication on the ports specified

SoftwareSerial esp8266(2,3);

#define DEBUG true
#define STORAGE_LUX_ADDR 0

int fullRangePin = 9, coolWhiteLedPin = 10;
int relayPin1 = 12, relayPin2 = 8;
char okCode [] = "OK", ipdCode [] = "+IPD,", qMark [] = "?";
const unsigned int MAX_BRIGHTNESS = 255;

int ledBrightnessLevel = 0;
int light = 0;
int eeAddress = 0;

//Led Management Struct
struct Led {
  int brightness;
  int address;
  int pin;
};

//Relay Management Struct
struct Relay {
  int status;
  int address;
  int pin;
};

//The devices that will be used as LED
void* coolLed;
void* frLed;

//Used to setup the LED
void ledPin(void* ctrl) {
  Led* led = (Led*)ctrl;
  pinMode(led->pin, OUTPUT);
}

//Used to change the led brightness
void ledChangeBrightness(void *ctrl) {
  Led* led = (Led*) ctrl;
  if(led->brightness >= 0 && led->brightness <= MAX_BRIGHTNESS) {
    analogWrite(led->pin, led->brightness);
    _log("# PWM #%d : %d \n", led->pin, led->brightness);
    delay(50);
  }
}

//Setup the Cool White Led
void setupCoolLed()
{
  Led* led = new Led();
  led->pin = 10;
  led->address = 10;
  ledPin(led);
  coolLed = led;

  retainer(led);
}

//Setup the Full Range Led
void setupFrLed()
{
  Led* led = new Led();
  led->pin = 9;
  led->address = 10;
  ledPin(led);
  frLed = led;

  retainer(led);
}

void retainer(void* ctrl)
{
  Led* led = (Led*) ctrl;  
  EEPROM.get(led->address, light);  

  //Detect if there is settings
  if(light > 0)
  {
    handleBrightness(led, light);
  } else {
    handleBrightness(led, 1);
  }

  //Reset
  light = 0;
}

void handleBrightness(void *ctrl, int value)
{
  Led* led = (Led*) ctrl;
  if(value == led->brightness) {
    ledChangeBrightness(led);
    return;
  }

  if(value > led->brightness) {
    for(led->brightness; led->brightness <= value; ++led->brightness) {
      _log("#Pin%d ++++ - %d : %d \n", led->pin, value, led->brightness);
      ledChangeBrightness(led);
    }
    EEPROM.put(led->address, led->brightness);
    return;
  }

  if(led->brightness > value) {
    for(led->brightness; led->brightness >= value; --led->brightness) {
      _log("#Pin%d ---- - %d : %d \n", led->pin, value, led->brightness);
      ledChangeBrightness(led);
    }
    EEPROM.put(led->address, led->brightness);
    return;
  }
}

int brightness(int value) {
    if(value < 0)
      return 0;

    float b = (value * MAX_BRIGHTNESS) / 100;
    return (int) b;
}

int getBrightness(void *ctrl)
{
  Led* led = (Led*) ctrl;
  return led->brightness;
}

void _log(const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);
    Serial.print(buffer);
}

//Setup code for the pin-outs/pin-ins
void setup()
{
  Serial.begin(9600);
  esp8266.begin(9600);

  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  setupCoolLed();
  setupFrLed();

  digitalWrite(relayPin1, HIGH);
  digitalWrite(relayPin2, HIGH);

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
           handleBrightness(coolLed, 0);
        }

        if(value == 1) {
           handleBrightness(coolLed, MAX_BRIGHTNESS);
        }
      }

      if(command == "rl1" && (value == 0 || value == 1)) {
        digitalWrite(relayPin1, value == 1 ? HIGH : LOW);
      }

      if(command == "rl2" && (value == 0 || value == 1)) {
        digitalWrite(relayPin2, value == 1 ? HIGH : LOW);
      }

      //Brightness as percentage
      if(command == "ct1" && (value >= 0 && value <= 100)) {
        handleBrightness(coolLed, brightness(value));
      }

      if(command == "ct2" && (value >= 0 && value <= 100)) {
        handleBrightness(frLed, brightness(value));
      }

      //Brightness as direct analog
      if(command == "lx1" && (value >= 0 && value <= MAX_BRIGHTNESS)) {
        handleBrightness(coolLed, value);
      }

      //Brightness as direct analog
      if(command == "lx2" && (value >= 0 && value <= MAX_BRIGHTNESS)) {
        handleBrightness(frLed, value);
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