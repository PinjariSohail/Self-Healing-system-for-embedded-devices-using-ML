const int mq2 = 34;

void setup(){
  Serial.begin(115200);
  Serial.println("MQ-2 test");
}

void loop(){
  int val = analogRead(mq2);
  Serial.print("mq-2 value: ");
  Serial.println(val);
  delay(500);
}
