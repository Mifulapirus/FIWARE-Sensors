#include <SoftwareSerial.h>
#include <ESP8266.h>

/*******************************
* Pin definitions
*******************************/
#define SDA_PORT PORTC
#define SDA_PIN 1
#define SCL_PORT PORTC
#define SCL_PIN 0

#define HV_SWITCH_PIN A5

#define WIFI_RX_PIN   2
#define WIFI_TX_PIN   3
#define WIFI_RST_PIN  4

#define LED_PIN  13

/*******************************
* General variables
*******************************/
#define FIRMWARE_VERSION   "V 0.1"
#define DEVICE_TYPE    "\"FireSensor\"" 
#define DEVICE_ID      "\"FireSensor_1\""
long KeepAliveDelay = 300;
long PreviousKeepAlive = 0;
#define CONNECTED "Connected: "

/*******************************
* WiFi Stuff
*******************************/
#define BAUD 9600
//#define SSID "bamboo_demo"
//#define PASS "paloverde"

#define SSID "Interne"
#define PASS "Mec0mebien"

ESP8266 wifi(WIFI_RX_PIN, WIFI_TX_PIN, WIFI_RST_PIN, BAUD);

/*******************************
* TPA81 Stuff
*******************************/
#define ADDRESS             0xD0                                  // Address of TPA81
#define SOFTREG             0x00                                   // Byte for software version
#define AMBIENT             0x01                                   // Byte for ambiant temperature
int temperature[] = {0,0,0,0,0,0,0,0};                   // Array to hold temperature data
byte servoPos = 0;
#define SERVO_POS_MAX 30
#define SERVO_POS_MIN 0
#define SERVO_DIRECTION_LEFT 1
#define SERVO_DIRECTION_RIGHT -1
byte servoDirection = SERVO_DIRECTION_LEFT;
int ambientTemp = 0;
#define FIRE_THRESHOLD 50        //Threshold temperature to detect a fire in ºC

/*******************************
* FIWARE Stuff
*******************************/
#define CB_IP "130.206.127.115"
//#define CB_IP "192.168.4.42"
#define CB_Port "1026"

#define CBStaticHeader         "POST /NGSI10/updateContext\r\nAccept:application/json\r\nAccept-Encoding:deflate\r\nCache-Control:no-cache\r\nContent-Type: application/json\r\nContent-Length:"
//#define CBStaticHeader         "POST /NGSI10/updateContext\r\nAccept:application/json\r\nAccept-Encoding:deflate\r\nCache-Control:no-cache\r\nContent-Length:"
String CBElement_Length        = "169";
#define CBBody1                "\r\n\r\n{\"contextElements\":[{\"type\":" //\"FireSensor\"
#define CBElement_Type         DEVICE_TYPE
#define CBBody2                ",\"isPattern\":\"false\",\"id\":"//\"Sensor1\"
#define CBElement_ID           DEVICE_ID
#define CBBody3                ",\"attributes\":[{\"name\":" //\"thereisfire\"
#define CBElement_Att_1_Name   "\"FireDetected\""
#define CBBody4                ",\"type\":" //\"bool\"
#define CBElement_Att_1_Type   "\"bool\""
#define CBBody5                ",\"value\":" //\"0\"
String CBElement_Att_1_Value   = "\"0\"";
#define CBBody6                "},{\"name\":"
#define CBElement_Att_2_Name   "\"FirePos\""
#define CBBody7                CBBody4 //\"bool\"
#define CBElement_Att_2_Type   "\"int\""
#define CBBody8                CBBody5 //\"0\"
String CBElement_Att_2_Value   = "\"0\"";
#define CBBody9                "}]}],\"updateAction\":\"APPEND\"}\r\n"

#define FIRE_ON "\"1\""
#define FIRE_OFF "\"0\""

#define CBPostOK "\"code\" : \"200\""

long CBLastUpdate = 0;
long CBRegularUpdateDelay = 30000;  //60 sec
long CBUpdateDelay = CBRegularUpdateDelay;

#include <SoftI2CMaster.h>    //This library needs to be included at the end

/************************************
* Setup and initializations
************************************/  
void setup() {
  //Set output pins
  pinMode(WIFI_RST_PIN, OUTPUT);
  pinMode(HV_SWITCH_PIN, OUTPUT);
  
  //Initialize native serial port
  Serial.begin(9600);
  
  //Initialize Seoftware I2C  
  i2c_init();
  //Serial.print("Amb: ");
  ambientTemp = getTPA81Data(AMBIENT);                    // Get reading of ambiant temperature and print to LCD03 screen
  Serial.println(ambientTemp);
  wifi.wifiLongMessage.reserve(400);
  wifi.listen();
  bootUp(); 
}

//Bootup sequence
void bootUp() {
  byte _err = -1;
  Serial.println(DEVICE_ID);
  Serial.println(FIRMWARE_VERSION);
  wifi.setTxMode(false);
  delay(200);
  //wifi.connectionMode("1");
  //delay(200);
  
  //Serial.println("Init");
  _err = wifi.init(SSID, PASS);
  if(_err != NO_ERROR) {
    Serial.print("E: ");
    Serial.println(String(_err));
  }
  
  else{
    Serial.print(CONNECTED);
    Serial.println(wifi.IP);
  }
  
  _err = wifi.connectionMode("0");
  if(_err != NO_ERROR) {
    Serial.print("E: ");
    Serial.println(String(_err));
  }
  
  wifi.setTxMode(false);
}

/************************************
* General Functions
************************************/
void loop() {
  PeriodicUpdate();  //Just Update the sensor, so we know it is alive
}

/************************************
* Context Broker functions
************************************/
void UpdateCB() {
  Serial.print("U");
  Serial.println(millis());
  
  //Open TCP
  Serial.println("Op");
  byte _err = wifi.openTCP(CB_IP, CB_Port, false);

  if(_err != NO_ERROR) {
    Serial.print("E: ");
    Serial.println(String(_err));
    Serial.println(wifi.lastResponse);
  }
  
  else{
    Serial.print(CONNECTED);
    Serial.println(CB_IP);
  }
  delay(100);
  
  //Calculate CBElement_Length
  CBElement_Length = String((sizeof(CBBody1)-1
                     + sizeof(CBElement_Type)-2 
                     + sizeof(CBBody2)-1 
                     + sizeof(CBElement_ID)-2
                     + sizeof(CBBody3)-1 
                     + sizeof(CBElement_Att_1_Name)-2
                     + sizeof(CBBody4)-1 
                     + sizeof(CBElement_Att_1_Type)-2
                     + sizeof(CBBody5)-1 
                     + CBElement_Att_1_Value.length() 
                     + sizeof(CBBody6)-1
                     + sizeof(CBElement_Att_2_Name)-2
                     + sizeof(CBBody7)-1
                     + sizeof(CBElement_Att_2_Type)-2 
                     + sizeof(CBBody8)-1
                     + CBElement_Att_2_Value.length() 
                     + sizeof(CBBody9)-1));
                                   
  wifi.wifiLongMessage = CBStaticHeader;
  wifi.wifiLongMessage += CBElement_Length;
  wifi.wifiLongMessage +=  CBBody1;
  wifi.wifiLongMessage += CBElement_Type;
  wifi.wifiLongMessage += CBBody2;
  wifi.wifiLongMessage += CBElement_ID;
  wifi.wifiLongMessage += CBBody3;
  wifi.wifiLongMessage += CBElement_Att_1_Name;
  wifi.wifiLongMessage += CBBody4;
  wifi.wifiLongMessage += CBElement_Att_1_Type;
  wifi.wifiLongMessage += CBBody5;
  wifi.wifiLongMessage += CBElement_Att_1_Value;
  wifi.wifiLongMessage += CBBody6;
  wifi.wifiLongMessage += CBElement_Att_2_Name;
  wifi.wifiLongMessage += CBBody7;
  wifi.wifiLongMessage += CBElement_Att_2_Type;
  wifi.wifiLongMessage += CBBody8;
  wifi.wifiLongMessage += CBElement_Att_2_Value;
  wifi.wifiLongMessage += CBBody9;
  

  _err = wifi.sendLongMessage(CBPostOK, false);
  if(_err != NO_ERROR) {
    Serial.print("E: ");
    Serial.println(String(_err));
    Serial.println(wifi.lastResponse);
  }
  
  else{
    Serial.println(CBPostOK);
  }
  wifi.closeTCP();
  

  CBLastUpdate = millis();  
}

void PeriodicUpdate() {
  long Now = millis();
  if (Now > CBLastUpdate + CBUpdateDelay) {
    UpdateCB();
  }
  
  else if (Now > PreviousKeepAlive + KeepAliveDelay) {
    //Refresh ambient temperature
    PreviousKeepAlive = Now;
    ambientTemp = getTPA81Data(AMBIENT);                    // Get reading of ambiant temperature and print to LCD03 screen
    
    for(int i = 0; i < 8; i++){                            // Loops and stores temperature data in array
      temperature[i] = getTPA81Data(i+2);
      while (temperature[i] > ambientTemp + FIRE_THRESHOLD) {  //FIRE!!!!
        CBElement_Att_1_Value = FIRE_ON;                          //There is fire
        CBElement_Att_2_Value = "\"" + String(servoPos) + "\"";   //Fire Location
        Serial.print("F @ " + String(CBElement_Att_2_Value) + "> " + String(temperature[i]));
        UpdateCB();
        temperature[i] = getTPA81Data(i+2);
        delay(5000);
      }
      
      if ((CBElement_Att_1_Value == FIRE_ON) && !(temperature[i] > ambientTemp + FIRE_THRESHOLD)) { //If there was FIRE, but not anymore, 
        CBElement_Att_1_Value = FIRE_OFF;                         //There is fire
        CBElement_Att_2_Value = "\"" + String(servoPos) + "\"";   //Fire Location
        Serial.print("C @ ");
        Serial.print(CBElement_Att_2_Value);
        Serial.print("> ");
        Serial.print(temperature[i]);
        Serial.println("C");
        UpdateCB();
      }
    }
    if (servoPos == SERVO_POS_MAX)  servoDirection = SERVO_DIRECTION_RIGHT;
    else if (servoPos == SERVO_POS_MIN)  servoDirection = SERVO_DIRECTION_LEFT;
    setServo(servoPos += servoDirection);  //Move servo
  }
}

/**************************************
* TPA81 Functions
**************************************/
byte getTPA81Data(byte reg){              // Function to receive one byte of data from TPA81
  i2c_start(ADDRESS+I2C_WRITE);                // Begin communication with TPA81
  i2c_write(reg);                 // Send reg to TPA81
  i2c_rep_start(ADDRESS+I2C_READ);      // Request 1 byte
  delay(10);
  byte data = i2c_read(true);           // Get byte
  i2c_stop();
  return(data);                      // return byte
}

void setServo(int _pos) {
  i2c_start(ADDRESS+I2C_WRITE);      // Begin communication with TPA81
  i2c_write(0);                      // Command Reg
  i2c_write(_pos);
  i2c_stop();
}


