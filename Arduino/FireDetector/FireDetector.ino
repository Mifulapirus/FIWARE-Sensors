/****************************************
* FIWARE Sensors
* Fire detector
* Version = 0.2
* Attached peripherals:
*  - Hall effect: A3144 on pin D2
*  - ESP8266: TX-12, RX-13
*****************************************/
#include <stdlib.h>
#include <SoftwareSerial.h>
//#include <Wire.h>

/*******************************
* General variables
*******************************/
#define FirmwareVersion "0.1"
#define DeviceCategory "FD" //FIWARE-Fire detector";
String DeviceID = "\"FireSensor_1\""; 
#define DeviceType "\"FireSensor\""
long KeepAliveDelay = 1000;
long PreviousKeepAlive = 0;


/*******************************
* WiFi Stuff
*******************************/
//#define SSID "Interne"
//#define PASS "Mec0mebien"
#define SSID "bamboo_wifi"
#define PASS "paloverde"
SoftwareSerial WiFiSerial(2, 3); // RX, TX
String OwnIP = "000.000.000.000";
String WiFiLongMessage;

//----------------------
//Messages sent
#define ResponseOK     "OK"
#define Error          "ERROR "
  #define ErrorUnableToLink             "1"
  #define SendLongMessageError          "2"  
  #define ErrorModuleDoesntRespond      "3"
  #define ErrorModuleDoesntRespondToAT  "4"
  #define ErrorResponseNotFound         "5"
  #define ErrorUnableToConnect          "6"
  
#define CIPSTART       "AT+CIPSTART=\"TCP\",\""


/*************************************
* FIWARE Stuff
*-------------------------------------
* Server to be used: 130.206.127.115
* Port: 1026
* Protocol: NGSI10
* Service: updateContext
* Final URL: "http://130.206.127.115:1026/NGSI10/updateContext"
--------------------------------------
* Typical message
*-------------------------------------
* Accept:application/json
* Accept-Encoding:deflate
* Cache-Control:no-cache
* Content-Length:675
* Content-Type:application/x-www-form-urlencoded; charset=UTF-8
* {"contextElements": [
*   {"type": "FireSensor",
*    "isPattern": "false",
*    "id": "Sensor1",
*    "attributes": [
*      {"name": "thereisfire",
*       "type": "bool",
*       "value": "0"
*      }]
*  }],
*  "updateAction": "UPDATE"
* }
* /EOF
*------------------------------
* Adding special characters, each string should be as the following:
* "Accept:application/json\r\n"
* "Accept-Encoding:deflate\r\n"
* "Cache-Control:no-cache\r\n"
* "Content-Length:675\r\n"
* "Content-Type:application/x-www-form-urlencoded; charset=UTF-8\r\n"
* "{\"contextElements\":[{\"type\":\"FireSensor\",\"isPattern\":\"false\",\"id\":\"Sensor1\",\"attributes\":[{\"name\":\"thereisfire\",\"type\":\"bool\",\"value\":\"0\"}]}],\"updateAction\":\"UPDATE\"}\r\n\0"
********************************/
#define CBIP "130.206.127.115"
#define CBPort "1026"

#define CBStaticHeader "POST /NGSI10/updateContext\r\nAccept:application/json\r\nAccept-Encoding:deflate\r\nCache-Control:no-cache\r\nContent-Type: application/json\r\nContent-Length:"
String CBElement_Length = "169";
#define CBBody1 "\r\n\r\n{\"contextElements\":[{\"type\":" //\"FireSensor\"
String CBElement_Type = DeviceType;
#define CBBody2 ",\"isPattern\":\"false\",\"id\":"//\"Sensor1\"
String CBElement_ID = DeviceID;
#define CBBody3 ",\"attributes\":[{\"name\":" //\"thereisfire\"
String CBElement_Att_1_Name = "\"FireDetected\"";
#define CBBody4 ",\"type\":" //\"bool\"
String CBElement_Att_1_Type = "\"bool\"";
#define CBBody5 ",\"value\":" //\"0\"
String CBElement_Att_1_Value = "\"0\"";
#define CBBody6 "},{\"name\":"
String CBElement_Att_2_Name = "\"FirePosition\"";
#define CBBody7 ",\"type\":" //\"bool\"
String CBElement_Att_2_Type = "\"int\"";
#define CBBody8 ",\"value\":" //\"0\"
String CBElement_Att_2_Value = "\"0\"";
#define CBBody9 "}]}],\"updateAction\":\"APPEND\"}\r\n"

#define AttValue_1 "\"1\""
#define AttValue_0 "\"0\""

#define CBPostOK "\"code\" : \"200\""

long CBLastUpdate = 0;
long CBRegularUpdateDelay = 60000;  //60 sec
long CBMinUpdateDelay = 60000;         //10 seconds
long CBUpdateDelay = CBRegularUpdateDelay;

/******************************
* Pin definition
******************************/
const int PIRPin = 12;       // Hall Effect Sensor pin
const int LedPin = 13;      // Onboard LED pin

/****************************
* Setup
****************************/
void setup() {
  pinMode(LedPin, OUTPUT);
  pinMode(PIRPin, INPUT);
  
  Serial.begin(9600);
  WiFiSerial.begin(9600);
  
  Serial.println(DeviceType);
  Serial.println(DeviceCategory);
  Serial.print("V. ");
  Serial.println(FirmwareVersion);
  Serial.print("ID: ");
  Serial.println(DeviceID);
  //Attach the interruption
//  attachInterrupt(0, FireDetected, RISING);
  WiFiLongMessage.reserve(400);
  
  InitWiFi();
  //WiFiEcho();
  UpdateCB();
}

/************************************
* General Functions
************************************/
void loop() {
  PeriodicUpdate();  //Just Update the sensor, so we know it is alive
  //Check if there is fire and Update
  if (digitalRead(PIRPin)) {FireDetected();}

}

void PrintError(char* ErrorMessage) {
  Serial.print(Error);
  Serial.println(ErrorMessage);
  }


/****************************
* Fire Interruption CallBack
*****************************/
void FireDetected() {
  long CurrentTime = millis();
  digitalWrite(LedPin, HIGH);
  Serial.print (CurrentTime);
  Serial.println(" - FIRE!");
  CBElement_Att_1_Value=AttValue_1;
  UpdateCB();
  CBElement_Att_1_Value=AttValue_0;
  delay(5000);
  digitalWrite(LedPin, LOW);
}

/************************************
* Context Broker functions
************************************/
void UpdateCB() {
  Serial.print("CB @ ");
  Serial.println(millis());
  while (!OpenTCP(CBIP, CBPort)) {}
  delay(1000);
  //Calculate CBElement_Length
  CBElement_Length = String(sizeof(CBBody1)-2
                     + CBElement_Type.length() 
                     + sizeof(CBBody2)-2 
                     + CBElement_ID.length() 
                     + sizeof(CBBody3)-2 
                     + CBElement_Att_1_Name.length() 
                     + sizeof(CBBody4)-2 
                     + CBElement_Att_1_Type.length() 
                     + sizeof(CBBody5)-2 
                     + CBElement_Att_1_Value.length() 
                     + sizeof(CBBody6)-2
                     + CBElement_Att_2_Name.length() 
                     + sizeof(CBBody7)-2
                     + CBElement_Att_2_Type.length() 
                     + sizeof(CBBody8)-2
                     + CBElement_Att_2_Value.length() 
                     + sizeof(CBBody9)-2);
                     
                     
  WiFiLongMessage = CBStaticHeader;
  WiFiLongMessage += CBElement_Length;
  WiFiLongMessage +=  CBBody1;
  WiFiLongMessage += CBElement_Type;
  WiFiLongMessage += CBBody2;
  WiFiLongMessage += CBElement_ID;
  WiFiLongMessage += CBBody3;
  WiFiLongMessage += CBElement_Att_1_Name;
  WiFiLongMessage += CBBody4;
  WiFiLongMessage += CBElement_Att_1_Type;
  WiFiLongMessage += CBBody5;
  WiFiLongMessage += CBElement_Att_1_Value;
  WiFiLongMessage += CBBody6;
  WiFiLongMessage += CBElement_Att_2_Name;
  WiFiLongMessage += CBBody7;
  WiFiLongMessage += CBElement_Att_2_Type;
  WiFiLongMessage += CBBody8;
  WiFiLongMessage += CBElement_Att_2_Value;
  WiFiLongMessage += CBBody9;
  
  SendLongMessage(CBPostOK);
  CloseTCP();
  CBLastUpdate = millis();  
}

void PeriodicUpdate() {
  long Now = millis();
  if (Now > CBLastUpdate + CBUpdateDelay) {
    UpdateCB();
  }
  else if (Now > PreviousKeepAlive + KeepAliveDelay) {
    digitalWrite(LedPin, HIGH);
    Serial.print(".");
    digitalWrite(LedPin, LOW);
    PreviousKeepAlive = Now;
  }
}

/***********************************
* WiFi Functions
***********************************/
boolean OpenTCP(String IP, String Port) {
  String cmd = CIPSTART; 
  cmd += IP;
  cmd += "\",";
  cmd += Port;
  SendDebug(cmd);
  if (!ExpectResponse("Linked")) {
    PrintError(ErrorUnableToLink);
    return false;
  }
  return true;
}

boolean SendLongMessage(char* ExpectedReply) {
  Serial.println("Sending: ");
  Serial.println(WiFiLongMessage);
  WiFiSerial.print("AT+CIPSEND=");
  WiFiSerial.println(WiFiLongMessage.length());
  delay(200); 
  WiFiSerial.print(WiFiLongMessage);
  if (ExpectResponse(ExpectedReply)) {
    Serial.println(ResponseOK);
    return true;
    }
  else {
    PrintError(SendLongMessageError);
    return false;
    }
}

void WiFiEcho() {
  while(1) {
    if (WiFiSerial.available())
      Serial.write(WiFiSerial.read());
    if (Serial.available())
      WiFiSerial.write(Serial.read());
  }
}

void SendDebug(String cmd) {
  WiFiSerial.flush();
  Serial.print("Sent: ");
  Serial.println(cmd);
  WiFiSerial.println(cmd);
}

boolean CloseTCP() {
  SendDebug("AT+CIPCLOSE");
  if (!ExpectResponse("Unlink")) {CheckWiFi();}
}


boolean InitWiFi() {
  while (!WiFiReboot())  {}
  //while (!CheckWiFi())   {}
  while (!ConnectWiFi()) {}
  SetCIPMODE(false);
  return true;
}

boolean WiFiReboot() {
  WiFiSerial.flush();
  Serial.println("Reboot");
  WiFiSerial.println("AT+RST"); // restet and test if module is redy
  if (ExpectResponse("Ready")) {
    return true;  
  }
  else {
    PrintError(ErrorModuleDoesntRespond);
    return false;
  }
}

boolean CheckWiFi() {
  SendDebug("AT");
  if (ExpectResponse(ResponseOK)) {
    Serial.println(ResponseOK);
    return true;
  }
  else {
    PrintError(ErrorModuleDoesntRespondToAT);
    InitWiFi();
    return false;
  }
}

boolean ExpectResponse(char* _Expected) {
  Serial.print("Waiting for ");
  Serial.print(_Expected);
  Serial.print(": ");

  for (int i = 0; i < 10; i++) {
    if (WiFiSerial.find(_Expected)) {
      Serial.println(ResponseOK);
      return true;
    }

    else {
      //TODO: Do something if not found yet?
      Serial.print(".");
    }
    delay(1);
  }
  PrintError(ErrorResponseNotFound);
  return false;
}

String GetIP() {
  WiFiSerial.flush();
  WiFiSerial.println("AT+CIFSR"); // Get IP
  OwnIP = "";
  char incomingByte = ' ';
  delay(100);
  while (WiFiSerial.available() > 0) {
    // read the incoming byte:
    incomingByte = WiFiSerial.read();
    // say what you got:
    OwnIP += incomingByte;
    delay(1);
  }
  return OwnIP;
}

boolean ConnectWiFi() {
  WiFiSerial.flush();
  Serial.println("Connecting");
  WiFiSerial.println("AT+CWMODE=1");
  delay(2000);
  String cmd = "AT+CWJAP=\"";
  cmd += SSID;
  cmd += "\",\"";
  cmd += PASS;
  cmd += "\"";
  SendDebug(cmd);
  if (ExpectResponse(ResponseOK)) {
    Serial.print("IP: ");
    Serial.println(GetIP());
  }
  else {
    PrintError(ErrorUnableToConnect);
  }
}

boolean SetCIPMODE(boolean Value) {
  if (Value) {WiFiSerial.println("AT+CIPMODE=1");}
  else {WiFiSerial.println("AT+CIPMODE=0");}
}
