// LCD_SDA 21 
// LCD_SCL 22

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <EEPROM.h>
#include "fault_detection_model.h"

#define mq2 34
#define ir 27
#define blue 5
#define red 33
#define orange 25
#define green 26

#define buzzer 14
#define button 12

#define TFT_CS   -1
#define TFT_DC    2
#define TFT_RST   4
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_BLK  15

#define motor_in1 13
#define motor_in2 32

LiquidCrystal_I2C lcd(0x27, 6, 2);
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

static unsigned long irstart = 0;


#define EEPROM_SIZE 256

enum faulttype :uint8_t {
  f_none = 0,
  f_mq2 = 1,
  f_motion = 2,
  f_display =3,
  f_motor = 4,
  f_test = 5,
};

enum recoverytype : uint8_t {
  r_none = 0,
  r_restart_sensor = 1,
  r_reinit_display = 2,
  r_restart_motor = 3,
  r_simulate = 4,
};

enum recoverystatus : uint8_t {
  rs_unknown = 0,
  rs_success = 1,
  rs_failure = 2
};

struct faultlog {
  uint8_t faulttype;
  uint8_t recoverytype;
  uint8_t faulttime;
  uint8_t recoverytime;
  uint8_t recoverystatus;
};
faultlog lastlog;

unsigned long pressstart = 0;
bool pressed = false;

uint8_t lastautologgedfault = f_none;
unsigned long lastautologtime = 0;
const unsigned long auto_log_cooldown = 10000UL;
s
struct simfault {
  uint8_t type = f_none;
  unsigned long endtime = 0;
} simfault;

const int max_recovery_attempts = 3;
const unsigned long recovery_retry_delay = 800;

unsigned long seedentropy() {
  unsigned long x = micros();
  x ^= analogRead(35);
  x ^= ((unsigned long)millis() <<8);
  return x;
}

Eloquent::ML::Port::DecisionTree faultDetector;

void savelog(const faultlog &log) {
  EEPROM.put(0,log);
  EEPROM.commit();
}

void loadlog(faultlog &log) {
  EEPROM.get(0, log);
  if (log.recoverystatus >rs_failure) log.recoverystatus = rs_unknown;
  if (log.faulttype > f_test) {
    log.faulttype = f_none;
    log.recoverytype = r_none;
    log.faulttime = 0;
    log.recoverytime = 0;
    log.recoverystatus = rs_unknown;
  }
}

void displaysensors(int mq2value, int irvalue) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_BLUE);
  tft.setCursor(10, 30);
  tft.println("MQ-2 values: ");
  tft.setCursor(10, 65);
  tft.setTextColor(ST77XX_GREEN);
  tft.println(mq2value);

   tft.setTextColor(ST77XX_BLUE);
  tft.setCursor(10,110);
  tft.println("Motion: ");
  tft.setCursor(10, 145);
  tft.setTextColor(ST77XX_GREEN);
  tft.println(irvalue == LOW ? "DETECTED" : "NONE");
}

void displayfault(const faultlog &log) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10,30);
  if (log.faulttype == f_none) {
    tft.println("NO FAULT");
    tft.setCursor(10,60);
    tft.println("DETECTED");
    return;
  }
  tft.print("Fault : ");
  switch (log.faulttype) {
    case f_mq2: 
      tft.println("MQ-2"); 
      break;
      
    case f_motion:
      tft.println("Motion");
      break;

    case f_display:
      tft.println("Display");
      break;

    case f_motor:
      tft.println("Motor");
      break;

    case f_test:
      tft.println("Test");
      break;

    default:
      tft.println("Unknown");
      break;
  }
  tft.setCursor(10, 90);
  tft.print("Fault@s: ");
  tft.println(log.faulttime / 1000);
  tft.setCursor(10, 120);
  tft.print("Recover@s: ");
  if (log.recoverytime == 0) tft.println("N/A");
  else tft.println(log.recoverytime / 1000);
  tft.setCursor(10, 150);
  tft.print("Action:");
  switch (log.recoverytype) {
    case r_restart_sensor:
      tft.println("Sensor Check");
      break;

    case r_reinit_display:
      tft.println("Reint OLED");
      break;

    case r_restart_motor:
      tft.println("Motor Check");
      break;

    case r_simulate: 
      tft.println("Simulated");
      break;

    default:
      tft.println("None");
      break;
  }
  tft.setCursor(10, 180);
  tft.print("Result: ");
  switch (log.recoverystatus) {
    case rs_unknown: 
      tft.println("Unknown");
      break;

    case rs_success: 
      tft.println("Success");
      break;

    case rs_failure:
      tft.println("Failed");
      break;

    default:
      tft.println("N/A");
      break;
  }
}

void showrecovering(uint8_t fault) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.setTextColor(ST77XX_YELLOW);
  tft.println("FAULT DETECTED");
  tft.setCursor(10, 90);
  tft.println("recovering...");
}

bool ismq2safe(){
  int v = analogRead(mq2);
  return (v <= 2000);
}

bool ismotionnormal() {
  int v = digitalRead(ir);
  return (v == HIGH);
}

bool ismotorok() {
}

bool isdisplayok() {
  tft.fillRect(0, 0,10, 10, ST77XX_WHITE);
  delay(50);
  return true;
}

bool attemptrecoverymq2(faultlog &log) {
  for (int attempt = 1; attempt <= max_recovery_attempts; ++attempt) {
    digitalWrite(blue, HIGH);
    digitalWrite(buzzer, HIGH);
    delay(150);
    digitalWrite(blue, LOW);
    digitalWrite(buzzer, LOW);

    delay(recovery_retry_delay);
    if (ismq2safe()) {
      log.recoverytype = r_restart_sensor;
      log.recoverytime = millis();
      log.recoverystatus = rs_success;
      return true;
    }
  }
  log.recoverytype = r_restart_sensor;
  log.recoverytime = millis();
  log.recoverystatus = rs_failure;
  return false;
}

bool attemptrecoverymotion(faultlog &log) {
  for (int attempt = 1; attempt <= max_recovery_attempts; ++attempt) {
    digitalWrite(orange, HIGH);
    delay(150);
    digitalWrite(orange, LOW);
    delay(recovery_retry_delay);

    if (ismotionnormal()) {
      log.recoverytype = r_restart_sensor;
      log.recoverytime = millis();
      log.recoverystatus = rs_success;
      return true; 
    }
  }
  log.recoverytype = r_restart_sensor;
  log.recoverytime = millis();
  log.recoverystatus = rs_failure;
  return false;
}

bool attemptrecoverydisplay(faultlog &log) {
  for (int attempt = 1; attempt <= max_recovery_attempts; ++attempt) {
    tft.init(240, 240, SPI_MODE3);
    tft.setRotation(2);
    delay(200);
    if (isdisplayok()) {
      log.recoverytype = r_reinit_display;
      log.recoverystatus = rs_success;
      return true;
    }
    delay(recovery_retry_delay);
  }
  log.recoverytype = r_reinit_display;
  log.recoverytime = millis();
  log.recoverystatus = rs_failure;
  return false;
}

bool attemptrecoverymotor(faultlog &log) {
  for (int attempt = 1; attempt <= max_recovery_attempts; ++attempt) {
    digitalWrite(motor_in1, HIGH);
    digitalWrite(motor_in2, LOW);
    delay(2000);
    digitalWrite(motor_in1, LOW);
    digitalWrite(motor_in2, HIGH);
    delay(2000);
    digitalWrite(motor_in1, LOW);
    digitalWrite(motor_in2, LOW);
    delay(1000);

    if (ismotorok()) {
      log.recoverytype = r_restart_motor;
      log.recoverytime = millis();
      log.recoverystatus = rs_success;
      return true;
    }
  }
  log.recoverytype = r_restart_motor;
  log.recoverytime = millis();
  log.recoverystatus = rs_failure;
  return false;
}

bool attemptrecoverytest(faultlog &log) {
  for (int attempt = 1; attempt <= max_recovery_attempts; ++attempt) {
    for (int i = 0; i < 2; ++i) {
      digitalWrite(red, HIGH);
      digitalWrite(orange, HIGH);
      digitalWrite(green, HIGH);
      digitalWrite(blue, HIGH);
      delay(120);
      digitalWrite(red, LOW);
      digitalWrite(orange, LOW);
      digitalWrite(green, LOW);
      digitalWrite(blue, LOW);
      delay(120);
    }
    delay(recovery_retry_delay);
    log.recoverytype = r_simulate;
    log.recoverytime = millis();
    log.recoverystatus = rs_success;
    return true;
  }
  log.recoverytype = r_simulate;
  log.recoverytime = millis();
  log.recoverystatus = rs_failure;
  return false;
}

bool isirlowfor20s() {
    if (ir == LOW) {
        if (irstart == 0) irstart = millis();
        return (millis() - irstart >= 20000);
    } else {
        irstart = 0;
        return false;
    }
}

void irwireerror(){
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 70);
  tft.setTextColor(ST77XX_RED);
  tft.println("Check IR Wire");
  tft.setCursor(10, 120);
  tft.println("Connections...");
  delay(2000);
}

void mq2wireerror(){
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 70);
    tft.setTextColor(ST77XX_RED);
    tft.println("Check MQ-2 WIRE");
    tft.setCursor(10, 120);
    tft.println("CONNECTIONS...");
    delay(2000);

    digitalWrite(buzzer, HIGH);
}

void performrecovery(faultlog &log) {
  showrecovering(log.faulttype);
  delay(200);

  bool ok = false;
  switch (log.faulttype) {
    case f_mq2:
      ok = attemptrecoverymq2(log);
      break;

    case f_motion:
      ok = attemptrecoverymotion(log);
      break;

    case f_display:
      ok = attemptrecoverydisplay(log);
      break;

    case f_motor:
      ok = attemptrecoverymotor(log);
      break;

    case f_test:
      ok = attemptrecoverytest(log);
      break;

    default:
      log.recoverytype = r_none;
      log.recoverystatus = rs_unknown;
      log.recoverytime = millis();
      ok = false;
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
  switch (log.recoverytype) {
    
    case r_restart_sensor:
      tft.println("Sensor Check");
      break;

    case r_reinit_display:
      tft.println("Reinit TFT");
      break;

    case r_restart_motor:
      tft.println("Motor Check");
      break;

    case r_simulate:
      tft.println("Simulated");
      break;

    default :
      tft.println("NONE");
      break;
  }
  tft.setCursor(10, 130);
  tft.print("Time: ");
  tft.println(log.recoverytime / 1000);
  delay(1200);

  lastlog = log;
  savelog(lastlog);
}

void injectfault() {
  uint8_t ft = random(1, 6);
  //Serial.print("Injected random faault type: ");

  simfault.type = ft;
  simfault.endtime = millis() + 6000;

  faultlog log;
  log.faulttype = ft;
  log.faulttime = millis();
  log.recoverytype = r_none;
  log.recoverytime = millis();
  log.recoverystatus = rs_unknown;

  performrecovery(log);
}

void setup() {
  Serial.begin(115200);

  pinMode(blue, OUTPUT);
  pinMode(red, OUTPUT);
  pinMode(orange, OUTPUT);
  pinMode(green, OUTPUT);

  pinMode(buzzer, OUTPUT);

  pinMode(motor_in1, OUTPUT);
  pinMode(motor_in2, OUTPUT);

  pinMode(ir, INPUT);
  pinMode(button, INPUT_PULLUP);

  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1); 
  tft.init(240, 240, SPI_MODE3);
  tft.setRotation(2);

  lcd.init();
  lcd.backlight();

  EEPROM.begin(EEPROM_SIZE);
  loadlog(lastlog);

  randomSeed(seedentropy());

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setCursor(20, 110);
  tft.setTextColor(ST77XX_RED);
  tft.println("SYSTEM READY");
  delay(800);
}

void loop() {
  int mq2value = analogRead(mq2);
  int irvalue = digitalRead(ir);

  if (simfault.type != f_none && millis() < simfault.endtime) {
    switch (simfault.type) {
      case f_mq2:
        mq2value >= 3500;
        break;

      case f_motion:
        irvalue = LOW;
        break;

      case f_display:
        break;

      case f_motor:
        break;
    }
  } else{
     if (simfault.type != f_none && millis() >=simfault.endtime){
      simfault.type = f_none;
    }
  }

  float features[] = { (float)mq2value, (float)(irvalue == LOW ? 1 : 0) };

  int predictedfault = faultDetector.predict(features);
 
  if (predictedfault != f_none && (lastautologgedfault != predictedfault || millis() - lastautologtime >auto_log_cooldown)) {
    faultlog log;
    log.faulttype = (uint8_t)predictedfault;
    log.faulttime = millis();
    log.recoverytype = r_none;
    log.recoverytime = 0;
    log.recoverystatus = rs_unknown;
    performrecovery(log);
    lastautologgedfault = predictedfault;
    lastautologtime = millis();
  }

  if (mq2value > 850) {
    digitalWrite(red, HIGH);
    digitalWrite(buzzer, HIGH);
    digitalWrite(motor_in1, HIGH);
    digitalWrite(motor_in2, LOW);
  } else {
    digitalWrite(red, LOW);
    digitalWrite(buzzer, LOW);
    digitalWrite(motor_in1, LOW);
    digitalWrite(motor_in2, HIGH);
  }

  if (irvalue == LOW) {
    digitalWrite(orange, HIGH);
  } else {
    digitalWrite(orange, LOW);
  }

  if (mq2value <= 850 && irvalue == HIGH) {
    digitalWrite(green, HIGH);
  } else {
    digitalWrite(green, LOW);
  }

  if (mq2value >=0 && mq2value <=150) {
    mq2wireerror();
  }
  if (ir == LOW && irstart <= 20000){
    irwireerror();
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Status:");
  
  if (simfault.type != f_none) {
    lcd.setCursor(0, 1);
    lcd.print("SYSTEM ERROR");
  } 
   
  else if (mq2value > 850) {
    lcd.setCursor(0, 1);
    lcd.print("SMOKE DETECTED");
  }
  else if (mq2value >=0 && mq2value <=150){
    lcd.setCursor(0, 1);
    lcd.print("CHECK CONNECTIONS");
  } 
  else if(ir == LOW && irstart <= 20000){
    lcd.setCursor(0, 1);
    lcd.print("CHECK CONNEctionS");
  }
  else {
    lcd.setCursor(0, 1);
    lcd.print("SYSTEM ACTIVE");
  }

  int btnstate = digitalRead(button);
  if (btnstate == LOW && !pressed) {
    pressed = true;
    pressstart = millis();
  }
  if (btnstate == HIGH && pressed) {
    unsigned long duration = millis() - pressstart;
    pressed = false;
    if (duration >= 1000 && duration < 3000) {
      loadlog(lastlog);
      displayfault(lastlog);
      delay(3000);
    }
    else if (duration >= 3000) {
      injectfault();
    }
   }

   if (!pressed) {
    if (simfault.type == f_display) {
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(10, 100);
      tft.setTextColor(ST77XX_RED);
      tft.println("TFT ERROR");
    }
    else {
      displaysensors(mq2value, irvalue);
    }
   }

   delay(300);
 }
