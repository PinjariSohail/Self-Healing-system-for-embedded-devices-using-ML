// LCD_SDA 21
//LCD_SCL 22

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>


#define mq2 34
#define ir 27

#define red_led 25
#define orange_led 26
#define green_led 33

#define buzzer 14

#define TFT_CS -1 //not used
#define TFT_DC 2
#define TFT_RST 4
#define TFT_MOSI 23//SDA
#define TFT_SCLK 18//SCL
#define TFT_BLK 15

LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

void setup(){
  Serial.begin(115200);

  pinMode(red_led, OUTPUT);
  pinMode(orange_led, OUTPUT);
  pinMode(green_led, OUTPUT);
  
  pinMode(buzzer, OUTPUT);

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
  
}

void loop(){
  int mq2value = analogRead(mq2);
  int irvalue = digitalRead(ir);

  /*if(mq2value >400){
    digitalWrite(red_led, HIGH);
    digitalWrite(buzzer, HIGH);
  }
  else{
    digitalWrite(red_led, LOW);
    digitalWrite(buzzer, LOW);
  }*/

  if(irvalue == LOW){
    digitalWrite(orange_led, HIGH);
  }
  else{
    digitalWrite(orange_led, LOW);
  }

  if(mq2value <=400 && irvalue == HIGH){
    digitalWrite(green_led, HIGH); 
  }
  else{
    digitalWrite(green_led, LOW);
  }
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_BLUE);
  tft.setCursor(10, 40);
  tft.println("MQ-2 value: ");
  tft.setCursor(10, 70);
  tft.setTextColor(ST77XX_GREEN);
  tft.println(mq2value);

  tft.setTextColor(ST77XX_BLUE);
  tft.setCursor(10, 100);
  tft.println("motion : ");
  tft.setCursor(10, 130);
  tft.setTextColor(ST77XX_GREEN);
  if(irvalue == LOW) tft.println("Detected");
  else tft.println("None");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System status: ");
  lcd.setCursor(0, 1);
  if(mq2value > 400){
    digitalWrite(red_led, HIGH);
    digitalWrite(buzzer, HIGH); 
    lcd.print("Smoke detected");
  }
  else{
    digitalWrite(red_led, LOW);
    digitalWrite(buzzer, LOW);
    lcd.print("System Active");
  }

  delay(1000);
}
