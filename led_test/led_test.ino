const int orange = 15;
const int blue = 13;
const int red = 4;
const int green = 5;

void setup(){
  Serial.begin(115200);
  pinMode(orange, OUTPUT);
  pinMode(blue, OUTPUT);
  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);
}

void loop(){
  digitalWrite(orange, HIGH);
  delay(500);
  digitalWrite(orange,LOW);

  digitalWrite(blue,HIGH);
  delay(500);
  digitalWrite(blue,LOW);
  
  digitalWrite(red, HIGH);
  delay(500);
  digitalWrite(red, LOW);

  digitalWrite(green, HIGH);
  delay(500);
  digitalWrite(green,LOW);

  delay(1000);
}
