#define TRIG_PIN 5   // GPIO 5
#define ECHO_PIN 19  // GPIO 19


void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void loop() {
  // Clear trigger pin
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Send a 10us pulse to trigger pin
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, LOW);

  // Read echo pin (time in microseconds)
  long duration = pulseIn(ECHO_PIN, HIGH);

  // Calculate distance (speed of sound is ~343 m/s)
  int distance = duration / 58;

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  delay(500); // update every half second
}
