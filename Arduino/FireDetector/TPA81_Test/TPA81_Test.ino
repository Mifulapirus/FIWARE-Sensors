/********************************************
* TPA 81 Test based on James Henderson's version
*Author: Angel Hernandez
********************************************/
#define NO_INTERRUPT 1
#define I2C_TIMEOUT 1000

#define SDA_PORT PORTC
#define SDA_PIN 1
#define SCL_PORT PORTC
#define SCL_PIN 0

#include <SoftI2CMaster.h>

#define ADDRESS             0xD0                                  // Address of TPA81
#define SOFTREG             0x00                                   // Byte for software version
#define AMBIANT             0x01                                   // Byte for ambiant temperature

int temperature[] = {0,0,0,0,0,0,0,0};                   // Array to hold temperature data

void setup(){
  Serial.begin(9600);
  Serial.print("TPA81 Example  V:");
  i2c_init();
  byte software = getData(SOFTREG);                       // Get software version 
  Serial.println(software);                                // Print software version to the screen
  Serial.print("Ambient: ");
  int ambiantTemp = getData(AMBIANT);                    // Get reading of ambiant temperature and print to LCD03 screen
  Serial.println(ambiantTemp);

  delay(2000);                                            // Wait to make sure everything is powerd up
}
  
void loop(){
  for (int servoPos = 2; servoPos < 32; servoPos++) {
    for(int i = 0; i < 8; i++){                            // Loops and stores temperature data in array
      temperature[i] = getData(i+2);
      Serial.print(String(temperature[i]) + " ");
    }
    Serial.println("");
    setServo(servoPos);
    delay(200);
  }

  for (int servoPos = 31; servoPos >= 2; servoPos--) {
    for(int i = 0; i < 8; i++){                            // Loops and stores temperature data in array
      temperature[i] = getData(i+2);
      Serial.print(String(temperature[i]) + " ");
    }
    Serial.println("");
    setServo(servoPos);
    delay(200);
  }

  
}

byte getData(byte reg){              // Function to receive one byte of data from TPA81
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
