#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <Wire.h>

#define USE_ARDUINO_INTERRUPTS true

const int MPU_addr = 0x68;  // I2C address of the MPU-6050
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
float lastLat = 28.367047, lastLng = 77.317573;

const int LED = LED_BUILTIN;

static const int RXPin = 4, TXPin = 3;
static const uint32_t GPSBaud = 9600;
boolean fall = true;       //stores if a fall has occurred
boolean locSaved = false;  //stores if a fall has occurred
boolean trigger1 = false;
boolean trigger2 = false;  //stores if second trigger (upper threshold) has occurred
boolean trigger3 = false;  //stores if third trigger (orientation change) has occurred
byte trigger1count = 0;    //stores the counts past since trigger 1 was set true
byte trigger2count = 0;    //stores the counts past since trigger 2 was set true
byte trigger3count = 0;    //stores the counts past since trigger 3 was set true
int angleChange = 0;
int AM;
const int pushBtn = 4;
const int ledSafe = 9;

TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);  // for gps
SoftwareSerial mySerial(5, 6);    // for gsm  6 to tx and 5 to rx


void setup() {
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  Serial.begin(9600);
  mySerial.begin(9600);

  ss.begin(GPSBaud);
  pinMode(ledSafe, OUTPUT);
  pinMode(pushBtn, INPUT);
  pinMode(11, OUTPUT);
}

void loop() {
  mpu_read();
  locSaved = false;
  ax = (AcX) / 16384.00;
  ay = (AcY) / 16384.00;
  az = (AcZ) / 16384.00;

  //270, 351, 136 for gyroscope
  gx = (GyX) / 131.07;
  gy = (GyY) / 131.07;
  gz = (GyZ) / 131.07;

  // calculating Amplitute vactor for 3 axis
  float Raw_AM = pow(pow(ax, 2) + pow(ay, 2) + pow(az, 2), 0.5);
  angleChange = pow(pow(gx, 2) + pow(gy, 2) + pow(gz, 2), 0.5);
  AM = Raw_AM * 10;
  Serial.println(AM);
  sendtoNode();
  if (trigger3 == true) {
    trigger3count++;
    if (trigger3count >= 10) {
      if ((angleChange >= 0) && (angleChange <= 10)) {  //if orientation changes remains between 0-10 degrees
        fall = true;
        trigger3 = false;
        trigger3count = 0;
      } else {  //user regained normal orientation
        trigger3 = false;
        trigger3count = 0;
      }
    }
  }
  if (fall == true) {  //in event of a fall detection
    Serial.println("FALL DETECTED");
    sendtoNode();
    for (int i = 0; i < 3; i++) {
      digitalWrite(11, HIGH);
      digitalWrite(10, HIGH);
      delay(400);
      digitalWrite(11, LOW);
      digitalWrite(10, LOW);
      sendLoc();
    }
    fall = false;
    if (digitalRead(pushBtn) == HIGH) {
      int on = 1;
      for (int i = 0; i < 10; i++) {
        if (digitalRead(pushBtn) == !HIGH) {
          on = 0;
          break;
        }
        delay(100);
      }

      if (on == 1) {
        Serial.println("ON");
        digitalWrite(ledSafe, HIGH);
        displaySafeInfo();
        delay(1000);
      }
    } else {
      Serial.println("OFF");
      digitalWrite(ledSafe, LOW);
    }
  }
  if (trigger2count >= 6) {  //allow 0.5s for orientation change
    trigger2 = false;
    trigger2count = 0;
    Serial.println("TRIGGER 2 DEACTIVATED");
  }
  if (trigger1count >= 6) {  //allow 0.5s for AM to break upper threshold
    trigger1 = false;
    trigger1count = 0;
    Serial.println("TRIGGER 1 DEACTIVATED");
  }
  if (trigger2 == true) {
    trigger2count++;
    //angleChange=acos(((double)x*(double)bx+(double)y*(double)by+(double)z*(double)bz)/(double)AM/(double)BM);
    angleChange = pow(pow(gx, 2) + pow(gy, 2) + pow(gz, 2), 0.5);
    // Serial.println(angleChange);
    if (angleChange >= 30 && angleChange <= 400) {  //if orientation changes by between 80-100 degrees
      trigger3 = true;
      trigger2 = false;
      trigger2count = 0;
      // Serial.println(angleChange);
      Serial.println("TRIGGER 3 ACTIVATED");
    }
  }
  if (trigger1 == true) {
    trigger1count++;
    if (AM >= 11) {  //if AM breaks upper threshold (3g)
      trigger2 = true;
      Serial.println("TRIGGER 2 ACTIVATED");
      trigger1 = false;
      trigger1count = 0;
    }
  }
  if (AM <= 2 && trigger2 == false) {  //if AM breaks lower threshold (0.4g)
    trigger1 = true;
    Serial.println("TRIGGER 1 ACTIVATED");
  }

  saveLoc();

  delay(100);
}

void sendtoNode() {
  Serial.print("::");
  Serial.print(AM);
  Serial.print(",");
  Serial.print(lastLat);
  Serial.print(",");
  Serial.print(lastLng);
  Serial.print(",");
  Serial.print(angleChange);
  Serial.print(",");
  Serial.println(fall ? 1 : 0);
}

void saveLoc() {
  while (ss.available() > 0) {
    if (gps.encode(ss.read())) {
      saveInfo();
    }
    if (locSaved) {
      break;
    }
  }
  if (millis() > 5000 && gps.charsProcessed() < 10) {
    Serial.println(F("No GPS detected: check wiring."));
  }
}
void sendLoc() {
  displayInfo();
}

void saveInfo() {
  if (gps.location.isValid()) {

    Serial.println(F("Location saved. "));
    lastLat = gps.location.lat();
    lastLng = gps.location.lng();
    locSaved = true;
  }
}

void displaySafeInfo() {

  Serial.print(F("Sending Location: "));
  String message;
  if (lastLat != 200 && lastLng != 200) {
    message = "I am safe. No emergency detected.\n\nLast Known Location: \nhttps://maps.google.com?q=";
    message += String(lastLat, 6);        // assuming 6 decimal places for latitude
    message += "," + String(lastLng, 6);  // assuming 6 decimal places for longitude
    message += "\nLast HeartRate Measured: ";
    message += "103";
    message += "\n\nThank you for your concern.";
  } else {
    message = "I am safe. No emergency detected.\n\nLast Known Location: Not available";
    message += "\nLast HeartRate Measured: Not available";
    message += "\n\nThank you for your concern.";
  }


  mySerial.println("AT");
  updateSerial();

  mySerial.println("AT+CMGF=1");
  updateSerial();
  mySerial.println("AT+CMGS=\"+917011263403\"");  // enter your phone number here (prefix country code)
  updateSerial();
  mySerial.print(message);
  updateSerial();
  mySerial.write(26);

  Serial.println(message);
}

void updateSerial() {
  delay(300);
  while (Serial.available()) {
    mySerial.write(Serial.read());
  }
  while (mySerial.available()) {
    Serial.write(mySerial.read());
  }
}

void displayInfo() {

  // Serial.print(F("Sending Location: "));
  String message;
  if (lastLat != 200 && lastLng != 200) {
    message = "EMERGENCY ALERT! \nFall Detected!\n\nA fall has been detected. \nLast Known Location: \nhttps://maps.google.com?q=";
    message += String(lastLat, 6);        // assuming 6 decimal places for latitude
    message += "," + String(lastLng, 6);  // assuming 6 decimal places for longitude
    message += "\nLast HeartRate Measured: ";
    message += "103";
    message += "\n\nPlease check on the person as soon as possible";
    // Serial.println(message);
  } else {
    message = "EMERGENCY ALERT! \nFall Detected!\n\nA fall has been detected. \nLast Known Location: Not available";
    message += "\nLast HeartRate Measured: ";
    message += "\nPlease check on the person as soon as possible. \n\nYour prompt attention to this matter is crucial.";
  }

  mySerial.println("AT");
  updateSerial();

  mySerial.println("AT+CMGF=1");
  updateSerial();
  mySerial.println("AT+CMGS=\"+917011263403\"");  // enter your phone number here (prefix country code)
  updateSerial();
  mySerial.print(message);
  updateSerial();
  mySerial.write(26);

  Serial.println();
}


void mpu_read() {
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 14, true);  // request a total of 14 registers
  AcX = Wire.read() << 8 | Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  AcY = Wire.read() << 8 | Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ = Wire.read() << 8 | Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Tmp = Wire.read() << 8 | Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX = Wire.read() << 8 | Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY = Wire.read() << 8 | Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ = Wire.read() << 8 | Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
}
