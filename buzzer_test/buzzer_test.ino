const int buzzer = 14;

void setup(){
  Serial.begin(115200);
  pinMode(buzzer, OUTPUT);
}

void loop(){
  digitalWrite(buzzer, HIGH);
  delay(1000);
  digitalWrite(buzzer, LOW);
  delay(1000);
}
