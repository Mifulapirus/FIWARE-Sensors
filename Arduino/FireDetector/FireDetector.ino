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
#define FirmwareVersion "0.1"
#define DeviceCategory "FD" //FIWARE-Fire detector";
int DeviceID = 1; 
#define DeviceName "F"

/*******************************
* WiFi Stuff
*******************************/
#define SSID "Interne"
#define PASS "Mec0mebien"
SoftwareSerial WiFiSerial(12, 11); // RX, TX
String OwnIP = "0.0.0.0";

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
*--------------------------------------
* The shortest way to send it would be as follows
* Accept:application/json
* Accept-Encoding:deflate
* Cache-Control:no-cache
* Content-Length:675
* Content-Type:application/x-www-form-urlencoded; charset=UTF-8
* {"contextElements":[{"type":"FireSensor","isPattern":"false","id":"Sensor1","attributes":[{"name":"thereisfire","type":"bool","value":"0"}]}],"updateAction":"UPDATE"} /EOF
*----------------------------------------
* Adding special characters, each string should be as the following:
* "Accept:application/json\r\n"
* "Accept-Encoding:deflate\r\n"
* "Cache-Control:no-cache\r\n"
* "Content-Length:675\r\n"
* "Content-Type:application/x-www-form-urlencoded; charset=UTF-8\r\n"
* "{\"contextElements\":[{\"type\":\"FireSensor\",\"isPattern\":\"false\",\"id\":\"Sensor1\",\"attributes\":[{\"name\":\"thereisfire\",\"type\":\"bool\",\"value\":\"0\"}]}],\"updateAction\":\"UPDATE\"}\r\n\0"
********************************/
#define CBIP "130.206.127.115"
//String CBIP = "192.168.0.111";
#define CBPort "1026"

/*String CBUpdateURL = "POST /NGSI10/updateContext\r\n";
String CBHeader1 = "Accept:application/json\r\n";
String CBHeader2 = "Accept-Encoding:deflate\r\n";
String CBHeader3 = "Cache-Control:no-cache\r\n";
String CBHeader4 = "Content-Length:166\r\n";
String CBHeader5 = "Content-Type:application/x-www-form-urlencoded; charset=UTF-8\r\n";
String CBUpdateMessage1 = "{\"contextElements\":[{\r\n";
String CBUpdateMessage2= "\"type\":\"FireSensor\",\r\n";
String CBUpdateMessage3 = "\"isPattern\":\"false\",\r\n";
String CBUpdateMessage4 = "\"id\":\"Sensor1\",\r\n";
String CBUpdateMessage5 = "\"attributes\":[{\r\n";
String CBUpdateMessage6 = "\"name\":\"thereisfire\",\r\n";
String CBUpdateMessage7 = "\"type\":\"bool\",\r\n";
String CBUpdateMessage8 = "\"value\":\"0\"}]}],\r\n";
String CBUpdateMessage9 = "\"updateAction\":\"UPDATE\"}\r\n";
*/

#define UpdateString "POST /NGSI10/updateContext\r\nAccept:application/json\r\nAccept-Encoding:deflate\r\nCache-Control:no-cache\r\nContent-Length:166\r\nContent-Type: application/json\r\n\r\n{\"contextElements\":[{\r\n\"type\":\"FireSensor\",\r\n\"isPattern\":\"false\",\r\n\"id\":\"Sensor1\",\r\n\"attributes\":[{\r\n\"name\":\"thereisfire\",\r\n\"type\":\"bool\",\r\n\"value\":\"0\"}]}],\r\n\"updateAction\":\"UPDATE\"}\r\n\r\n"

long CBLastUpdate = 0;
long CBRegularUpdateDelay = 10000;  //10 sec
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
  //WiFiEcho();
  UpdateCB();
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
  Serial.print (CurrentTime);
  Serial.println(" - FIRE!");
  digitalWrite(LedPin, LOW);
}

/************************************
* Context Broker functions
************************************/
void UpdateCB() {
  Serial.print("ContextBroker update @ ");
  Serial.println(millis());
  while (!OpenTCP(CBIP, CBPort)) {}
  delay(1000);
  SendToHost(UpdateString);
  CloseTCP();
  
}

void PeriodicUpdate() {
  if (millis() > CBLastUpdate + CBUpdateDelay) {
    UpdateCB();
    CBLastUpdate = millis();
  }
}

/***********************************
* WiFi Functions
***********************************/
boolean OpenTCP(String IP, String Port) {
  String cmd = "AT+CIPSTART=\"TCP\",\""; 
  cmd += IP;
  cmd += "\",";
  cmd += Port;
  SendDebug(cmd);
  delay(1000);
  if (WiFiSerial.find("ERROR")) {
    Serial.println("ERROR: 1");  //Unable to open TCP connection
    return false;
  }
  return true;
}

boolean SendToHost(String cmd) {
  Serial.print("Sending CMD: ");
  Serial.println(cmd);
  Serial.print("Length: ");
  Serial.println(cmd.length());
  WiFiSerial.print("AT+CIPSEND=");
  WiFiSerial.println(cmd.length());
  delay(200);
  //if (ExpectResponse(">")) {
    Serial.print(">");
    Serial.print(cmd);
    WiFiSerial.print(cmd);
    //return true;
  //}
  //return false;
  //else {CloseTCP();}
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
  SetCIPMODE(false);
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
  delay(100);
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

boolean SetCIPMODE(boolean Value) {
  Serial.print("Set CIP Mode to ");
  Serial.println(Value);
  if (Value) {WiFiSerial.println("AT+CIPMODE=1");}
  else {WiFiSerial.println("AT+CIPMODE=0");}
}
