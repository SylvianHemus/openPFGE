// connections
// servo: Pin 9 (& 5v (4.8 - 7.2 V) to power source)
// thermoresistor: GND and A0 (& 10 kohm from Pin A0 to 3.3V && wire 3.3V with AREF on arduino)
// bluetooth hc-05: RX --> Pin 11 & TX --> Pin 10 (& 5v to power source)
// OLED/I2C VCC -> 5V / GND -> GND / SDA -> A4 / SCK/SCL -> A5
// Mosfet Pump --> Pin 3
// Mosfet Fan 1 --> Pin 5
// Mosfet Fan 2 --> Pin 6
// Mosfet Peltier 1 --> Pin 7
// Mosfet Peltier 2 --> Pin 8

// libraries
#include <SoftwareSerial.h>
#include <Chrono.h>
#include <VarSpeedServo.h>
#include <SSD1306AsciiAvrI2c.h>

// firmware
#define firmwareVersion 1
#define firmwareSubversion 0

// debug
#define serialDebug true // should the program outputs the debug by serial port
bool serialStarted = false; // serial started?

// motor servo
VarSpeedServo servo; // the servo object
#define servoPin 9 // PWM signal pin
#define servoUsFrom 500 // minimum microseconds // for ds3218
#define servoUsTo 2500 // maximum microseconds // for ds3218
#define servoVelocity 255 // 0=full speed, 1-255 slower to faster
#define waitForMotorMove true // programs wait until motor end moving
int motorPosition = 0; // store current motor position (-1 = left, 0 = center, 1 = right)

// bluetooth
SoftwareSerial BT(10, 11); // TX, RX

// OLED / I2C
#define I2C_ADDRESS 0x3C
#define RST_PIN -1
SSD1306AsciiAvrI2c display;
Chrono displayTimer(Chrono::SECONDS); // Chrono for info update and view change
bool displayActive = true; // update display info?
int displayUpdateInfoInterval = 2; // display update info interval (seconds)
#define oledLineHeight 1
int lastDisplayState = -1;

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
String inData; // capture serial inputs
String method; // switch commands from serial input
char tmpBuffer[100]; // temporal
bool autoEnd = false; // whether the program has automaticlly ended
#define maxIntervalUpdate 60 // 60 seconds for maximum update interval
int bufferTemperatureUpdateInterval = 10; // buffer temperature update interval (seconds)
int oledLine = 1;
#define methodSync "y"
#define methodSet "s"
#define methodWho "w"
#define methodAutomaticEnd "a"
#define methodUnknown "u"
#define methodCommunicationError "c"

// timers
Chrono runTimer(Chrono::SECONDS); // running timer
Chrono stepTimer(Chrono::SECONDS); // step timer
Chrono bufferTemperatureTimer(Chrono::SECONDS); // buffer temperature update timer

void setup() {
  // Chronos
  runTimer.stop();
  stepTimer.stop();
  bufferTemperatureTimer.stop();
  displayTimer.stop();

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
  centerMotor();

  // BT series port initialice (For Mode AT 2)
  BT.begin(9600);

  // debug
  serialDebugWrite(F("Setup done"));

  // display init and splash
  display.begin(&Adafruit128x64, I2C_ADDRESS);
  display.clear();
  display.setFont(System5x7);
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
    requestString();

    // test message is complete
    if (!inData.substring(0, 1).equals("<") || !inData.substring(inData.length() - 1).equals(">")) {
      sprintf(tmpBuffer, "m=%s", methodCommunicationError);
      btSendMessage(tmpBuffer);
      return;
    }
    inData.remove(0, 1);
    inData.remove(inData.length() - 1);

    strcpy(tmpBuffer, inData.c_str());

    strtok(strtok(tmpBuffer, "@"), "=");
    method = strtok(NULL, "=");


    if (method == methodWho) {
      sprintf(tmpBuffer, "m=%s@fv=%d@fs=%d", methodWho, firmwareVersion, firmwareSubversion);
      btSendMessage(tmpBuffer);
    } else if (method == methodSync) {
      encodeCurrent();
      sprintf(tmpBuffer + strlen(tmpBuffer), "@m=%s", methodSync);
      btSendMessage(tmpBuffer);
    } else if (method == methodAutomaticEnd) {
      sprintf(tmpBuffer, "m=%s", methodAutomaticEnd);
      btSendMessage(tmpBuffer);
    } else if (method == methodSet) {
      setParams();
      encodeCurrent();
      sprintf(tmpBuffer + strlen(tmpBuffer), "@m=%s", methodSet);
      btSendMessage(tmpBuffer);
    } else if (method == methodCommunicationError) {
      displayCommError();
    } else {
      sprintf(tmpBuffer, "m=%s", methodUnknown);
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
    } else if (strcmp(pch, "da") == 0) {
      displayActive = stob(pchv);
    } else if (strcmp(pch, "dui") == 0) {
      displayUpdateInfoInterval = constrain(stoi(pchv), 1, maxIntervalUpdate);
    } else if (strcmp(pch, "bui") == 0) {
      bufferTemperatureUpdateInterval = constrain(stoi(pchv), 1, maxIntervalUpdate);
    } else if (strcmp(pch, "btac") == 0) {
      bufferTemperatureAutomaticControl = stob(pchv);
    } else if (strcmp(pch, "bts") == 0) {
      bufferTemperatureSetpoint = constrain(stoi(pchv), bufferTemperatureSetpointMin, bufferTemperatureSetpointMax);
    } else if (strcmp(pch, "btm") == 0) {
      bufferTemperatureMaxError = constrain(stoi(pchv), bufferTemperatureMaxErrorMin, bufferTemperatureMaxErrorMax);
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
      centerMotor();
      runTimer.stop();
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
  servo.write(map(finalAngle, 0, 180, servoUsFrom, servoUsTo), servoVelocity, waitForMotorMove);
  servo.detach();
}

void loopDisplay() {
  if (displayActive && (!displayTimer.isRunning() || displayTimer.hasPassed(displayUpdateInfoInterval))) {
    displayTimer.restart();
    if (autoEnd) {
      if (lastDisplayState == 1) return;
      lastDisplayState = 1;
      display.clear();
      display.set2X();
      display.setCursor(10, 0);
      display.print(F("automatic"));
      display.setCursor(50, 20);
      display.print(F("END"));
      display.setCursor(10, 50);
      display.print(secondsToTime(runTimer.elapsed()));
    } else {
      // OnOff
      if (onoff) { // system on or paused
        if (pause) { // system paused
          if (lastDisplayState == 2) return;
          lastDisplayState = 2;
          display.clear();
          display.set2X();
          display.setCursor(30, 1);
          display.print(F("Paused"));
          display.setCursor(20, 5);
          display.print(secondsToTime(runTimer.elapsed()));
        } else { // system on
          if (lastDisplayState != 3) {
            display.clear();
          }
          lastDisplayState = 3;
          oledLine = 1;
          display.set1X();

          display.setCursor(0, 0);
          display.println(F("System / Running"));

          // Timer
          display.setCursor(0, oledLineHeight * oledLine);
          oledLine++;
          display.print(F("Run time / "));
          display.println(secondsToTime(runTimer.elapsed()));

          // Angle
          display.setCursor(0, oledLineHeight * oledLine);
          oledLine++;
          display.print(F("Angle / "));
          display.print(angle);
          display.println(F("°"));

          // Buffer temperature
          display.setCursor(0, oledLineHeight * oledLine);
          oledLine++;
          display.print(F("Buffer temp. / "));
          display.print(bufferTemperature);
          display.println(F(" C"));

          // Ramp
          if (!ramp) {
            display.setCursor(0, oledLineHeight * oledLine);
            oledLine++;
            display.println(F("Ramp / Off"));
          }

          // Wop/ramp
          display.setCursor(0, oledLineHeight * oledLine);
          oledLine++;
          if (ramp) {
            display.print(F("Ramp-WOP / "));
            display.print(wopAuto);
            display.println(F(" s"));
            
            display.setCursor(0, oledLineHeight * oledLine);
            oledLine++;
            display.print(F("Ramp-Start / "));
            display.print(rampStart);
            display.println(F(" s"));
            
            display.setCursor(0, oledLineHeight * oledLine);
            oledLine++;
            display.print(F("Ramp-End / "));
            display.print(rampEnd);
            display.println(F(" s"));
            
            display.setCursor(0, oledLineHeight * oledLine);
            oledLine++;
            display.print(F("Ramp-Duration / "));
            display.print(rampDuration);
            display.println(F(" h"));
          } else {
            display.print(F("WOP / "));
            display.print(wop);
            display.println(F(" s"));
          }
        }
      } else { // system off
        if (lastDisplayState == 4) return;
        lastDisplayState = 4;
        display.clear();
        display.set2X();
        display.setCursor(30, 1);
        display.print(F("System"));
        display.setCursor(50, 5);
        display.println(F("Off"));
      }
    }
  }
}

void displaySplash() {
  if (displayActive) {
    display.clear();
    display.set2X();
    display.setCursor(40, 1);
    display.println(F("open"));
    display.setCursor(40, 5);
    display.println(F("PFGE"));

    delay(2000);
  }
}

void displayParamUpdated() {
  if (displayActive) {
    display.clear();

    display.set2X();
    display.setCursor(5, 1);
    display.println(F("Parameters"));
    display.setCursor(25, 5);
    display.println(F("Updated"));
    lastDisplayState = -1;

    delay(1000);
  }
}

void displayCommError() {
  if (displayActive) {
    display.clear();

    display.set2X();
    display.setCursor(5, 1);
    display.println(F("Communication"));
    display.setCursor(30, 5);
    display.println(F("Error"));
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

String requestString() {
  if (BT.available()) {
    inData =  BT.readString();
    return;
  }
  inData =  "";
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
  sprintf(tmpBuffer, "o=%s@p=%s@r=%s@a=%d@w=%d@rs=%d@re=%d@rd=%d@aw=%d@bt=%d@hr=%lu@ae=%s@da=%s@dui=%d@bui=%d@btac=%s@bts=%d@btm=%d",
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
          btos(displayActive), // from here, deep config
          displayUpdateInfoInterval,
          bufferTemperatureUpdateInterval, // end deep config
          btos(bufferTemperatureAutomaticControl),
          bufferTemperatureSetpoint,
          bufferTemperatureMaxError
         );
}

void btSendMessage(String message) {
  btSendMessage(message, true);
}

void btSendMessage(String message, bool newLine) {
  message = "<" + message + ">";
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
  sprintf(str, "%d:%02d:%02d", h, m, s);
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
