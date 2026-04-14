void servo_send_pulse(int pulseUs) {
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(pulseUs);
    digitalWrite(SERVO_PIN, LOW);
    delayMicroseconds(SERVO_PERIOD_US - pulseUs);
}

int servo_read_current() {
    return analogRead(CURRENT_SENSE_PIN);
}

// Returns true if move completed normally, false if blockage detected (and reverted).
bool servo_move(int targetPulseUs, int prevPulseUs) {
    int iterations = SERVO_HOLD_TIME_MS / 20; // 500ms / 20ms = 25 iterations
    int stallCount = 0;

    for (int i = 0; i < iterations; i++) {
        servo_send_pulse(targetPulseUs);

        // Skip stall checks during initial movement window
        if (i < SERVO_MOVE_WINDOW_MS / 20) continue;

        int adcVal = servo_read_current();
        if (adcVal > SERVO_STALL_THRESHOLD) {
            stallCount++;
        } else {
            stallCount = 0;
        }

        if (stallCount >= SERVO_STALL_CONSEC) {
            Serial.println("[SERVO] BLOCKED - reverting");
            for (int j = 0; j < 15; j++) {
                servo_send_pulse(prevPulseUs);
            }
            return false;
        }
    }

    Serial.print("[SERVO] Position set to ");
    Serial.print(targetPulseUs);
    Serial.println(" us");
    return true;
}

bool servo_lock() {
    bool ok = servo_move(SERVO_LOCKED_PULSE, SERVO_UNLOCKED_PULSE);
    if (ok) {
        // Record the actual hold window so battery_life_report() can account
        // for this servo operation's current draw.
        cycle_mark_servo(SERVO_HOLD_TIME_MS);
        Serial.println("[SERVO] LOCKED");
    }
    return ok;
}

bool servo_unlock() {
    bool ok = servo_move(SERVO_UNLOCKED_PULSE, SERVO_LOCKED_PULSE);
    if (ok) {
        cycle_mark_servo(SERVO_HOLD_TIME_MS);
        Serial.println("[SERVO] UNLOCKED");
    }
    return ok;
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
    servo_move(pulseUs, pulseUs); // no revert target for manual test
}

