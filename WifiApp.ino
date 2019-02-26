#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <Wire.h>

SoftwareSerial esp8266(2,3);

#define DEBUG true
#define STORAGE_LUX_ADDR 0
#define DS3231_I2C_ADDRESS 0x68

int fullRangePin = 9, coolWhiteLedPin = 10;
int relayPin1 = 12, relayPin2 = 8;
char okCode [] = "OK", ipdCode [] = "+IPD,", qMark [] = "?";
const unsigned int MAX_BRIGHTNESS = 255;

int ledBrightnessLevel = 0;
int light = 0;
int eeAddress = 0;
byte tminute = 0;


struct Led {
  int brightness, address, pin, maxBrightness;
};

struct Relay {
  int status, address, pin;
};

void* coolLed;
void* frLed;

void ledPin(void* ctrl)
{
  Led* led = (Led*)ctrl;
  pinMode(led->pin, OUTPUT);
}

void ledChangeBrightness(void *ctrl)
{
  Led* led = (Led*) ctrl;
  if(led->brightness >= 0 && led->brightness <= led->maxBrightness) {
    analogWrite(led->pin, led->brightness);
    delay(50); 
  }
}

void setupCoolLed()
{
  Led* led = new Led();
  led->pin = 10;
  led->address = 11;
  led->maxBrightness = MAX_BRIGHTNESS;
  ledPin(led);
  coolLed = led;

  retainer(led);
}

void setupFrLed()
{
  Led* led = new Led();
  led->pin = 9;
  led->address = 10;
  led->maxBrightness = 120;
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
    return;
  }
  
  if(value > led->brightness) {
    for(led->brightness; led->brightness <= value; led->brightness++) {
      ledChangeBrightness(led);
    }    
  }

  if(led->brightness > value) {
    for(led->brightness; led->brightness >= value; led->brightness--) {
      ledChangeBrightness(led);
    }
  }

  //Store the lux
  EEPROM.put(led->address, led->brightness);
}

int brightness(int value)
{
    if(value < 0)
      return 0;

    float b = (value * MAX_BRIGHTNESS) / 100;    
    return (int) b;
}

// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val){
  return( (val/10*16) + (val%10) );
}

// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val){
  return( (val/16*10) + (val%16) );
}

byte parseString(String inputString, int index) {
  return (byte)inputString[index] - 48;
}

byte buildTimeUnit(byte firstByte, byte secondByte) {
  return firstByte*10 + secondByte;
}

void serialDateParser(String inputString) 
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
   
  //@Todo check the lenght is correct
  Serial.println(inputString);  
  year = buildTimeUnit(parseString(inputString, 0), parseString(inputString, 1));
  month = buildTimeUnit(parseString(inputString, 2), parseString(inputString, 3));
  dayOfMonth = buildTimeUnit(parseString(inputString, 4), parseString(inputString, 5));
  dayOfWeek = parseString(inputString, 6);
  hour = buildTimeUnit(parseString(inputString, 7), parseString(inputString, 8));
  minute = buildTimeUnit(parseString(inputString, 9), parseString(inputString, 10));
  second = buildTimeUnit(parseString(inputString, 11), parseString(inputString, 12));   
  setDS3231time(second, minute, hour, dayOfWeek, dayOfMonth, month, year);
}

void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year) {
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(second)); // set seconds
  Wire.write(decToBcd(minute)); // set minutes
  Wire.write(decToBcd(hour)); // set hours
  Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(month)); // set month
  Wire.write(decToBcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}

void readDS3231time(byte *second,byte *minute,byte *hour,byte *dayOfWeek,byte *dayOfMonth,byte *month,byte *year) {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *second = bcdToDec(Wire.read() & 0x7f);
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read() & 0x3f);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
}

void bootstrap_clock()
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;  
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  tminute = minute;
}

void minutely()
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;  
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  if(minute != tminute) {
    tminute = minute;
    Serial.println("Change");
    Serial.print(minute, DEC);
    handleBrightness(coolLed, brightness(minute));
  }
  
}

void displayTime(){
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  // send it to the serial monitor
  Serial.print(hour, DEC);
  // convert the byte variable to a decimal number when displayed
  Serial.print(":");
  if (minute<10){
    Serial.print("0");
  }
  Serial.print(minute, DEC);
  Serial.print(":");
  if (second<10){
    Serial.print("0");
  }
  Serial.print(second, DEC);
  Serial.print(" ");
  Serial.print(dayOfMonth, DEC);
  Serial.print("/");
  Serial.print(month, DEC);
  Serial.print("/");
  Serial.print(year, DEC);
  Serial.print(" Day of week: ");
  switch(dayOfWeek){
  case 1:
    Serial.println("Sunday");
    break;
  case 2:
    Serial.println("Monday");
    break;
  case 3:
    Serial.println("Tuesday");
    break;
  case 4:
    Serial.println("Wednesday");
    break;
  case 5:
    Serial.println("Thursday");
    break;
  case 6:
    Serial.println("Friday");
    break;
  case 7:
    Serial.println("Saturday");
    break;
  }
}

//Setup code for the pin-outs/pin-ins
void setup()
{  
  Wire.begin();
  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  setupCoolLed();
  setupFrLed();  

  digitalWrite(relayPin1, HIGH); 
  digitalWrite(relayPin2, HIGH); 
  
  Serial.begin(9600);
  esp8266.begin(9600);

  bootstrap_clock();
  
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
  minutely();
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

      if(command == "sdt") {
        serialDateParser(valueStr);
      }

      if(command == "gdt") {
        displayTime();
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
