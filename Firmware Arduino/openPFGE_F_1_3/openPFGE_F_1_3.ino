// connections
// servo: Pin 9 (& 5v (4.8 - 7.2 V) to power source)
// thermoresistor: GND and A0 (& 10 kohm from Pin A0 to 3.3V && wire 3.3V with AREF on arduino)
// bluetooth hc-05: RX --> Pin 11 & TX --> Pin 10 (& 5v to power source)
// LCD/I2C VCC -> 5V / GND -> GND / SDA -> A4 / SCK/SCL -> A5
// Mosfet Pump --> Pin 3
// Mosfet Fan 1 --> Pin 5
// Mosfet Fan 2 --> Pin 6
// Mosfet Peltier 1 --> Pin 7
// Mosfet Peltier 2 --> Pin 8

// libraries
#include <SoftwareSerial.h>
#include <Chrono.h>
#include <VarSpeedServo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// firmware
#define firmwareVersion 2

// debug
#define serialDebug false // should the program outputs the debug by serial port
bool serialStarted = false; // serial started?

// motor servo
VarSpeedServo servo; // the servo object
#define servoPin 9 // PWM signal pin
#define servoUsFrom 500 // minimum microseconds // for ds3218
#define servoUsTo 2500 // maximum microseconds // for ds3218
#define waitForMotorMove true // programs wait until motor end moving
int motorPosition = 0; // store current motor position (-1 = left, 0 = center, 1 = right)
int servoSpeed = 150;  // 0=full speed, 1-255 slower to faster

// bluetooth
SoftwareSerial BT(10, 11); // TX, RX

// LCD / I2C
Chrono displayTimer(Chrono::SECONDS); // Chrono for info update and view change
bool displayActive = true; // update display info?
int displayUpdateInterval = 2; // display update info interval (seconds)
int lastDisplayState = -1;
int lastDisplayParameter = 0;
bool displayBacklight = true;
LiquidCrystal_I2C display(0x27, 16, 2); // Direction 0x27 & 16 cols & 2 rows

// temperature control
int bufferTemperature = 0; // current read of buffer temperature
bool bufferTemperatureAutomaticControl = false; // if temperature of the buffer is automaticlly controlled by the refrigeration system
bool bufferCoolingOn = false;
int bufferTemperatureSetpoint = 15; // optimal buffer temperature (°C)
int bufferTemperatureMaxError = 3; // max diference between current buffer temperature and set point allowed before start the cooling system
#define bufferTemperatureSetpointMin 10
#define bufferTemperatureSetpointMax 30
#define bufferTemperatureMaxErrorMin 1
#define bufferTemperatureMaxErrorMax 10
#define pumpPin 3
#define fan1Pin 5
#define fan2Pin 6
#define peltier1Pin 7
#define peltier2Pin 8

// running parameters
bool onoff = true; // System on/off
bool pause = false; // System paused on/off
bool ramp = true; // System ramp on/off
int angle = 120; // Turning angle
int wop = 4; // between each movement in ramp off mode (seconds)
int wopAuto = 0; // // between each movement in ramp on mode (seconds)
#define maxWop 1000 // max wop value for ramp start/end (seconds)
#define maxRampDuration 100 // ramp max duration (hours)
int rampStart = 1; // initial time movement in ramp on mode (seconds)
int rampEnd = 25; // final time movement in ramp on mode (seconds)
int rampDuration = 12; // ramp time in ramp on mode (hours)

// variables
char inOutData[100]; // serial inputs & outpus
bool autoEnd = false; // whether the program has automaticlly ended
#define maxIntervalUpdate 60 // 60 seconds for maximum update interval
int bufferTemperatureUpdateInterval = 10; // buffer temperature update interval (seconds)
char methodSync = 'y';
char methodSet = 's';
char methodWho = 'w';
char methodAutomaticEnd = 'a';
char methodUnknown = 'u';
char methodCommunicationError = 'c';

// parameter options
#define paramOpOnOff "0"
#define paramOpPause "1"
#define paramOpRamp "2"
#define paramOpAngle "3"
#define paramOpWop "4"
#define paramOpAutoWop "5"
#define paramOpHasRun "6"
#define paramOpAutoEnd "7"
#define paramOpRampStart "8"
#define paramOpRampEnd "9"
#define paramOpRampDuration "10"
#define paramOpDisplayActive "11"
#define paramOpDisplayUpdateInterval "12"
#define paramOpBufferTemperature "13"
#define paramOpBufferTemperatureUpdateInterval "14"
#define paramOpBufferTemperatureAutomaticControl "15"
#define paramOpBufferTemperatureSetpoint "16"
#define paramOpBufferTemperatureMaxError "17"
#define paramOpDisplayBacklight "18"
#define paramOpServoSpeed "19"

// timers
Chrono runTimer(Chrono::SECONDS); // running timer
Chrono stepTimer(Chrono::SECONDS); // step timer
Chrono bufferTemperatureTimer(Chrono::SECONDS); // buffer temperature update timer

void setup() {
  // Chronos
  runTimer.start();
  stepTimer.start();
  bufferTemperatureTimer.start();
  displayTimer.start();

  // thermoresistor
  analogReference(EXTERNAL); // Connect AREF to 3.3V!

  // temperature control
  pinMode(pumpPin, OUTPUT);
  pinMode(fan1Pin, OUTPUT);
  pinMode(fan2Pin, OUTPUT);
  pinMode(peltier1Pin, OUTPUT);
  pinMode(peltier2Pin, OUTPUT);
  digitalWrite(pumpPin, LOW);
  digitalWrite(fan1Pin, LOW);
  digitalWrite(fan2Pin, LOW);
  digitalWrite(peltier1Pin, LOW);
  digitalWrite(peltier2Pin, LOW);

  // Motor init and center
  moveMotor(170);
  moveMotor(10);
  centerMotor();

  // BT series port initialice (For Mode AT 2)
  BT.begin(9600);

  // debug
  serialDebugWrite(F("Setup done"));

  // display init and splash
  display.init();
  displaySplash();
}

void loop() {
  // Check whether is there anything at serial
  loopSerial();

  // Make the next motor move
  loopMotor();

  // buffer temperature loop
  loopBufferTemperature();

  // display loop
  loopDisplay();
}

void loopSerial() {
  if (BT.available())
  {
    requestBtData();

    char * pch;
    char * method;
    char * inOutDataForProcess = (char *)malloc(strlen(inOutData) + 1);
    strcpy(inOutDataForProcess, inOutData);
    pch = strtok(inOutDataForProcess, "@=");

    while (pch != NULL)
    {
      method = strtok(NULL, "@=");
      if (strcmp(pch, "m") == 0) {
        break;
      }
      pch = strtok(NULL, "@=");
    }

    if (method[0] == methodWho) {
      sprintf(inOutData, "m=%c@fv=%d", methodWho, firmwareVersion);
      btSendMessage(inOutData);
    } else if (method[0] == methodSync) {
      encodeCurrent();
      sprintf(inOutData + strlen(inOutData), "@m=%c", methodSync);
      btSendMessage(inOutData);
      displayParamSent();
    } else if (method[0] == methodAutomaticEnd) {
      sprintf(inOutData, "m=%c", methodAutomaticEnd);
      btSendMessage(inOutData);
    } else if (method[0] == methodSet) {
      setParams();
      encodeCurrent();
      sprintf(inOutData + strlen(inOutData), "@m=%c", methodSet);
      btSendMessage(inOutData);
    } else if (method[0] == methodCommunicationError) {
      displayCommError();
    } else {
      sprintf(inOutData, "m=%c", methodUnknown);
      btSendMessage(inOutData);
    }
  }
}

void setParams() {
  char * pch;
  char * pchv;
  pch = strtok(inOutData, "@=");

  while (pch != NULL)
  {
    pchv = strtok(NULL, "@=");
    if (strcmp(pch, paramOpOnOff) == 0) {
      setOnOff(stob(pchv));
    } else if (strcmp(pch, paramOpPause) == 0) {
      setPause(stob(pchv));
    } else if (strcmp(pch, paramOpRamp) == 0) {
      setRamp(stob(pchv));
    } else if (strcmp(pch, paramOpAngle) == 0) {
      angle = constrain(stoi(pchv), 1, 180);
    } else if (strcmp(pch, paramOpWop) == 0) {
      wop = constrain(stoi(pchv), 1, maxWop);
    } else if (strcmp(pch, paramOpRampStart) == 0) {
      rampStart = constrain(stoi(pchv), 1, rampEnd - 1);
    } else if (strcmp(pch, paramOpRampEnd) == 0) {
      rampEnd = constrain(stoi(pchv), rampStart + 1, maxWop);
    } else if (strcmp(pch, paramOpRampDuration) == 0) {
      rampDuration = constrain(stoi(pchv), 1, maxRampDuration);
    } else if (strcmp(pch, paramOpAutoEnd) == 0) {
      autoEnd = false; // set to false always if it is set
    } else if (strcmp(pch, paramOpDisplayActive) == 0) {
      displayActive = stob(pchv);
      if (!displayActive)
        display.clear();
    } else if (strcmp(pch, paramOpDisplayUpdateInterval) == 0) {
      displayUpdateInterval = constrain(stoi(pchv), 1, maxIntervalUpdate);
    } else if (strcmp(pch, paramOpDisplayBacklight) == 0) {
      displayBacklight = stob(pchv);
      if (displayBacklight)
        display.backlight();
      else
        display.noBacklight();
    } else if (strcmp(pch, paramOpBufferTemperatureUpdateInterval) == 0) {
      bufferTemperatureUpdateInterval = constrain(stoi(pchv), 1, maxIntervalUpdate);
    } else if (strcmp(pch, paramOpBufferTemperatureAutomaticControl) == 0) {
      bufferTemperatureAutomaticControl = stob(pchv);
    } else if (strcmp(pch, paramOpBufferTemperatureSetpoint) == 0) {
      bufferTemperatureSetpoint = constrain(stoi(pchv), bufferTemperatureSetpointMin, bufferTemperatureSetpointMax);
    } else if (strcmp(pch, paramOpBufferTemperatureMaxError) == 0) {
      bufferTemperatureMaxError = constrain(stoi(pchv), bufferTemperatureMaxErrorMin, bufferTemperatureMaxErrorMax);
    } else if (strcmp(pch, paramOpServoSpeed) == 0) {
      servoSpeed = constrain(stoi(pchv), 0, 255);
    }
    pch = strtok(NULL, "@=");
  }
  displayParamUpdated();
}

void loopMotor() {
  if (onoff && !pause) {
    int currentwop = wop;
    if (ramp) {
      currentwop = wopAuto;
    }
    if (motorPosition == 0 || stepTimer.elapsed() > currentwop) {
      if (motorPosition == 0 || motorPosition == -1) {
        // go to +1 position
        moveMotor((int) (90.0 + angle / 2.0));
        motorPosition = 1;
      } else {
        // go to -1 position
        moveMotor((int) (90.0 - angle / 2.0));
        motorPosition = -1;
      }
      setNextWopAuto();
      stepTimer.restart();
    }
  }
}

void setNextWopAuto() {
  if (ramp) {
    if (runTimer.hasPassed((long) rampDuration * 3600)) {
      // Run end time reached
      setOnOff(true);
      autoEnd = true;
    } else {
      wopAuto = map((long) runTimer.elapsed(), (long) 0, (long) rampDuration * 3600, (long) rampStart, (long) (rampEnd + 1)); // +1 to be able to reach the last ramp wop
    }
  }
}

void centerMotor() {
  moveMotor(90);
  motorPosition = 0;
}

void moveMotor(int finalAngle) {
  servo.attach(servoPin);
  delay(15);
  servo.write(map(finalAngle, 0, 180, servoUsFrom, servoUsTo), servoSpeed, waitForMotorMove);
  servo.detach();
}

void loopDisplay() {
  if (displayActive && (!displayTimer.isRunning() || displayTimer.hasPassed(displayUpdateInterval))) {
    displayTimer.restart();
    if (autoEnd) {
      if (lastDisplayState == 1) return;
      lastDisplayState = 1;
      display.clear();
      display.setCursor(0, 0);
      display.print(F("Automatic end"));
      display.setCursor(0, 1);
      display.print(secondsToTime(runTimer.elapsed()));
    } else {
      // OnOff
      if (onoff) { // system on or paused
        if (pause) { // system paused
          if (lastDisplayState == 2) return;
          lastDisplayState = 2;
          display.clear();
          display.setCursor(0, 0);
          display.print(F("Paused "));
          display.print(secondsToTime(runTimer.elapsed()));
        } else { // system on
          if (lastDisplayState != 3) {
            display.clear();
            lastDisplayParameter = 0;
          }
          lastDisplayState = 3;

          display.setCursor(0, 0);
          display.print(F("Running "));

          // Timer
          display.print(secondsToTime(runTimer.elapsed()));

          display.setCursor(0, 1);
          display.print("                ");
          display.setCursor(0, 1);

          // Angle
          if (lastDisplayParameter == 0) {
            display.print(F("Angle / "));
            display.print(angle);
            display.print(F(" deg"));
          }

          // Buffer temperature
          if (lastDisplayParameter == 1) {
            display.print(F("B-Temp / "));
            display.print(bufferTemperature);
            display.print(F(" C"));
          }

          // Wop/ramp
          int lastDisplayParameterMax = 6;
          if (ramp) {
            if (lastDisplayParameter == 2) {
              display.print(F("R-WOP / "));
              display.print(wopAuto);
              display.print(F(" s"));
            }

            if (lastDisplayParameter == 3) {
              display.print(F("R-Start / "));
              display.print(rampStart);
              display.print(F(" s"));
            }

            if (lastDisplayParameter == 4) {
              display.print(F("R-End / "));
              display.print(rampEnd);
              display.print(F(" s"));
            }

            if (lastDisplayParameter == 5) {
              display.print(F("R-Dura / "));
              display.print(rampDuration);
              display.print(F(" h"));
            }
          } else {
            if (lastDisplayParameter == 2) {
              display.print(F("WOP / "));
              display.print(wop);
              display.print(F(" s"));
            }
            lastDisplayParameterMax = 3;
          }

          lastDisplayParameter++;
          if (lastDisplayParameter == lastDisplayParameterMax) {
            lastDisplayParameter = 0;
          }
        }
      } else { // system off
        if (lastDisplayState == 4) return;
        lastDisplayState = 4;
        display.clear();
        display.setCursor(5, 0);
        display.print(F("System"));
        display.setCursor(7, 1);
        display.print(F("Off"));
      }
    }
  }
}

void displaySplash() {
  if (displayActive) {
    display.clear();
    display.setCursor(6, 0);
    display.print(F("open"));
    display.setCursor(6, 1);
    display.print(F("PFGE"));

    delay(2000);
  }
}

void displayParamSent() {
  if (displayActive) {
    display.clear();

    display.setCursor(4, 0);
    display.print(F("Parameters"));
    display.setCursor(7, 1);
    display.print(F("Sent"));
    lastDisplayState = -1;

    delay(1000);
  }
}

void displayParamUpdated() {
  if (displayActive) {
    display.clear();

    display.setCursor(4, 0);
    display.print(F("Parameters"));
    display.setCursor(5, 1);
    display.print(F("Updated"));
    lastDisplayState = -1;

    delay(1000);
  }
}

void displayCommError() {
  if (displayActive) {
    display.clear();
    display.setCursor(0, 0);
    display.print(F("Communication Error"));
    lastDisplayState = -1;

    delay(1000);
  }
}

void loopBufferTemperature() {
  if (!bufferTemperatureTimer.isRunning() || bufferTemperatureTimer.hasPassed(bufferTemperatureUpdateInterval)) {
    bufferTemperature = readTemperature();
    bufferTemperatureTimer.restart();
  }

  if (bufferTemperatureAutomaticControl) {
    // decide if cooling is needed
    if (bufferTemperature + bufferTemperatureMaxError > bufferTemperatureSetpoint && !bufferCoolingOn) { // start cooling
      digitalWrite(pumpPin, HIGH);
      digitalWrite(fan1Pin, HIGH);
      digitalWrite(fan2Pin, HIGH);
      digitalWrite(peltier1Pin, HIGH);
      digitalWrite(peltier2Pin, HIGH);
      bufferCoolingOn = true;
    } else { // stop cooling
      if (bufferCoolingOn) { // just if it is cooling
        digitalWrite(pumpPin, LOW);
        digitalWrite(fan1Pin, LOW);
        digitalWrite(fan2Pin, LOW);
        digitalWrite(peltier1Pin, LOW);
        digitalWrite(peltier2Pin, LOW);
        bufferCoolingOn = false;
      }
    }
  }
}

void setOnOff(bool newOnOff) {
  if (onoff == newOnOff) {
    return;
  }
  if (newOnOff) {
    runTimer.restart();
    stepTimer.restart();
    setNextWopAuto();
    autoEnd = false;
  } else {
    runTimer.stop();
    stepTimer.stop();
    centerMotor();
  }
  onoff = newOnOff;
}

void setPause(bool newPause) {
  if (pause == newPause) {
    return;
  }
  if (newPause) {
    runTimer.stop();
    stepTimer.stop();
    centerMotor();
  } else {
    runTimer.resume();
    stepTimer.resume();
  }
  pause = newPause;
}

void setRamp(bool newRamp) {
  if (ramp == newRamp) {
    return;
  }
  if (onoff) {
    runTimer.restart();
    stepTimer.restart();
  }
  ramp = newRamp;
}

void requestBtData() {
  if (BT.available()) {
    strcpy(inOutData, BT.readString().c_str());
    return;
  }
  sprintf(inOutData, "");
}

void serialDebugWrite(String outputtext) {
  if (serialDebug) {
    if (!serialStarted) {
      Serial.begin(9600);
      serialStarted = true;
    }
    Serial.print(millis());
    Serial.print(F(" | "));
    Serial.println(outputtext);
  }
}

void encodeCurrent() {
  sprintf(inOutData, "%s=%c@%s=%c@%s=%c@%s=%d@%s=%d@%s=%d@%s=%d@%s=%d@%s=%d@%s=%d@%s=%lu@%s=%c@%s=%c@%s=%d@%s=%d@%s=%c@%s=%d@%s=%d@%s=%c@%s=%d",
          paramOpOnOff, btos(onoff),
          paramOpPause, btos(pause),
          paramOpRamp, btos(ramp),
          paramOpAngle, angle,
          paramOpWop, wop,
          paramOpRampStart, rampStart,
          paramOpRampEnd, rampEnd,
          paramOpRampDuration, rampDuration,
          paramOpAutoWop, wopAuto,
          paramOpBufferTemperature, bufferTemperature,
          paramOpHasRun, runTimer.elapsed(),
          paramOpAutoEnd, btos(autoEnd),
          paramOpDisplayActive, btos(displayActive),
          paramOpDisplayUpdateInterval, displayUpdateInterval,
          paramOpBufferTemperatureUpdateInterval, bufferTemperatureUpdateInterval,
          paramOpBufferTemperatureAutomaticControl, btos(bufferTemperatureAutomaticControl),
          paramOpBufferTemperatureSetpoint, bufferTemperatureSetpoint,
          paramOpBufferTemperatureMaxError, bufferTemperatureMaxError,
          paramOpDisplayBacklight, btos(displayBacklight),
          paramOpServoSpeed, servoSpeed
         );
}

void btSendMessage(char * message) {
  BT.println(message);
}

int stoi(char * convert) {
  return atoi(convert);
}

bool stob(char * convert) {
  return strcmp(convert, "t") == 0 ? true : false;
}

char btos(bool convert) {
  return convert ? 't' : 'f';
}

char * secondsToTime(unsigned long t)
{
  static char str[12];
  sprintf(str, "");
  t = (unsigned long) t; // testing trick to fix correct transformation
  int h = t / 3600;
  t = t % 3600;
  int m = t / 60;
  int s = t % 60;
  sprintf(str, "%02d:%02d:%02d", h, m, s);
  return str;
}

// https://learn.adafruit.com/thermistor/using-a-thermistor
#define THERMISTORPIN A0
#define THERMISTORNOMINAL 10000
#define TEMPERATURENOMINAL 25
#define NUMSAMPLES 5
#define BCOEFFICIENT 3950
#define SERIESRESISTOR 10000
int samples[NUMSAMPLES];

int readTemperature() {
  uint8_t i;
  float average;

  // take N samples in a row, with a slight delay
  for (i = 0; i < NUMSAMPLES; i++) {
    samples[i] = analogRead(THERMISTORPIN);
    delay(10);
  }

  // average all the samples out
  average = 0;
  for (i = 0; i < NUMSAMPLES; i++) {
    average += samples[i];
  }
  average /= NUMSAMPLES;

  // convert the value to resistance
  average = 1023 / average - 1;
  average = SERIESRESISTOR / average;

  float steinhart;
  steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;

  return (int) steinhart;
}
