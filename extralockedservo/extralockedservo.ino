const int SERVO_PIN = 10;  // Arduino D10 = Timer1 OC1B

#define SERVO_LOCKED_PULSE_US    1600
#define SERVO_UNLOCKED_PULSE_US  2560
#define SERVO_PERIOD_US         20000
#define SERVO_MOVE_HOLD_MS        750
#define SERVO_STATE_DELAY_MS     2500

void servo_write_pulse_us(int pulseUs) {
    OCR1B = pulseUs * 2;  // Timer1 runs at 2 MHz with prescaler 8
}

void servo_move(int pulseUs) {
    servo_write_pulse_us(pulseUs);
    delay(SERVO_MOVE_HOLD_MS);

    Serial.print("[SERVO] Pulse set to ");
    Serial.print(pulseUs);
    Serial.println(" us");
}

void servo_lock() {
    servo_move(SERVO_LOCKED_PULSE_US);
    Serial.println("[SERVO] LOCKED");
}

void servo_unlock() {
    servo_move(SERVO_UNLOCKED_PULSE_US);
    Serial.println("[SERVO] UNLOCKED");
}

void servo_init() {
    pinMode(SERVO_PIN, OUTPUT);

    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;

    // 16 MHz / 8 prescaler = 2 MHz, so 20 ms period = 40000 ticks.
    ICR1 = (SERVO_PERIOD_US * 2) - 1;

    // Fast PWM, mode 14, TOP = ICR1.
    TCCR1A |= (1 << WGM11);
    TCCR1B |= (1 << WGM13) | (1 << WGM12);

    // Non-inverting PWM on OC1B (Arduino D10).
    TCCR1A |= (1 << COM1B1);

    // Prescaler = 8.
    TCCR1B |= (1 << CS11);

    servo_write_pulse_us(SERVO_LOCKED_PULSE_US);

    Serial.println("[SERVO] Timer1 initialized on D10");
    Serial.print("[SERVO] Locked pulse: ");
    Serial.print(SERVO_LOCKED_PULSE_US);
    Serial.println(" us");
    Serial.print("[SERVO] Unlocked pulse: ");
    Serial.print(SERVO_UNLOCKED_PULSE_US);
    Serial.println(" us");
}

void setup() {
    Serial.begin(9600);
    delay(250);
    Serial.println("[BOOT] Extra locked servo test");

    servo_init();
    servo_lock();
}

void loop() {
    delay(SERVO_STATE_DELAY_MS);
    servo_unlock();

    delay(SERVO_STATE_DELAY_MS);
    servo_lock();
}
