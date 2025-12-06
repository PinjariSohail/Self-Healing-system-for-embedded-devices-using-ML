// === Full Updated Arduino Sketch with ML Fault Detection ===
// LCD_SDA 21 
// LCD_SCL 22

// Include libraries
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <EEPROM.h>

// Include generated ML model header
#include "fault_detection_model.h"

// Pins and hardware definitions
#define MQ2_PIN    34
#define IR_PIN     27
#define BLUE_LED   5
#define RED_LED    33
#define ORANGE_LED 25
#define GREEN_LED  26
#define BUZZER_PIN 14
#define BUTTON_PIN 12

#define TFT_CS   -1
#define TFT_DC    2
#define TFT_RST   4
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_BLK  15

#define MOTOR_IN1 13
#define MOTOR_IN2 32

LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// EEPROM size for log storage
#define EEPROM_SIZE 256

// Enums for fault and recovery types
enum FaultType : uint8_t {
  F_NONE = 0,
  F_GAS = 1,
  F_MOTION = 2,
  F_MOTOR = 3,
  F_DISPLAY = 4,
  FAULT_TEST = 5
};

enum RecoveryType : uint8_t {
  R_NONE = 0,
  R_RESET_SENSOR = 1,
  R_REINIT_DISPLAY = 2,
  R_RESTART_MOTOR = 3,
  R_SIMULATED = 4
};

enum RecoveryStatus : uint8_t {
  RS_UNKNOWN = 0,
  RS_SUCCESS = 1,
  RS_FAILURE = 2
};

// Fault log struct
struct FaultLog {
  uint8_t faultType;
  uint8_t recoveryType;
  uint32_t faultTime;
  uint32_t recoveryTime;
  uint8_t recoveryStatus;
};
FaultLog lastLog;

// Globals for button press and fault throttle
unsigned long pressStart = 0;
bool pressed = false;

uint8_t lastAutoLoggedFault = F_NONE;
unsigned long lastAutoLogTime = 0;
const unsigned long AUTO_LOG_COOLDOWN = 10000UL; // 10 seconds cooldown

// Simulated fault structure
struct SimFault {
  uint8_t type = F_NONE;
  unsigned long endTime = 0;
} simFault;

const int MAX_RECOVERY_ATTEMPTS = 3;
const unsigned long RECOVERY_RETRY_DELAY = 800;

unsigned long seedEntropy() {
  unsigned long x = micros();
  // use an ADC that is free (ADC2 channels used by WiFi can conflict; but analogRead(35) is ADC1 and usually safe)
  x ^= analogRead(35);
  x ^= ((unsigned long)millis() << 8);
  return x;
}

// Instantiate ML model
Eloquent::ML::Port::DecisionTree faultDetector;

// Utility functions for EEPROM log storage
void saveLogToEEPROM(const FaultLog &log) {
  EEPROM.put(0, log);
  EEPROM.commit();
}

void loadLogFromEEPROM(FaultLog &log) {
  EEPROM.get(0, log);
  if (log.recoveryStatus > RS_FAILURE) log.recoveryStatus = RS_UNKNOWN;
  if (log.faultType > FAULT_TEST) {
    log.faultType = F_NONE;
    log.recoveryType = R_NONE;
    log.faultTime = 0;
    log.recoveryTime = 0;
    log.recoveryStatus = RS_UNKNOWN;
  }
}

// Display helpers
void displaySensorsOnTFT(int mq2val, int irval) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_BLUE);
  tft.setCursor(10, 30);
  tft.println("MQ-2 value:");
  tft.setCursor(10, 65);
  tft.setTextColor(ST77XX_GREEN);
  tft.println(mq2val);
  
  tft.setTextColor(ST77XX_BLUE);
  tft.setCursor(10, 110);
  tft.println("Motion:");
  tft.setCursor(10, 145);
  tft.setTextColor(ST77XX_GREEN);
  tft.println(irval == LOW ? "Detected" : "None");
}

void displayFaultOnTFT(const FaultLog &log) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 30);
  if (log.faultType == F_NONE) {
    tft.println("NO FAULTS");
    tft.setCursor(10, 60);
    tft.println("DETECTED");
    return;
  }
  tft.print("Fault: ");
  switch (log.faultType) {
    case F_GAS: tft.println("Gas"); break;
    case F_MOTION: tft.println("Motion"); break;
    case F_MOTOR: tft.println("Motor"); break;
    case F_DISPLAY: tft.println("Display"); break;
    case FAULT_TEST: tft.println("Test"); break;
    default: tft.println("Unknown"); break;
  }
  tft.setCursor(10, 90);
  tft.print("Fault@s: ");
  tft.println(log.faultTime / 1000);
  tft.setCursor(10, 120);
  tft.print("Recov@s: ");
  if (log.recoveryTime == 0) tft.println("N/A");
  else tft.println(log.recoveryTime / 1000);
  tft.setCursor(10, 150);
  tft.print("Action:");
  switch (log.recoveryType) {
    case R_RESET_SENSOR: tft.println("Sensor check"); break;
    case R_REINIT_DISPLAY: tft.println("Reinit OLED"); break;
    case R_RESTART_MOTOR: tft.println("Motor check"); break;
    case R_SIMULATED: tft.println("Simulated"); break;
    default: tft.println("None"); break;
  }
  tft.setCursor(10, 180);
  tft.print("Result: ");
  switch (log.recoveryStatus) {
    case RS_UNKNOWN: tft.println("Unknown"); break;
    case RS_SUCCESS: tft.println("Success"); break;
    case RS_FAILURE: tft.println("Failed"); break;
    default: tft.println("N/A"); break;
  }
}

void showRecoveringUI(uint8_t fault) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.setTextColor(ST77XX_YELLOW);
  tft.println("FAULT DETECTED!");
  tft.setCursor(10, 90);
  tft.println("Recovering...");
}

// Recovery verification helpers
bool isGasSafe() {
  int v = analogRead(MQ2_PIN);
  return (v <= 1000); // threshold tuned in your code
}
bool isMotionNormal() {
  int v = digitalRead(IR_PIN);
  return (v == HIGH);
}
bool isMotorOk() {
  // You can add motor current sensor or tachometer to check; for now assume success after restart
  return true;
}
bool isDisplayOk() {
  // Try a small test draw and assume OK
  tft.fillRect(0, 0, 10, 10, ST77XX_WHITE);
  delay(40);
  return true;
}
// Recovery attempts
bool attemptRecoverGas(FaultLog &log) {
  for (int attempt = 1; attempt <= MAX_RECOVERY_ATTEMPTS; ++attempt) {
    digitalWrite(BLUE_LED, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BLUE_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    
    delay(RECOVERY_RETRY_DELAY);
    if (isGasSafe()) {
      log.recoveryType = R_RESET_SENSOR;
      log.recoveryTime = millis();
      log.recoveryStatus = RS_SUCCESS;
      return true;
    }
  }
  log.recoveryType = R_RESET_SENSOR;
  log.recoveryTime = millis();
  log.recoveryStatus = RS_FAILURE;
  return false;
}

bool attemptRecoverMotion(FaultLog &log) {
  for (int attempt = 1; attempt <= MAX_RECOVERY_ATTEMPTS; ++attempt) {
    digitalWrite(ORANGE_LED, HIGH);
    delay(150);
    digitalWrite(ORANGE_LED, LOW);
    delay(RECOVERY_RETRY_DELAY);
    
    if (isMotionNormal()) {
      log.recoveryType = R_RESET_SENSOR;
      log.recoveryTime = millis();
      log.recoveryStatus = RS_SUCCESS;
      return true;
    }
  }
  log.recoveryType = R_RESET_SENSOR;
  log.recoveryTime = millis();
  log.recoveryStatus = RS_FAILURE;
  return false;
}

bool attemptRecoverMotor(FaultLog &log) {
  for (int attempt = 1; attempt <= MAX_RECOVERY_ATTEMPTS; ++attempt) {
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    delay(2000);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, HIGH);
    delay(2000);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    delay(1000);
    
    if (isMotorOk()) {
      log.recoveryType = R_RESTART_MOTOR;
      log.recoveryTime = millis();
      log.recoveryStatus = RS_SUCCESS;
      return true;
    }
  }
  log.recoveryType = R_RESTART_MOTOR;
  log.recoveryTime = millis();
  log.recoveryStatus = RS_FAILURE;
  return false;
}

bool attemptRecoverDisplay(FaultLog &log) {
  for (int attempt = 1; attempt <= MAX_RECOVERY_ATTEMPTS; ++attempt) {
    tft.init(240, 240, SPI_MODE3);
    tft.setRotation(2);
    delay(200);
    if (isDisplayOk()) {
      log.recoveryType = R_REINIT_DISPLAY;
      log.recoveryTime = millis();
      log.recoveryStatus = RS_SUCCESS;
      return true;
    }
    delay(RECOVERY_RETRY_DELAY);
  }
  log.recoveryType = R_REINIT_DISPLAY;
  log.recoveryTime = millis();
  log.recoveryStatus = RS_FAILURE;
  return false;
}

bool attemptRecoverTest(FaultLog &log) {
  for (int attempt = 1; attempt <= MAX_RECOVERY_ATTEMPTS; ++attempt) {
    for (int i = 0; i < 2; ++i) {
      digitalWrite(RED_LED, HIGH);
      digitalWrite(ORANGE_LED, HIGH);
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(BLUE_LED, HIGH);
      delay(120);
      digitalWrite(RED_LED, LOW);
      digitalWrite(ORANGE_LED, LOW);
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(BLUE_LED, LOW);
      delay(120);
    }
    delay(RECOVERY_RETRY_DELAY);
    log.recoveryType = R_SIMULATED;
    log.recoveryTime = millis();
    log.recoveryStatus = RS_SUCCESS;
    return true;
  }
  log.recoveryType = R_SIMULATED;
  log.recoveryTime = millis();
  log.recoveryStatus = RS_FAILURE;
  return false;
}

// Recovery and logging orchestrator
void performRecoveryWithLog(FaultLog &log) {
  showRecoveringUI(log.faultType);
  delay(200);
  
  bool ok = false;
  switch (log.faultType) {
    case F_GAS: ok = attemptRecoverGas(log); break;
    case F_MOTION: ok = attemptRecoverMotion(log); break;
    case F_MOTOR: ok = attemptRecoverMotor(log); break;
    case F_DISPLAY: ok = attemptRecoverDisplay(log); break;
    case FAULT_TEST: ok = attemptRecoverTest(log); break;
    default: 
      log.recoveryType = R_NONE; 
      log.recoveryStatus = RS_UNKNOWN; 
      log.recoveryTime = millis(); 
      ok =false;
      break;
  }
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 60);
  if (ok) { 
    tft.setTextColor(ST77XX_GREEN); 
    tft.println("RECOVERY SUCCESS"); 
  } else { 
    tft.setTextColor(ST77XX_RED); 
    tft.println("RECOVERY FAILED"); 
  }
  tft.setCursor(10, 100);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Action:");
  switch (log.recoveryType) {
    case R_RESET_SENSOR: tft.println("Sensor check"); break;
    case R_REINIT_DISPLAY: tft.println("Reinit TFT"); break;
    case R_RESTART_MOTOR: tft.println("Motor check"); break;
    case R_SIMULATED: tft.println("Simulated"); break;
    default: tft.println("None"); break;
  }
  tft.setCursor(10, 130);
  tft.print("Time: ");
  tft.println(log.recoveryTime / 1000);
  delay(1200);
  
  lastLog = log;
  saveLogToEEPROM(lastLog);
}

// Fault injection (same as before)
void injectRandomFaultWithLog() {
  uint8_t ft = random(1, 6); // 1..5
  Serial.print("Injecting random fault type ");
  Serial.println(ft);

  // simulated fault active for visibility
  simFault.type = ft;
  simFault.endTime = millis() + 6000; // simulation lasts 6 seconds

  // Create a log, set fault time (recovery will update)
  FaultLog log;
  log.faultType = ft;
  log.faultTime = millis();
  log.recoveryType = R_NONE;
  log.recoveryTime = 0;
  log.recoveryStatus = RS_UNKNOWN;

  performRecoveryWithLog(log);
}

// CSV Logging (optional if you want to continue logging data over serial)
void printCSVHeader() {
  Serial.println("fault_type,recovery_type,recovery_status,mq2_value,motion_state");
}

void logSensorDataCSV(int mq2, int ir) {
  Serial.print("0,0,0,");
  Serial.print(mq2);
  Serial.print(",");
  Serial.println(ir == LOW ? "0" : "1");
}

void logFaultRecoveryCSV(const FaultLog &log) {
  Serial.print(log.faultType);
  Serial.print(",");
  Serial.print(log.recoveryType);
  Serial.print(",");
  Serial.print(log.recoveryStatus);
  Serial.print(",");
  Serial.print(analogRead(MQ2_PIN));
  Serial.print(",");
  Serial.println(digitalRead(IR_PIN) == LOW ? "1" : "0");
}

// Setup function
void setup() {
  Serial.begin(115200);
  printCSVHeader();

  // Init pins
  pinMode(BLUE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(ORANGE_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  pinMode(IR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  
  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
  tft.init(240, 240, SPI_MODE3);
  tft.setRotation(2);
  
  lcd.init();
  lcd.backlight();

  EEPROM.begin(EEPROM_SIZE);
  loadLogFromEEPROM(lastLog);

  randomSeed(seedEntropy());
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setCursor(20, 110);
  tft.setTextColor(ST77XX_RED);
  tft.println("System Ready");
  delay(800);
}

// Main loop
void loop() {
  int mq2value = analogRead(MQ2_PIN);
  int irvalue = digitalRead(IR_PIN);

  // Simulated fault override if needed (keep if you want manual fault injection)
  if (simFault.type != F_NONE && millis() < simFault.endTime) {
    switch (simFault.type) {
      case F_GAS: mq2value = 4095; break; // extreme value
      case F_MOTION: irvalue = LOW; break; // force detected
      case F_MOTOR: /* motor not directly displayed; we can show on LCD */ break;
      case F_DISPLAY: /* cannot directly simulate except show message */ break;
      default: break;
    }
  } else {
    // reset simFault when expired
    if (simFault.type != F_NONE && millis() >= simFault.endTime) {
      simFault.type = F_NONE;
    }
  }

  // Prepare features for ML inference
  float features[] = { (float)mq2value, (float)(irvalue == LOW ? 1 : 0) };
  
  // Predict fault type via ML model (Decision Tree)
  int predictedFault = faultDetector.predict(features);

  // Trigger recovery if fault detected and cooldown expired
  if (predictedFault != F_NONE && (lastAutoLoggedFault != predictedFault || millis() - lastAutoLogTime > AUTO_LOG_COOLDOWN)) {
    FaultLog log;
    log.faultType = (uint8_t)predictedFault;
    log.faultTime = millis();
    log.recoveryType = R_NONE;
    log.recoveryTime = 0;
    log.recoveryStatus = RS_UNKNOWN;
    performRecoveryWithLog(log);
    lastAutoLoggedFault = predictedFault;
    lastAutoLogTime = millis();
  }

  // Update LEDs, buzzer, motor based on sensor values (you can customize this if you want)
  if (mq2value > 850) {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
  } else {
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
  }
  if (irvalue == LOW) digitalWrite(ORANGE_LED, HIGH); 
  else digitalWrite(ORANGE_LED, LOW);
  if (mq2value <= 850 && irvalue == HIGH) digitalWrite(GREEN_LED, HIGH); 
  else digitalWrite(GREEN_LED, LOW);

  // Update LCD status
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System status:");
  if (simFault.type != F_NONE) {
    lcd.setCursor(0, 1); lcd.print("SYSTEM ERROR");
  } else if (mq2value > 850) {
    lcd.setCursor(0, 1); lcd.print("SMOKE DETECTED");
  } else {
    lcd.setCursor(0, 1); lcd.print("SYSTEM ACTIVE");
  }

  // Handle button press (show log or inject fault)
  int btnState = digitalRead(BUTTON_PIN);
  if (btnState == LOW && !pressed) {
    pressed = true;
    pressStart = millis();
  }
  if (btnState == HIGH && pressed) {
    unsigned long duration = millis() - pressStart;
    pressed = false;
    if (duration >= 1000 && duration < 3000) {
      loadLogFromEEPROM(lastLog);
      displayFaultOnTFT(lastLog);
      delay(3000);
    } else if (duration >= 3000) {
      // Optional: implement fault injection if desired
       injectRandomFaultWithLog();
    }
  }

  // Update TFT display sensor data if no button action
  if (!pressed) {
    if (simFault.type == F_DISPLAY) {
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(10, 100);
      tft.setTextColor(ST77XX_RED);
      tft.println("TFT ERROR");
    } else {
      displaySensorsOnTFT(mq2value, irvalue);
    }
  }

  // Optional CSV sensor data logging via serial
  logSensorDataCSV(mq2value, irvalue);

  delay(300); // Adjust delay as needed
}
