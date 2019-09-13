// connections
// servo: Pin 9 (& 5v (4.8 - 7.2 V) to power source)
// thermoresistor: GND and A0 (& 10 kohm from Pin A0 to 3.3V && wire 3.3V with AREF on arduino)
// bluetooth hc-05: RX --> Pin 11 & TX --> Pin 10 (& 5v to power source)
// LCD/I2C VCC -> 5V / GND -> GND / SDA -> A4 / SCL -> A5

// libraries
#include <SoftwareSerial.h>
#include <Chrono.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// firmware
#define firmwareVersion 1
#define firmwareSubversion 0

// debug
#define serialDebug false // should the program outputs the debug by serial port
bool serialStarted = false; // serial started?

// motor servo
Servo servo; // the servo object
#define servoPin 9 // PWM signal pin
#define waitForMotorMove 250 // microseconds to wait after send a new position of the servo
#define servoUsFrom 500 // minimum microseconds // for ds3218
#define servoUsTo 2500 // maximum microseconds // for ds3218
int motorPosition = 0; // store current motor position (-1 = left, 0 = center, 1 = right)

// bluetooth
SoftwareSerial BT(10, 11); // TX, RX

// LCD / I2C
LiquidCrystal_I2C lcd(0x27, 16, 2); // Direction 0x27 & 16 cols & 2 rows
Chrono lcdTimer(Chrono::SECONDS); // Chrono for info update and view change
bool lcdActive = true; // update lcd info?
bool lcdView = 0; // lcd info is shown in diferent views. This holds the current view
bool lcdBacklight = false; // backlight on/off
int lcdUpdateInfoInterval = 2; // lcd update info interval (seconds)

// thermoresistor
int bufferTemperature = 0; // current read of buffer temperature
int bufferTemperatureUpdateInterval = 10; // buffer temperature update interval (seconds)

// running parameters
bool onoff = false; // System on/off
bool pause = false; // System paused on/off
bool ramp = false; // System ramp on/off
int angle = 120; // Turning angle
int wop = 2; // between each movement in ramp off mode (seconds)
int wopAuto = 0; // // between each movement in ramp on mode (seconds)
#define maxWop 1000 // max wop value for ramp start/end (seconds)
#define maxRampDuration 100 // ramp max duration (hours)
int rampStart = 1; // initial time movement in ramp on mode (seconds)
int rampEnd = 25; // final time movement in ramp on mode (seconds)
int rampDuration = 24; // ramp time in ramp on mode (hours)

// variables
String inData; // capture serial inputs
String method; // switch commands from serial input
#define tmpBufferSize 100
char tmpBuffer[tmpBufferSize]; // temporal
bool autoEnd = false; // whether the program has automaticlly ended
#define maxIntervalUpdate 60 // 60 seconds for maximum update interval

// timers
Chrono runTimer(Chrono::SECONDS); // running timer
Chrono stepTimer(Chrono::SECONDS); // step timer
Chrono bufferTemperatureTimer(Chrono::SECONDS); // buffer temperature update timer

void setup() {
  // Chrono
  runTimer.stop();
  stepTimer.stop();
  bufferTemperatureTimer.stop();

  // Motor init and center
  centerMotor();

  // LCD
  lcd.init();
  lcd.noBacklight();
  lcdTimer.stop();

  // thermoresistor
  analogReference(EXTERNAL); // Connect AREF to 3.3V!

  // BT series port initialice (For Mode AT 2)
  BT.begin(9600);

  // debug
  serialDebugWrite("Setup done");
}

void loop() {
  // Check whether is there anything at serial
  checkSerial();

  // Make the next motor move
  if (onoff && !pause) {
    nextMotorMove();
  }

  // buffer temperature loop
  loopBufferTemperature();

  // lcd loop
  loopLcd();
}

void checkSerial() {
  if (BT.available())
  {
    inData = requestString();
    serialDebugWrite("inData | " + inData);

    strcpy(tmpBuffer, inData.c_str());

    strtok(strtok(tmpBuffer, "@"), "=");
    method = strtok(NULL, "=");

    serialDebugWrite("method | " + method);

    if (method == "w") {
      sprintf(tmpBuffer, "m=w@fv=%d@fs=%d", firmwareVersion, firmwareSubversion);
      btSendMessage(tmpBuffer);
    } else if (method == "g") {
      encodeCurrent();
      sprintf(tmpBuffer + strlen(tmpBuffer), "@m=g");
      btSendMessage(tmpBuffer);
    } else if (method == "a") {
      sprintf(tmpBuffer, "m=a@res=ok");
      btSendMessage(tmpBuffer);
    } else if (method == "s") {
      setParams();
    } else {
      sprintf(tmpBuffer, "method=u");
      btSendMessage(tmpBuffer);
    }
  }
}

void setParams() {
  strcpy(tmpBuffer, inData.c_str());
  char * pch;
  char * pchv;
  pch = strtok(tmpBuffer, "@=");
  while (pch != NULL)
  {
    pchv = strtok(NULL, "@=");
    if (strcmp(pch, "o") == 0) {
      setOnOff(stob(pchv));
    } else if (strcmp(pch, "p") == 0) {
      setPause(stob(pchv));
    } else if (strcmp(pch, "r") == 0) {
      setRamp(stob(pchv));
    } else if (strcmp(pch, "a") == 0) {
      angle = constrain(stoi(pchv), 1, 180);
    } else if (strcmp(pch, "w") == 0) {
      wop = constrain(stoi(pchv), 1, maxWop);
    } else if (strcmp(pch, "rs") == 0) {
      rampStart = constrain(stoi(pchv), 1, rampEnd - 1);
    } else if (strcmp(pch, "re") == 0) {
      rampEnd = constrain(stoi(pchv), rampStart + 1, maxWop);
    } else if (strcmp(pch, "rd") == 0) {
      rampDuration = constrain(stoi(pchv), 1, maxRampDuration);
    } else if (strcmp(pch, "ae") == 0) {
      autoEnd = false; // set to false always if it is set
    } else if (strcmp(pch, "la") == 0) {
      lcdActive = stob(pchv);
    } else if (strcmp(pch, "lb") == 0) {
      lcdBacklight = stob(pchv);
    } else if (strcmp(pch, "lui") == 0) {
      lcdUpdateInfoInterval = constrain(stoi(pchv), 1, maxIntervalUpdate);
    } else if (strcmp(pch, "bui") == 0) {
      bufferTemperatureUpdateInterval = constrain(stoi(pchv), 1, maxIntervalUpdate);
    }
    pch = strtok(NULL, "@=");
  }
  encodeCurrent();
  sprintf(tmpBuffer + strlen(tmpBuffer), "@m=s@res=ok");
  btSendMessage(tmpBuffer);
}

void nextMotorMove() {
  int currentwop = wop;
  if (ramp) {
    currentwop = wopAuto;
  }
  if (runTimer.isRunning() && stepTimer.elapsed() > currentwop) {
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

void setNextWopAuto() {
  if (ramp) {
    if (runTimer.hasPassed((long) rampDuration * 3600)) {
      // Run end time reached
      centerMotor();
      runTimer.stop();
      serialDebugWrite("Automatic System Off [Run end time reached]");
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
  servo.writeMicroseconds(map(finalAngle, 0, 180, servoUsFrom, servoUsTo));
  delay(waitForMotorMove);
  servo.detach();
}

void loopLcd() {
  if (!lcdTimer.isRunning() | lcdTimer.hasPassed(lcdUpdateInfoInterval)) {
    if (lcdActive) {
      lcd.clear();
      if (lcdBacklight) {
        lcd.backlight();
      } else {
        lcd.noBacklight();
      }
      if (autoEnd) {
        lcd.print("AUTO END");
        // Has run
        lcd.setCursor(0, 1);
        lcd.print(secondsToTime(runTimer.elapsed()));
      } else {
        if (lcdView == 0) {
          // OnOff
          if (onoff) {
            if (pause) {
              lcd.print("Pause");
            } else {
              lcd.print("On");
            }
          } else {
            lcd.print("Off");
          }
          // Has run
          lcd.setCursor(8, 0);
          lcd.print(secondsToTime(runTimer.elapsed()));
          // Ramp
          lcd.setCursor(0, 1);
          lcd.print("Ramp/");
          if (ramp) {
            lcd.print("On");
          } else {
            lcd.print("Off");
          }
          // Angle
          lcd.setCursor(9, 1);
          lcd.print("Ang/");
          lcd.print(angle);

          lcdView = 1;
        } else if (lcdView == 1) {
          // Buffer temperature
          lcd.print("BT/");
          lcd.print(bufferTemperature);
          lcd.setCursor(8, 0);
          lcd.print("WOP/");
          if (ramp) {
            lcd.print(wopAuto);
            lcd.setCursor(0, 1);
            lcd.print("S/");
            lcd.print(rampStart);
            lcd.print(" E/");
            lcd.print(rampEnd);
            lcd.print(" D/");
            lcd.print(rampDuration);
          } else {
            lcd.print(wop);
          }

          lcdView = 0;
        }
      }
      lcdTimer.restart();
    } else {
      lcd.clear();
      lcd.noBacklight();
    }
  }
}

void loopBufferTemperature() {
  if (!bufferTemperatureTimer.isRunning() || bufferTemperatureTimer.hasPassed(bufferTemperatureUpdateInterval)) {
    bufferTemperature = readTemperature();
    bufferTemperatureTimer.restart();
  }
}

void setOnOff(bool newOnOff) {
  if (onoff == newOnOff) {
    serialDebugWrite("setOnOff | Already in the state ");
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
    serialDebugWrite("setPause | Already in the state ");
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
    serialDebugWrite("setRamp | Already in the state ");
    return;
  }
  if (onoff) {
    runTimer.restart();
    stepTimer.restart();
  }
  ramp = newRamp;
}

String requestStringMaxSize(int strLength) {
  String btRead = requestString();
  if (strLength == -1) {
    strLength = btRead.length() - 2;
  }
  btRead.remove(strLength);
  return btRead;
}

String requestString() {
  if (BT.available()) {
    return BT.readString();
  }
  return "";
}

void serialDebugWrite(String outputtext) {
  if (serialDebug) {
    if (!serialStarted) {
      Serial.begin(9600);
      serialStarted = true;
    }
    Serial.print(millis());
    Serial.print(" | ");
    Serial.println(outputtext);
  }
}

void encodeCurrent() {
  sprintf(tmpBuffer, "o=%s@p=%s@r=%s@a=%d@w=%d@rs=%d@re=%d@rd=%d@aw=%d@bt=%d@hr=%lu@ae=%s@la=%s@lb=%s@lui=%d@bui=%d",
          btos(onoff),
          btos(pause),
          btos(ramp),
          angle,
          wop,
          rampStart,
          rampEnd,
          rampDuration,
          wopAuto, // from here, just output
          bufferTemperature,
          runTimer.elapsed(),
          btos(autoEnd), // end just output
          btos(lcdActive), // from here, deep config
          btos(lcdBacklight),
          lcdUpdateInfoInterval,
          bufferTemperatureUpdateInterval // end deep config
         );
}

void btSendMessage(String message) {
  btSendMessage(message, true);
}

void btSendMessage(String message, bool newLine) {
  serialDebugWrite("BT | " + message);
  if (newLine) {
    BT.println(message);
  } else {
    BT.print(message);
  }
}

int stoi(String convert) {
  return convert.toInt();
}

bool stob(char * convert) {
  return strcmp(convert, "t") == 0 ? true : false;
}

char * btos(bool convert) {
  return convert ? "t" : "f";
}

char * secondsToTime(unsigned long t)
{
  static char str[12];
  sprintf(str, "");
  int h = (int) t / 3600;
  t = t % 3600;
  int m = (int) t / 60;
  int s = (int) t % 60;
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
