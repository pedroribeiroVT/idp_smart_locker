void servo_send_pulse(int pulseUs) {
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(pulseUs);
    digitalWrite(SERVO_PIN, LOW);
    delayMicroseconds(SERVO_PERIOD_US - pulseUs);
}

void servo_move(int pulseUs) {
    int iterations = SERVO_HOLD_TIME_MS / 20; // each pulse = ~20ms
    for (int i = 0; i < iterations; i++) {
        servo_send_pulse(pulseUs);
    }
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
    pinMode(SERVO_PIN, OUTPUT);
    digitalWrite(SERVO_PIN, LOW);
    Serial.println("[SERVO] Initialized");
    Serial.print("[SERVO] Signal pin: ");
    Serial.println(SERVO_PIN);
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

