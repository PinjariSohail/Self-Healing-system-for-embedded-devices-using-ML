#include <Adafruit_ST7735.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_ST7796S.h>
#include <Adafruit_ST77xx.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// Pin mapping for ESP32
#define TFT_CS -1 //not used
#define TFT_DC     2
#define TFT_RST    4
#define TFT_MOSI  23 // SDA
#define TFT_SCLK  18 // SCL
#define TFT_BLK   15

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(115200);
  
  // Init backlight
  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH); // Turn backlight ON

  // Initialize SPI bus
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);

  // Initialize display
  tft.init(240, 240, SPI_MODE3);  // ST7789 240x240
  tft.setRotation(0);  // Rotate if needed

  // Fill screen and write text
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(30, 100);
  tft.println("ESP32 OLED");
  tft.setCursor(30, 130);
  tft.println("240x240 IPS");
}

void loop() {
}
