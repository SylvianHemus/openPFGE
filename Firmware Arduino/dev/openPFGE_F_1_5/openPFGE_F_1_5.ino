// Target Circuit Version = openPFGE H1.3

// firmware
#define firmwareVersion 3

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
#include <Chrono.h> // https://github.com/SofaPirate/Chrono
#include <VarSpeedServo.h> // https://github.com/netlabtoolkit/VarSpeedServo
#include <Wire.h>
#include <LiquidCrystal_I2C.h> // https://github.com/johnrickman/LiquidCrystal_I2C
#include <ArduinoJson.h> // https://arduinojson.org/

// debug
#define serialDebug false // should the program outputs the debug by serial port
bool serialStarted = false; // serial started?

// motor servo
VarSpeedServo servo; // the servo object
#define servoPin 9 // PWM signal pin
#define servoUsFrom 850 // minimum microseconds // for ds3218
#define servoUsTo 2150 // maximum microseconds // for ds3218
#define waitForMotorMove true // programs wait until motor end moving
int motorPosition = 0; // store current motor position (-1 = left, 0 = center, 1 = right)
int servoSpeed = 255;  // 0=full speed, 1-255 slower to faster

// bluetooth
SoftwareSerial BT(10, 11); // TX, RX

// LCD / I2C
Chrono displayTimer(Chrono::SECONDS); // Chrono for info update and view change
bool displayActive = true; // update display info?
int displayUpdateInterval = 2; // display update info interval (seconds)
int lastDisplayState = -1;
int lastDisplayParameter = 0;
bool displayBacklight = false;
LiquidCrystal_I2C display(0x27, 16, 2); // Direction 0x27 & 16 cols & 2 rows

// temperature control
int bufferTemperature = 0; // current read of buffer temperature
bool bufferTemperatureAutomaticControl = false; // if temperature of the buffer is automaticlly controlled by the refrigeration system
bool bufferTemperatureManualControlOn = false; // if temperature of the buffer is manually turned on
bool bufferCoolingOn = false;
int bufferTemperatureSetpoint = 15; // optimal buffer temperature (°C)
int bufferTemperatureMaxError = 2; // max diference between current buffer temperature and set point allowed before start the cooling system
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
bool onoff = false; // System on/off
bool pause = false; // System paused on/off
bool ramp = false; // System ramp on/off
int angle = 120; // Turning angle
int wop = 4; // between each movement in ramp off mode (seconds)
int wopAuto = 0; // // between each movement in ramp on mode (seconds)
#define maxWop 1000 // max wop value for ramp start/end (seconds)
#define maxRampDuration 100 // ramp max duration (hours)
int rampStart = 1; // initial time movement in ramp on mode (seconds)
int rampEnd = 25; // final time movement in ramp on mode (seconds)
int rampDuration = 24; // ramp time in ramp on mode (hours)

// variables
bool autoEnd = false; // whether the program has automaticlly ended
#define maxIntervalUpdate 60 // 60 seconds for maximum update interval
int bufferTemperatureUpdateInterval = 3; // buffer temperature update interval (seconds)
bool bufferTemperatureSystemReadyOnState = false; // buffer temperature control system ready on state
bool pumpOn = false;
bool fansOn = false;
bool peltier1On = false;
bool peltier2On = false;
const int jsonCapacity =  30 * JSON_OBJECT_SIZE(1); // json document capacity
StaticJsonDocument<jsonCapacity> jsonDoc; // Allocate the JsonDocument
JsonObject jsonObj; // json object
DeserializationError jsonErr;

// parameter options
#define methodParam "m"
#define methodSync "sy"
#define methodSet "se"
#define methodWho "wh"
#define methodAutomaticEnd "ae"
#define methodUnknown "uk"
#define methodCommunicationError "ce"

#define paramOpOnOff "o"
#define paramOpPause "p"
#define paramOpRamp "r"
#define paramOpAngle "a"
#define paramOpWop "w"
#define paramOpAutoWop "aw"
#define paramOpHasRun "hr"
#define paramOpAutoEnd "ae"
#define paramOpRampStart "rs"
#define paramOpRampEnd "re"
#define paramOpRampDuration "rd"
#define paramOpDisplayActive "da"
#define paramOpDisplayUpdateInterval "du"
#define paramOpDisplayBacklight "db"
#define paramOpBufferTemperature "bt"
#define paramOpBufferTemperatureUpdateInterval "bu"
#define paramOpBufferTemperatureAutomaticControl "bc"
#define paramOpBufferTemperatureManualControlOn "bm"
#define paramOpBufferTemperatureSetpoint "bs"
#define paramOpBufferTemperatureMaxError "be"
#define paramOpServoSpeed "ss"
#define paramFirmwareVersion "fv"

// timers
Chrono runTimer(Chrono::SECONDS); // running timer
Chrono stepTimer(Chrono::SECONDS); // step timer
Chrono bufferTemperatureTimer(Chrono::SECONDS); // buffer temperature update timer

void setup() {
  // Chronos
  runTimer.stop();
  stepTimer.stop();
  bufferTemperatureTimer.restart();
  displayTimer.restart();

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

  // BT series port initialice (For Mode AT 2)
  BT.begin(9600);

  // debug
  serialDebugWrite(F("Setup done"));

  // display init and splash
  display.init();
  displaySplash();

  // Motor init and center
  centerMotor();
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
    if (requestBtData()) {
      if (jsonObj[methodParam] == methodWho) {
        jsonDoc.clear();
        jsonDoc[methodParam] = methodWho;
        jsonDoc[paramFirmwareVersion] = firmwareVersion;
        btSendMessage();
      } else if (jsonObj[methodParam] == methodSync) {
        encodeCurrent();
        jsonDoc[methodParam] = methodSync;
        btSendMessage();
        displayParamSent();
      } else if (jsonObj[methodParam] == methodAutomaticEnd) {
        jsonDoc.clear();
        jsonDoc[methodParam] = methodAutomaticEnd;
        btSendMessage();
      } else if (jsonObj[methodParam] == methodSet) {
        setParams();
        encodeCurrent();
        jsonDoc[methodParam] = methodSet;
        btSendMessage();
      } else if (jsonObj[methodParam] == methodCommunicationError) {
        displayCommError();
      } else {
        jsonDoc.clear();
        jsonDoc[methodParam] = methodUnknown;
        btSendMessage();
      }
    }
  }
}

void setParams() {
  if (jsonObj.containsKey(paramOpPause)) {
    setPause(stob(jsonObj[paramOpPause]));
  }
  if (jsonObj.containsKey(paramOpRamp)) {
    setRamp(stob(jsonObj[paramOpRamp]));
  }
  if (jsonObj.containsKey(paramOpAngle)) {
    angle = constrain(stoi(jsonObj[paramOpAngle]), 1, 180);
  }
  if (jsonObj.containsKey(paramOpWop)) {
    wop = constrain(stoi(jsonObj[paramOpWop]), 1, maxWop);
  }
  if (jsonObj.containsKey(paramOpRampStart)) {
    rampStart = constrain(stoi(jsonObj[paramOpRampStart]), 1, rampEnd - 1);
  }
  if (jsonObj.containsKey(paramOpRampEnd)) {
    rampEnd = constrain(stoi(jsonObj[paramOpRampEnd]), rampStart + 1, maxWop);
  }
  if (jsonObj.containsKey(paramOpRampDuration)) {
    rampDuration = constrain(stoi(jsonObj[paramOpRampDuration]), 1, maxRampDuration);
  }
  if (jsonObj.containsKey(paramOpAutoEnd)) {
    autoEnd = false; // set to false always if it is set
  }
  if (jsonObj.containsKey(paramOpDisplayActive)) {
    displayActive = stob(jsonObj[paramOpDisplayActive]);
    if (!displayActive)
      display.clear();
  }
  if (jsonObj.containsKey(paramOpDisplayUpdateInterval)) {
    displayUpdateInterval = constrain(stoi(jsonObj[paramOpDisplayUpdateInterval]), 1, maxIntervalUpdate);
  }
  if (jsonObj.containsKey(paramOpDisplayBacklight)) {
    displayBacklight = stob(jsonObj[paramOpDisplayBacklight]);
    if (displayBacklight)
      display.backlight();
    else
      display.noBacklight();
  }
  if (jsonObj.containsKey(paramOpBufferTemperatureUpdateInterval)) {
    bufferTemperatureUpdateInterval = constrain(stoi(jsonObj[paramOpBufferTemperatureUpdateInterval]), 1, maxIntervalUpdate);
  }
  if (jsonObj.containsKey(paramOpBufferTemperatureAutomaticControl)) {
    bufferTemperatureAutomaticControl = stob(jsonObj[paramOpBufferTemperatureAutomaticControl]);
  }
  if (jsonObj.containsKey(paramOpBufferTemperatureManualControlOn)) {
    bufferTemperatureManualControlOn = stob(jsonObj[paramOpBufferTemperatureManualControlOn]);
  }
  if (jsonObj.containsKey(paramOpBufferTemperatureSetpoint)) {
    bufferTemperatureSetpoint = constrain(stoi(jsonObj[paramOpBufferTemperatureSetpoint]), bufferTemperatureSetpointMin, bufferTemperatureSetpointMax);
  }
  if (jsonObj.containsKey(paramOpBufferTemperatureMaxError)) {
    bufferTemperatureMaxError = constrain(stoi(jsonObj[paramOpBufferTemperatureMaxError]), bufferTemperatureMaxErrorMin, bufferTemperatureMaxErrorMax);
  }
  if (jsonObj.containsKey(paramOpServoSpeed)) {
    servoSpeed = constrain(stoi(jsonObj[paramOpServoSpeed]), 0, 255);
  }
  // on/off at the end
  if (jsonObj.containsKey(paramOpOnOff)) {
    setOnOff(stob(jsonObj[paramOpOnOff]));
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
      setOnOff(false);
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
  servo.attach(servoPin, servoUsFrom, servoUsTo);
  delay(15);
  servo.write(finalAngle, servoSpeed, waitForMotorMove);
  servo.wait();
  delay(15);
  servo.detach();
}

void loopDisplay() {
  if (displayTimer.hasPassed(displayUpdateInterval)) {
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
            lastDisplayParameter = 0;
          }
          lastDisplayState = 3;
          display.clear();

          display.setCursor(0, 0);
          display.print(F("Running "));

          // Timer
          display.print(secondsToTime(runTimer.elapsed()));
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
  if (bufferTemperatureTimer.hasPassed(bufferTemperatureUpdateInterval)) {
    if (bufferTemperatureAutomaticControl) {
      // decide if cooling is needed
      if (bufferTemperature > bufferTemperatureSetpoint + bufferTemperatureMaxError) { // start cooling
        bufferCoolingOn = true;
      } else {
        if (bufferTemperature < bufferTemperatureSetpoint) // stop cooling if temperature reaches set point (-1)
          bufferCoolingOn = false;
      }
    } else { // check manual control
      if (bufferTemperatureManualControlOn) {
        bufferCoolingOn = true;
      } else {
        bufferCoolingOn = false;
      }
    }
    controlCoolingsystem();
    bufferTemperature = readTemperature();
    bufferTemperatureTimer.restart();
  }
}

void controlCoolingsystem() {
  if (!bufferTemperatureSystemReadyOnState) {
    if (bufferCoolingOn) { // start cooling
      if (!pumpOn) {
        digitalWrite(pumpPin, HIGH);
        pumpOn = true;
      } else if (!fansOn) {
        digitalWrite(fan1Pin, HIGH);
        digitalWrite(fan2Pin, HIGH);
        fansOn = true;
      } else if (!peltier1On) {
        digitalWrite(peltier1Pin, HIGH);
        peltier1On = true;
      } else if (!peltier2On) {
        digitalWrite(peltier2Pin, HIGH);
        peltier2On = true;
      }
    } else { // stop cooling
      if (peltier2On) {
        digitalWrite(peltier2Pin, LOW);
        peltier2On = false;
      } else if (peltier1On) {
        digitalWrite(peltier1Pin, LOW);
        peltier1On = false;
      } else if (pumpOn) {
        digitalWrite(pumpPin, LOW);
        pumpOn = false;
      } else if (fansOn) {
        digitalWrite(fan1Pin, LOW);
        digitalWrite(fan2Pin, LOW);
        fansOn = false;
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

void encodeCurrent() {
  jsonDoc.clear();
  jsonDoc[paramOpOnOff] = btos(onoff);
  jsonDoc[paramOpPause] = btos(pause);
  jsonDoc[paramOpRamp] = btos(ramp);
  jsonDoc[paramOpAngle] = angle;
  jsonDoc[paramOpWop] = wop;
  jsonDoc[paramOpRampStart] = rampStart;
  jsonDoc[paramOpRampEnd] = rampEnd;
  jsonDoc[paramOpRampDuration] = rampDuration;
  jsonDoc[paramOpAutoWop] = wopAuto;
  jsonDoc[paramOpBufferTemperature] = bufferTemperature;
  jsonDoc[paramOpHasRun] = runTimer.elapsed();
  jsonDoc[paramOpAutoEnd] = btos(autoEnd);
  jsonDoc[paramOpDisplayActive] = btos(displayActive);
  jsonDoc[paramOpDisplayUpdateInterval] = displayUpdateInterval;
  jsonDoc[paramOpBufferTemperatureUpdateInterval] = bufferTemperatureUpdateInterval;
  jsonDoc[paramOpBufferTemperatureAutomaticControl] = btos(bufferTemperatureAutomaticControl);
  jsonDoc[paramOpBufferTemperatureManualControlOn] = btos(bufferTemperatureManualControlOn);
  jsonDoc[paramOpBufferTemperatureSetpoint] = bufferTemperatureSetpoint;
  jsonDoc[paramOpBufferTemperatureMaxError] = bufferTemperatureMaxError;
  jsonDoc[paramOpDisplayBacklight] = btos(displayBacklight);
  jsonDoc[paramOpServoSpeed] = servoSpeed;
}

void btSendMessage() {
  serializeJson(jsonDoc, BT);
  BT.println();
  /*Serial.println("sending");
    serializeJson(jsonDoc, Serial);
    Serial.println("");*/
}

bool requestBtData() {
  if (BT.available()) {
    jsonErr = deserializeJson(jsonDoc, BT);
    if (!jsonErr) {
      jsonObj = jsonDoc.as<JsonObject>();
      /*Serial.println("recibiendo");
            serializeJson(jsonDoc, Serial);
        Serial.println("");*/
      return true;
    }
  }
  return false;
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

int stoi(char * convert) {
  return atoi(convert);
}

bool stob(char * convert) {
  return strcmp(convert, "t") == 0 ? true : false;
}

String btos(bool convert) {
  return convert ? "t" : "f";
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
  steinhart = average / THERMISTORNOMINAL;          // (R/Ro)
  steinhart = log(steinhart);                       // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                        // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                      // Invert
  steinhart -= 273.15;                              // convert to C

  return (int) steinhart;
}
