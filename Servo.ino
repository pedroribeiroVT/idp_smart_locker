// const int SERVO_PIN = 10; // must be Arduino pin 10 = OC1B hardware PWM
// physical pin on ATmega328P DIP = 16

// servo config
#define SERVO_LOCKED_PULSE   1600
#define SERVO_UNLOCKED_PULSE 2560
#define SERVO_PERIOD_US      20000
#define SERVO_HOLD_TIME_MS   500

void servo_write_pulse_us(int pulseUs) {
    // OCR1B turns output on for given time
    // prescaler = 8
    // 16 MHz / 8 = 2 MHz clock
    // each timer tick = 0.5 us
    // OCR1B = pulseUs / 0.5 = pulseUs * 2
    OCR1B = pulseUs * 2;
}

void servo_move(int pulseUs) {
    servo_write_pulse_us(pulseUs);

    // keep PWM running long enough for servo to move
    delay(SERVO_HOLD_TIME_MS);

    Serial.print("[SERVO] Position set to ");
    Serial.print(pulseUs);
    Serial.println(" us");
}

void servo_lock() {
    servo_move(SERVO_LOCKED_PULSE);
    Serial.println("[SERVO] LOCKED");
}

void servo_unlock() {
    servo_move(SERVO_UNLOCKED_PULSE);
    Serial.println("[SERVO] UNLOCKED");
}

void servo_init() {
    // PB2 = OC1B = physical pin 16 as output
    DDRB |= (1 << DDB2);

    // clear old timer settings
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;

    // 16 MHz clock / 8 prescaler = 2 MHz
    // 20 ms = 20000 us
    // 20000 us / 0.5 us = 40000 ticks
    // TOP = 39999
    ICR1 = 39999;

    // Fast PWM mode 14: WGM13:0 = 1110
    TCCR1A |= (1 << WGM11);
    TCCR1B |= (1 << WGM13) | (1 << WGM12);

    // non-inverting PWM on OC1B
    TCCR1A |= (1 << COM1B1);

    // prescaler = 8
    TCCR1B |= (1 << CS11);

    // start in locked position
    servo_write_pulse_us(SERVO_LOCKED_PULSE);

    Serial.println("[SERVO] Timer1 hardware PWM initialized");
    Serial.println("[SERVO] Signal pin: Arduino D10 / physical pin 16");
    Serial.print("[SERVO] TOP (ICR1): ");
    Serial.println(ICR1);
    Serial.print("[SERVO] Locked pulse: ");
    Serial.print(SERVO_LOCKED_PULSE);
    Serial.println(" us");
    Serial.print("[SERVO] Unlocked pulse: ");
    Serial.print(SERVO_UNLOCKED_PULSE);
    Serial.println(" us");
}

void servo_test(int pulseUs) {
    Serial.print("[SERVO TEST] Sending pulse: ");
    Serial.print(pulseUs);
    Serial.println(" us");
    servo_move(pulseUs);
}