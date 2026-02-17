#define SERVO_PIN 3

int degToPulse(int deg) {
  // converting from degrees to microdegrees, and mapping range
  // to 1000 to 2000 us
  return 1000 + (deg * 1000) / 180;
}

// the servo requires 50hz signal which is 20000us
// we turn the signal on, then wait the period - pulse length
// to generate the PWM signal
void sendServo(int deg) {
  int pulse = degToPulse(deg);

  digitalWrite(SERVO_PIN, HIGH);a
  delayMicroseconds(pulse);

  digitalWrite(SERVO_PIN, LOW);
  delayMicroseconds(20000 - pulse);
}

void setup() {
  pinMode(SERVO_PIN, OUTPUT);
}

void sendPulseUs(int pulse) {
  digitalWrite(SERVO_PIN, HIGH);
  delayMicroseconds(pulse);
  digitalWrite(SERVO_PIN, LOW);
  delayMicroseconds(20000 - pulse);
}
void holdPulse(int pulse, int ms) {
  for (int i = 0; i < ms / 20; i++) sendPulseUs(pulse);
}

void loop() {
  // 2500 for 90deg
  // 1600 for 0deg
  holdPulse(2500, 2000);
}