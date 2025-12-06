const int BUTTON_PIN = 21;
unsigned long pressStart = 0;
bool pressed = false;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Button test ready");
}

void loop() {
  int state = digitalRead(BUTTON_PIN);
  if (state == LOW && !pressed) {
    pressed = true;
    pressStart = millis();
  }
  if (state == HIGH && pressed) {
    unsigned long duration = millis() - pressStart;
    pressed = false;
    if (duration >= 3000) {
      Serial.println("Long press detected (>=3s)");
    } else {
      Serial.print("Short press: ");
      Serial.print(duration);
      Serial.println(" ms");
    }
    delay(200); 
  }
}
