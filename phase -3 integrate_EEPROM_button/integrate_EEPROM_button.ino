// LCD_SDA 21
//LCD_SCL 22

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <EEPROM.h>

#define mq2 34
#define ir 27

#define red_led 25
#define orange_led 26
#define green_led 33

#define buzzer 14
#define button 12

#define TFT_CS -1 //not used
#define TFT_DC 2
#define TFT_RST 4
#define TFT_MOSI 23//SDA
#define TFT_SCLK 18//SCL
#define TFT_BLK 15

#define EEPROM_SIZE 64

LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

struct faultlog {
  int faultType;
  unsigned long timestamp;
};
faultlog lastfault;

unsigned long pressStart = 0;
bool pressed = false;
unsigned long lastPressTime = 0;
int pressCount = 0;

void savefault(int type){
  lastfault.faultType = type;
  lastfault.timestamp = millis();
  EEPROM.put(0, lastfault);
  EEPROM.commit();
}

void loadfault(){
  EEPROM.get(0, lastfault);
}

void displayfault(){
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10,40);

  if(lastfault.faultType == 0) {
    tft.println("NO FAULT");
    tft.setCursor(10, 70);
    tft.println("DETECTED");
  }
  else{
    tft.print("Fault: ");
    if(lastfault.faultType == 1) tft.println("GAS");
    else if(lastfault.faultType == 2) tft.println("MOTION");
    else if(lastfault.faultType == 3) tft.println("Test");

    tft.setCursor(10, 100);
    tft.print("Time: ");
    tft.println(lastfault.timestamp / 1000);
  }
}

void displaysensor(int mq2value, int irvalue) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_BLUE);
  tft.setCursor(10, 40);
  tft.println("MQ-2 values: ");
  tft.setCursor(10, 70);
  tft.setTextColor(ST77XX_GREEN);
  tft.println(mq2value);

  tft.setTextColor(ST77XX_BLUE);
  tft.setCursor(10,100);
  tft.println("Motion: ");
  tft.setCursor(10,130);
  tft.setTextColor(ST77XX_GREEN);
  if(irvalue == LOW) tft.println("DETECTED");
  else tft.println("NONE");
}

void setup(){
  Serial.begin(115200);

  pinMode(red_led, OUTPUT);
  pinMode(orange_led, OUTPUT);
  pinMode(green_led, OUTPUT);
  
  pinMode(buzzer, OUTPUT);
  
  pinMode(button, INPUT_PULLUP);

  pinMode(ir, INPUT);

  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
  tft.init(240, 240, SPI_MODE3);
  tft.setRotation(0);
  //tft.fillScreen(ST77XX_BLACK);
  //tft.setTextColor(ST77XX_WHITE);

  lcd.init();
  lcd.backlight();

  EEPROM.begin(EEPROM_SIZE);
  loadfault();
}


void loop(){
  int mq2value = analogRead(mq2);
  int irvalue = digitalRead(ir);

  if (mq2value >850) {
    digitalWrite(red_led, HIGH);
    digitalWrite(buzzer, HIGH);
    savefault(1);
  }
  else{
    digitalWrite(red_led, LOW);
    digitalWrite(buzzer, LOW);
  }

  if(irvalue == LOW){
    digitalWrite(orange_led, HIGH);
    savefault(2);
  }
  else{
    digitalWrite(orange_led, LOW);
  }

  if(mq2value <= 850 && irvalue == HIGH){
    digitalWrite(green_led, HIGH);
  }
  else{
    digitalWrite(green_led, LOW);
  }


  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("System Status: ");
  lcd.setCursor(0,1);
  if(mq2value > 850){
    lcd.print("Smoke Detected");
  }
  else{
    lcd.print("System Active");
  }

  int btnstate = digitalRead(button);
  if(btnstate == LOW && !pressed){
    pressed = true;
    pressStart = millis();
  }
  if(btnstate == HIGH && pressed){
    unsigned long duration = millis() - pressStart;
    pressed = false;

    if(duration >= 3000) {
      savefault(3);
      displayfault();
      delay(2000);
    }
    else{
      pressCount++;
      if (pressCount == 1){
        lastPressTime = millis();
      }
      else if(pressCount == 2 && millis() - lastPressTime < 500){
        loadfault();
        displayfault();
        pressCount = 0;
        delay(2000);
      }
    }
  }
  if(pressCount == 1 && millis() - lastPressTime >= 500){
    pressCount = 0;
  }

  if(!pressed && pressCount == 0){
    displaysensor(mq2value, irvalue);
  }

  delay(500);
}
