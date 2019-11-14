// Thanks to:
// RoboIndia Code for HC-05 with AT Mode
// https://www.roboindia.com/tutorials

// bluetooth hc-05: RX --> Pin 11 & TX --> Pin 10 & EN --> 5V (& 5v to power source)

#include <SoftwareSerial.h>

SoftwareSerial BTSerial(10, 11); // TX, RX

void setup()
{
  Serial.begin(9600);
  Serial.println("Entering AT commands:");
  BTSerial.begin(38400);
}

void loop()
{
  // SEND AT COMMANDS
  //AT+NAME
  //AT+PSWD

  // Keep reading from HC-05 and send to Arduino Serial Monitor
  if (BTSerial.available())
    Serial.write(BTSerial.read());

  // Keep reading from Arduino Serial Monitor and send to HC-05
  if (Serial.available())
    BTSerial.write(Serial.read());
}
