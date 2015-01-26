/****************************************
* FIWARE Sensors
* Fire detector
* Version = 0.1
* Attached peripherals:
*  - Hall effect: A3144 on pin D2
*  - ESP8266: TX-12, RX-13
*****************************************/
#include <stdlib.h>
#include <SoftwareSerial.h>

/*******************************
* General variables
*******************************/
String FirmwareVersion = "0.1";
String DeviceCategory = "FIWARE-Fire detector";
int DeviceID = 1; 
String DeviceName = "Fire";

/*******************************
* WiFi Stuff
*******************************/
#define SSID "Interne"
#define PASS "Mec0mebien"
SoftwareSerial WiFiSerial(12, 11); // RX, TX
String OwnIP = "0.0.0.0";

/*******************************
* FIWARE Stuff
********************************/
String ContextBrokerURL = "http://localhost:1026";
String NGSIHeader = "/NGSI10/updateContext";
String CBUpdate = "{\"contextElements\":[{\"type\":\"FireSensor\",\"isPattern\": \"false\",\"id\": \"Sensor1\",\"attributes\": [ {\"name\": \"thereisfire\",\"type\": \"bool\",\"value\": \"0\"} ] } ], \"updateAction\": \"UPDATE\"}";
long CBLastUpdate = 0;
long CBRegularUpdateDelay = 60000*60;  //1 hour
long CBMinUpdateDelay = 10000;         //10 seconds
long CBUpdateDelay = CBRegularUpdateDelay;

/******************************
* Pin definition
******************************/
const int HallPin = 2;       // Hall Effect Sensor pin
const int LedPin =  13;      // Onboard LED pin

/****************************
* Setup
****************************/
void setup() {
  pinMode(LedPin, OUTPUT);
  pinMode(HallPin, INPUT);
  
  Serial.begin(9600);
  WiFiSerial.begin(9600);
  
  Serial.println(DeviceName);
  Serial.println(DeviceCategory);
  Serial.print("V. ");
  Serial.println(FirmwareVersion);
  Serial.print("ID: ");
  Serial.println(DeviceID);
  //Attach the interruption
  attachInterrupt(0, FireDetected, RISING);
  
  InitWiFi();
//  WiFiEcho();
}

/************************************
* Main Loop
************************************/
void loop() {
  PeriodicUpdate();  //Just Update the sensor, so we know it is alive
}


/****************************
* Fire Interruption CallBack
*****************************/
void FireDetected() {
  long CurrentTime = millis();
  digitalWrite(LedPin, HIGH);
  Serial.print (CurrentTime)
  Serial.print(" - FIRE!");
  digitalWrite(LedPin, LOW);
}

/************************************
* Context Broker functions
************************************/
void UpdateCB() {
  Serial.print("ContextBroker update @ ");
  Serial.println(millis());
  OpenTCP(ContextBrokerURL);
  String cmd = NGSIHeader;
  cmd += CBUpdate;
  cmd += "\r\n";
  WiFiSerial.write("AT+CIPSEND=");
  WiFiSerial.write(cmd.length());
  
  if (ExpectResponse(">")) {
    Serial.print(">");
    SendDebug(cmd);
  }
  //else {CloseTCP();}
}

void PeriodicUpdate() {
  if (millis() > CBLastUpdate + CBUpdateDelay) {
    UpdateCB();
  }
}

/***********************************
* WiFi Functions
***********************************/
boolean OpenTCP(String IP) {
  String cmd = "AT+CIPSTART=\"TCP\",\""; 
  cmd += IP;
  cmd += "\",80";
  SendDebug(cmd);
  delay(1000);
  if (WiFiSerial.find("Error")) {
    Serial.println("ERROR: 1");  //Unable to open TCP connection
    return false;
  }
  return true;
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
  if (!ExpectResponse("OK")) {CheckWiFi();}
}


boolean InitWiFi() {
  while (!WiFiReboot())  {}
  //while (!CheckWiFi())   {}
  while (!ConnectWiFi()) {}
  return true;
}

boolean WiFiReboot() {
  WiFiSerial.flush();
  Serial.println("Rebooting WiFi");
  WiFiSerial.println("AT+RST"); // restet and test if module is redy
  if (ExpectResponse("Ready")) {
    Serial.println("->Ready");
    //SendDebug("AT+CIPMODE=1");
    return true;  
  }
  else {
    Serial.println("ERROR: 2"); //Module doesn't respond
    return false;
  }
}

boolean CheckWiFi() {
  SendDebug("AT");
  if (ExpectResponse("OK")) {
    Serial.println("->OK");
    return true;
  }
  else {
    Serial.println("ERROR: 3"); //"WiFi module does not respond to AT"
    InitWiFi();
    return false;
  }
}

boolean ExpectResponse(char* _Expected) {
  Serial.print("Waiting for ");
  Serial.println(_Expected);

  for (int i = 0; i < 10; i++) {
    if (WiFiSerial.find(_Expected)) {
      //Serial.print("RECEIVED: ");
      //Serial.println(_Expected);
      return true;
    }

    else {
      //TODO: Do something if not found yet?
      Serial.print(".");
    }
    delay(1);
  }
  Serial.print("ERROR: ");
  Serial.print(_Expected);
  Serial.println(" not found");
  return false;
}

String GetIP() {
  WiFiSerial.flush();
  WiFiSerial.println("AT+CIFSR"); // Get IP
  String _IP;
  char incomingByte = ' ';
  delay(20);
  while (WiFiSerial.available() > 0) {
    // read the incoming byte:
    incomingByte = WiFiSerial.read();
    // say what you got:
    _IP += incomingByte;
    delay(1);
  }
  OwnIP = _IP;
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
  if (ExpectResponse("OK")) {
    Serial.println("Connected!");
    Serial.print("IP: ");
    Serial.println(GetIP());
  }
  else {
    Serial.println("ERROR: 4"); //"ERROR: Not connected"
  }
}
