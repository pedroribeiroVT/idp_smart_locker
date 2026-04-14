const int SERVO_PIN = 6; // servo PWM output
const int KEYPAD_ANALOG_PIN = A0; // analog input for keypad rows
const int COL_PINS[] = {5, 4, 3}; // keypad column outputs
const int CURRENT_SENSE_PIN = A1; // low-side current shunt sense

// password config
#define PASSWORD_LENGTH 4
#define PASSWORD_START_ADDR 0

// servo config
#define SERVO_LOCKED_PULSE   1600
#define SERVO_UNLOCKED_PULSE 2560
#define SERVO_PERIOD_US      20000
#define SERVO_HOLD_TIME_MS   500
#define SERVO_STALL_THRESHOLD  110   // ADC counts ~540mA with 1Ω shunt
#define SERVO_STALL_CONSEC     3     // consecutive over-threshold readings = stall
#define SERVO_MOVE_WINDOW_MS   150   // ignore stall flags during initial movement

// keypad config
#define KEYPAD_SETTLE_US   800 
#define KEYPAD_DEBOUNCE_MS 150
#define KEYPAD_ZERO_MAX    10
#define KEYPAD_CUT_0_1     700
#define KEYPAD_CUT_1_2     410
#define KEYPAD_CUT_2_3     290

enum State {
    SLEEP,
    ACTIVE,
    PASSWORD_INPUT,
    VERIFICATION,
    UNLOCKED,
    LOCKING,
    ERROR,
};

State currentState = ACTIVE;

String inputBuffer = "";
unsigned long lastActivityTime = 0;
const unsigned long INACTIVITY_TIMEOUT = 30000;  // 30 seconds until SLEEP is activated

// Battery / cycle tracking
// Timestamp set at startup and reset after each wakeup from deep sleep.
// cycle_mark_active() is called with the elapsed time just before sleeping
// so battery_life_report() has accurate active-phase data on next wakeup.
unsigned long wakeTime = 0;
// Assumed average unlock operations per day used for battery-life projection.
// Adjust to match expected usage pattern.
#define ASSUMED_CYCLES_PER_DAY 5.0f

// hold # for 3 seconds while unlocked to reset password
unsigned long hashHoldStart = 0;
const unsigned long RESET_HOLD_TIME = 3000;  // 3 seconds to reset password

void setup() {
    Serial.begin(9600);
    
    // initialize hardware modules
    servo_init();
    keypad_init();
    init_watchdog();
    power_optimize_init();
    
    // check if password exists in EEPROM
    if (!isPasswordSet()) {
        Serial.println("[SETUP] Arduino Detected");
        Serial.println("[SETUP] Enter 4-digit password on KEYPAD:");
        Serial.println("[SETUP] Press # when done");

        // Collect password from keypad
        char newPassword[PASSWORD_LENGTH + 1];
        int digitCount = 0;

        while (digitCount < PASSWORD_LENGTH) {
            reset_watchdog();
            char key = keypad_get_key();
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
            reset_watchdog();  // FIX(1): keep WDT alive while waiting for confirmation
            char key = keypad_get_key();
            if (key == '#') {
                break;
            }
        }
        savePassword(newPassword);
        Serial.println("[SETUP] Password set successfully!");
    } else {
        Serial.println("[SETUP] Password loaded from EEPROM");
    }
    
    // Start in locked position
    if (!servo_lock()) {
        Serial.println("[SETUP] WARNING: servo blocked during initial lock");
    }
    
    Serial.println("-----------------------------------------");
    Serial.println("System LOCKED");
    Serial.println("Enter password + # to unlock");
    Serial.println("When unlocked: * to lock, hold # to reset");
    Serial.println("-----------------------------------------");
    
    lastActivityTime = millis();
    wakeTime = millis();  // start timing the first active cycle
}

void loop() {
    // inactivity sleep check
    if (currentState != SLEEP && currentState != UNLOCKED) {
        if (millis() - lastActivityTime > INACTIVITY_TIMEOUT) {
            currentState = SLEEP;

            // Finalise this cycle's active-phase timing before sleeping.
            // Servo time was already accumulated by servo_lock/servo_unlock.
            cycle_mark_active(millis() - wakeTime);

            Serial.println(F("\n[STATE] -> SLEEP"));
            Serial.flush();  // ensure all output is transmitted before the UART goes quiet
            enter_deep_sleep();

            // === Resumed from deep sleep ===
            // Reset the cycle clock and report the stats from the cycle that
            // just ended so the user sees real data immediately on wakeup.
            wakeTime = millis();
            battery_life_report(ASSUMED_CYCLES_PER_DAY);
            cycle_timer_reset();
        }
    }

    // STATE switch cases
    switch (currentState) {
        // SLEEP: goes into low power
        case SLEEP: {
            char key = keypad_get_key();
            if (key != '\0') {
                currentState = ACTIVE;
                lastActivityTime = millis();
                Serial.println("\n[STATE] -> ACTIVE");
                Serial.println("Enter password:");
            }
            break;
        }

        // ACTIVE: waiting for first input
        case ACTIVE: {
            char key = keypad_get_key();
            if (key >= '0' && key <= '9') {
                inputBuffer = String(key);
                currentState = PASSWORD_INPUT;
                lastActivityTime = millis();
                Serial.print("[INPUT] ");
                Serial.println(key);
            }
            break;
        }

        // PASSWORD_INPUT: collecting digits
        // Note: the clock is kept at 16 MHz throughout this state.
        // Slowing to 250 kHz via CLKPR also scales Timer0, so delay() and
        // millis() run 64x slower in wall time. A single keypad_get_key()
        // call (delay(150) debounce) would take ~9.6 s real time, exceeding
        // the 4 s WDT window before reset_watchdog() could fire.
        case PASSWORD_INPUT: {
            char key = keypad_get_key();
            if (key >= '0' && key <= '9') {
                inputBuffer += key;
                lastActivityTime = millis();
                Serial.print("[INPUT] ");
                Serial.println(key);
            }
            else if (key == '#') {
                currentState = VERIFICATION;
                Serial.println("\n[STATE] -> VERIFICATION");
            }
            else if (key == '*') {
                inputBuffer = "";
                currentState = ACTIVE;
                Serial.println("[INPUT] Cleared - enter password:");
            }
            break;
        }

        // VERIFICATION: Check password
        case VERIFICATION: {
            Serial.print("[VERIFY] Checking: ");
            Serial.println(inputBuffer);
            
            if (verifyPassword(inputBuffer)) {
                Serial.println("[VERIFY] ACCESS GRANTED");
                if (servo_unlock()) {
                    currentState = UNLOCKED;
                    Serial.println("\n[STATE] -> UNLOCKED");
                    Serial.println("Press * to lock | Hold # 3s to reset password");
                } else {
                    currentState = ERROR;
                    Serial.println("\n[STATE] -> ERROR (servo blocked, still locked)");
                }
            } else {
                Serial.println("[VERIFY] ACCESS DENIED");
                currentState = ERROR;
                Serial.println("\n[STATE] -> ERROR");
            }
            
            inputBuffer = "";
            break;
        }

        // UNLOCKED: waiting for lock command, hold # for 3 seconds to reset password
        case UNLOCKED: {
            char key = scanKeyOnce();
            
            if (key == '*') {
                currentState = LOCKING;
                hashHoldStart = 0;
                Serial.println("\n[STATE] -> LOCKING");
            }
            else if (key == '#') {
                if (hashHoldStart == 0) {
                    hashHoldStart = millis();
                    Serial.println("[RESET] Hold # for 3 seconds to reset password...");
                }
                else if (millis() - hashHoldStart >= RESET_HOLD_TIME) {
                    Serial.println("\n[RESET] Password reset triggered!");
                    clearPassword();
                    if (!servo_lock()) {
                        Serial.println("[RESET] WARNING: servo blocked during reset lock");
                    }
                    currentState = ACTIVE;
                    hashHoldStart = 0;
                    
                    Serial.println("[SETUP] Enter new 4-digit password on KEYPAD:");
                    Serial.println("[SETUP] Press # when done");
                    
                    char newPassword[PASSWORD_LENGTH + 1];
                    int digitCount = 0;
                    
                    while (digitCount < PASSWORD_LENGTH) {
                        reset_watchdog();  // FIX(1): keep WDT alive during password re-entry
                        char k = keypad_get_key();
                        if (k >= '0' && k <= '9') {
                            newPassword[digitCount] = k;
                            digitCount++;
                            Serial.print("[SETUP] Digit ");
                            Serial.print(digitCount);
                            Serial.println(": *");
                        }
                        else if (k == '*' && digitCount > 0) {
                            digitCount--;
                            Serial.println("[SETUP] Digit removed");
                        }
                    }
                    newPassword[PASSWORD_LENGTH] = '\0';

                    Serial.println("[SETUP] Press # to confirm password");
                    while (true) {
                        reset_watchdog();  // FIX(1): keep WDT alive while waiting for confirmation
                        char k = keypad_get_key();
                        if (k == '#') break;
                    }
                    
                    savePassword(newPassword);
                    Serial.println("[SETUP] New password set!");
                    if (!servo_lock()) {
                        Serial.println("[RESET] WARNING: servo blocked after new password set");
                    }
                    lastActivityTime = millis();
                }
            }
            else {
                if (hashHoldStart != 0) {
                    hashHoldStart = 0;
                    Serial.println("[RESET] Cancelled");
                }
            }
            delay(50); // small delay for held key detection
            break;
        }

        // LOCKING: engage lock mechanism
        case LOCKING: {
            if (servo_lock()) {
                currentState = ACTIVE;
                lastActivityTime = millis();
                Serial.println("\n[STATE] -> ACTIVE");
                Serial.println("System LOCKED - enter password:");
            } else {
                currentState = UNLOCKED;
                Serial.println("\n[STATE] -> UNLOCKED (servo blocked, reverted)");
                Serial.println("Press * to lock | Hold # 3s to reset password");
            }
            break;
        }

        // ERROR: brief delay then reset
        case ERROR: {
            delay(2000);
            currentState = ACTIVE;
            lastActivityTime = millis();
            Serial.println("\n[STATE] -> ACTIVE");
            Serial.println("Try again - enter password:");
            break;
        }
    }
}
