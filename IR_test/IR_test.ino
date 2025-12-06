const int IR = 27;

void setup(){
  Serial.begin(115200);
  pinMode(IR, INPUT);
  Serial.println("IR TEST");
}

void loop(){
  int val = digitalRead(IR);
  Serial.print("IR values: ");
  Serial.println(val);
  delay(2000);
}
