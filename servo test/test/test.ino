#include <ESP32Servo.h>

Servo servo1;  // Create servo object

int servoPin = 19;  // PWM-capable pin

void setup() {
  Serial.begin(115200);
  servo1.attach(servoPin);  // Attach servo to pin
  Serial.println("Servo test started");
}

void loop() {
  servo1.write(0);
  delay(2000);

  servo1.write(90);
  delay(2000);
}
