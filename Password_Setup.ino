void setupNewPasswordFlow(const char *startMsg1, const char *startMsg2, const char *doneMsg) {
    char newPassword[PASSWORD_LENGTH + 1];
    int digitCount = 0;

    setLedMode(LED_BLINK_MODE);
    Serial.println(startMsg1);
    Serial.println(startMsg2);

    while (digitCount < PASSWORD_LENGTH) {
        updateLed();
        char key = keypad_get_key();

        if (key != '\0') {
            triggerLedPulse();
        }

        if (key >= '0' && key <= '9') {
            newPassword[digitCount] = key;
            digitCount++;
            Serial.print("[SETUP] Digit ");
            Serial.print(digitCount);
            Serial.println(": *");
        }
        else if (key == '*' && digitCount > 0) {
            digitCount--;
            Serial.println("[SETUP] Digit removed");
        }
    }

    newPassword[PASSWORD_LENGTH] = '\0';

    Serial.println("[SETUP] Press # to confirm password");
    while (true) {
        updateLed();
        char key = keypad_get_key();

        if (key != '\0') {
            triggerLedPulse();
        }

        if (key == '#') {
            break;
        }
    }

    savePassword(newPassword);
    printStoredPasswordHash();
    setLedMode(LED_OFF_MODE);
    Serial.println(doneMsg);
}
