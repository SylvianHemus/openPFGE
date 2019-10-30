// Thanks to:
// RoboIndia Code for HC-05 with AT Mode
// https://www.roboindia.com/tutorials

#include <SoftwareSerial.h>

SoftwareSerial BTSerial(10, 11); // TX, RX

#define BTName "openPFGE"
#define BTPass "1234"

void setup()
{
  Serial.begin(9600);
  Serial.println("Enter AT commands:");
  BTSerial.begin(38400);
  //if (BTSerial.available()){
  /*} else {
    Serial.println("BT not available");
    }*/
}

void loop(){  
    BTSerial.print("AT+NAME=");
    BTSerial.println(BTName);
    BTSerial.print("AT+PSWD=");
    BTSerial.println(BTPass);
    
    Serial.println("Name & Password set");
        
    Serial.print("Name: ");
    BTSerial.println("AT+NAME");
    Serial.println(BTSerial.read());
    
    Serial.println("Password:");
    BTSerial.println("AT+PSWD");
    Serial.println(BTSerial.read());
    
    Serial.println("Version:");
    BTSerial.println("AT+VERSION");
    Serial.println(BTSerial.read());
  }
