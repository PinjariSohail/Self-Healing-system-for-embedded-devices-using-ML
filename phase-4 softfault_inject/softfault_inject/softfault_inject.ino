// LCD_SDA 21 
// LCD_SCL 22

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <EEPROM.h>

#define MQ2_PIN    34
#define IR_PIN     27

#define BLUE_LED   5
#define GREEN_LED  33
#define ORANGE_LED 25
#define RED_LED    26


#define BUZZER_PIN 14
#define BUTTON_PIN 12

// TFT OLED (ST7789, SPI)
#define TFT_CS   -1
#define TFT_DC    2
#define TFT_RST   4
#define TFT_MOSI 23 // SDA/MOSI
#define TFT_SCLK 18 // SCL/SCLK
#define TFT_BLK  15

#define MOTOR_IN1 13
#define MOTOR_IN2 32

LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

#define EEPROM_SIZE 128

enum FaultType : uint8_t {
  F_NONE = 0,
  F_GAS  = 1,
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


struct FaultLog {
  uint8_t faultType;
  uint8_t recoveryType;
  uint32_t faultTime;      // millis() when fault detected
  uint32_t recoveryTime;   // millis() when recovered
};

FaultLog lastLog;

unsigned long pressStart = 0;
bool pressed = false;

uint8_t lastAutoLoggedFault = F_NONE;
unsigned long lastAutoLogTime = 0;
const unsigned long AUTO_LOG_COOLDOWN = 10UL * 1000UL; // 10 seconds

struct SimFault {
  uint8_t type = F_NONE;
  unsigned long endTime = 0;
} simFault;

// Random seed helper
unsigned long seedEntropy() {
  // Combine analog noise and micros()
  unsigned long x = micros();
  x ^= analogRead(35); // a floating pin will give noisy values
  x ^= ((unsigned long)millis() << 8);
  return x;
}

void saveLogToEEPROM(const FaultLog &log) {
  EEPROM.put(0, log);
  EEPROM.commit();
}

void loadLogFromEEPROM(FaultLog &log) {
  EEPROM.get(0, log);
}

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
  tft.println(log.faultTime/1000);
  tft.setCursor(10, 120);
  tft.print("Recov@s: ");
  if (log.recoveryTime == 0) tft.println("N/A");
  else tft.println(log.recoveryTime/1000);
  tft.setCursor(10, 150);
  tft.print("Action:");
  switch (log.recoveryType) {
    case R_RESET_SENSOR: tft.println("Sensor check"); break;
    case R_REINIT_DISPLAY: tft.println("Reinit OLED"); break;
    case R_RESTART_MOTOR: tft.println("Motor check"); break;
    case R_SIMULATED: tft.println("Simulated"); break;
    default: tft.println("None"); break;
  }
}

// Show "Recovering..." then "Recovered" messages
void showRecoveringUI(uint8_t fault) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.setTextColor(ST77XX_YELLOW);
  tft.println("FAULT DETECTED!");
  tft.setCursor(10, 90);
  tft.println("Recovering...");
}

// ---------------- Recovery routines ----------------
void recoverGasFault(FaultLog &log) {
  for (int i = 0; i < 3; ++i) {
    digitalWrite(BLUE_LED, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BLUE_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
  // Simulate sensor reset (no hardware reset available)
  // Mark recovery type and time
  log.recoveryType = R_RESET_SENSOR;
  log.recoveryTime = millis();
}

void recoverMotionFault(FaultLog &log) {
  // Try debounce / "re-init" by waiting and toggling orange LED
  for (int i = 0; i < 2; ++i) {
    digitalWrite(BLUE_LED, HIGH);
    delay(200);
    digitalWrite(BLUE_LED, LOW);
    delay(200);
  }
  log.recoveryType = R_RESET_SENSOR;
  log.recoveryTime = millis();
}

void recoverMotorFault(FaultLog &log) {
  // Try simple motor restart: forward -> reverse -> stop
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  delay(1000);
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, HIGH);
  delay(1000);
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  log.recoveryType = R_RESTART_MOTOR;
  log.recoveryTime = millis();
}

void recoverDisplayFault(FaultLog &log) {
  // Try re-initializing the TFT display
  tft.init(240, 240, SPI_MODE3);
  tft.setRotation(0);
  log.recoveryType = R_REINIT_DISPLAY;
  log.recoveryTime = millis();
}

void recoverTestFault(FaultLog &log) {
  // Simulate a multi-step recovery
  // small wobble: blink all LEDs
  for (int i = 0; i < 3; ++i) {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(ORANGE_LED, HIGH);
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(BLUE_LED, HIGH);
    delay(150);
    digitalWrite(RED_LED, LOW);
    digitalWrite(ORANGE_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BLUE_LED, LOW);
    delay(150);
  }
  log.recoveryType = R_SIMULATED;
  log.recoveryTime = millis();
}

// Performs recovery depending on fault type
void performRecovery(FaultLog &log) {
  showRecoveringUI(log.faultType);
  delay(400); // short pause so TFT updates

  switch (log.faultType) {
    case F_GAS:
      recoverGasFault(log);
      break;
    case F_MOTION:
      recoverMotionFault(log);
      break;
    case F_MOTOR:
      recoverMotorFault(log);
      break;
    case F_DISPLAY:
      recoverDisplayFault(log);
      break;
    case FAULT_TEST:
      recoverTestFault(log);
      break;
    default:
      log.recoveryType = R_NONE;
      log.recoveryTime = millis();
      break;
  }

  // Show recovered message
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 80);
  tft.setTextColor(ST77XX_GREEN);
  tft.println("Recovered");
  tft.setCursor(10, 110);
  tft.print("Action:");
  switch (log.recoveryType) {
    case R_RESET_SENSOR: tft.println("Sensor check"); break;
    case R_REINIT_DISPLAY: tft.println("Reinit TFT"); break;
    case R_RESTART_MOTOR: tft.println("Motor Check"); break;
    case R_SIMULATED: tft.println("Simulated"); break;
    default: tft.println("None"); break;
  }
  delay(1500);
}

// ---------------- Fault injection ----------------
void injectRandomFault() {
  uint8_t ft = random(1, 6); // 1..5
  Serial.print("Injecting random fault type ");
  Serial.println(ft);

  // set simulated fault that will affect displayed sensor values for a short time
  simFault.type = ft;
  simFault.endTime = millis() + 6000; // simulation lasts 6 seconds

  // Create a log, set fault time
  FaultLog log;
  log.faultType = ft;
  log.faultTime = millis();
  log.recoveryType = R_NONE;
  log.recoveryTime = 0;

  // perform recovery and save
  performRecovery(log);
  lastLog = log;
  saveLogToEEPROM(lastLog);
}

// ---------------- Setup & Loop ----------------
void setup() {
  Serial.begin(115200);

  // init pins
  pinMode(BLUE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(ORANGE_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);

  // init displays
  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
  tft.init(240, 240, SPI_MODE3);
  tft.setRotation(180);

  lcd.init();
  lcd.backlight();

  // init EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadLogFromEEPROM(lastLog);

  // seed RNG
  randomSeed(seedEntropy());

  // initial TFT message
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setCursor(20, 110);
  tft.setTextColor(ST77XX_RED);
  tft.println("System Ready");
  delay(800);
}

void loop() {
  // Read sensors
  int mq2value = analogRead(MQ2_PIN);
  int irvalue = digitalRead(IR_PIN);

  // Override with simulated fault if active
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

  // Basic automatic detection (rule-based safety)
  // Gas threshold (emergency) -> immediate action & log (but avoid spamming)
  
    if (mq2value > 2000 && (lastAutoLoggedFault != F_GAS || millis() - lastAutoLogTime > AUTO_LOG_COOLDOWN)) {
    FaultLog log = {F_GAS, R_NONE, millis(), 0};
    performRecovery(log);
    lastLog = log;
    saveLogToEEPROM(lastLog);
    lastAutoLoggedFault = F_GAS;
    lastAutoLogTime = millis();
  }

  // Motion fault detection (stuck/noise) - small example: treat a detected motion as a logged event
  if (irvalue == LOW && (lastAutoLoggedFault != F_MOTION || millis() - lastAutoLogTime > AUTO_LOG_COOLDOWN)) {
    FaultLog log = {F_MOTION, R_NONE, millis(), 0};
    performRecovery(log);
    lastLog = log;
    saveLogToEEPROM(lastLog);
    lastAutoLoggedFault = F_MOTION;
    lastAutoLogTime = millis();
  }

  // LED + buzzer status
  if (mq2value > 1000) {
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
  if (irvalue == LOW) digitalWrite(ORANGE_LED, HIGH); else digitalWrite(ORANGE_LED, LOW);
  if (mq2value <= 1000 && irvalue == HIGH) digitalWrite(GREEN_LED, HIGH); else digitalWrite(GREEN_LED, LOW);

  // Update LCD status
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System status:");

  bool faultActive = (simFault.type != F_NONE); // simulated fault active
  if (faultActive) {
    lcd.setCursor(0, 1);
    lcd.print("SYSTEM ERROR");
  }
  else if (mq2value > 1000) {
    lcd.setCursor(0, 1);
    lcd.print("SMOKE DETECTED");
  }
  else {
    lcd.setCursor(0, 1);
    lcd.print("SYSTEM ACTIVE");
  }


  // Button handling (double press + long press)
  int btnState = digitalRead(BUTTON_PIN);
  if (btnState == LOW && !pressed) {
    pressed = true;
    pressStart = millis();
  }
  if (btnState == HIGH && pressed){
    unsigned long duration = millis() - pressStart;
    pressed = false;
    if(duration >= 1000 && duration <3000){
      loadLogFromEEPROM(lastLog);
      displayFaultOnTFT(lastLog);
      delay(3000);
    }
    else if(duration >= 3000){
      injectRandomFault();
    }
  }

  // Normal TFT display when not in button/long-press action
  // We show sensors unless simFault.type == F_DISPLAY which simulates display fault (so we might show alternate message)
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

  // small loop delay
  delay(300);
}
