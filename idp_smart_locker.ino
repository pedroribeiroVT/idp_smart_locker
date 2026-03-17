const int SERVO_PIN = 6; // servo PWM output
const int KEYPAD_ANALOG_PIN = A0; // analog input for keypad rows
const int COL_PINS[] = {5, 4, 3}; // keypad column outputs

// password config
#define PASSWORD_LENGTH 4
#define PASSWORD_START_ADDR 0

// servo config
#define SERVO_LOCKED_PULSE   1600
#define SERVO_UNLOCKED_PULSE 2560
#define SERVO_PERIOD_US      20000
#define SERVO_HOLD_TIME_MS   500

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

// hold # for 3 seconds while unlocked to reset password
unsigned long hashHoldStart = 0;
const unsigned long RESET_HOLD_TIME = 3000;  // 3 seconds to reset password

void setup() {
    Serial.begin(9600);
    
    // initialize hardware modules
    servo_init();
    keypad_init();
    init_watchdog();
    
    // check if password exists in EEPROM
    if (!isPasswordSet()) {
        set_clock_speed(true); // set CPU to 250kHz
        Serial.println("[SETUP] Arduino Detected");
        Serial.println("[SETUP] Enter 4-digit password on KEYPAD:");
        Serial.println("[SETUP] Press # when done");
        
        // Collect password from keypad
        char newPassword[PASSWORD_LENGTH + 1];
        int digitCount = 0;
        
        while (digitCount < PASSWORD_LENGTH) {
            char key = keypad_get_key();
            if (key >= '0' && key <= '9') {
                newPassword[digitCount] = key;
                digitCount++;
                Serial.print("[SETUP] Digit ");
                Serial.print(digitCount);
                Serial.println(": *");  // Hide actual digit for security
            }
            else if (key == '*' && digitCount > 0) {
                digitCount--;
                Serial.println("[SETUP] Digit removed");
            }
        }
        newPassword[PASSWORD_LENGTH] = '\0';
        
        // Wait for # to confirm
        Serial.println("[SETUP] Press # to confirm password");
        while (true) {
            char key = keypad_get_key();
            if (key == '#') {
                break;
            }
        }
        set_clock_speed(false); // set CPU at 16MHz
        savePassword(newPassword);
        Serial.println("[SETUP] Password set successfully!");
    } else {
        Serial.println("[SETUP] Password loaded from EEPROM");
    }
    
    // Start in locked position
    servo_lock();
    
    Serial.println("-----------------------------------------");
    Serial.println("System LOCKED");
    Serial.println("Enter password + # to unlock");
    Serial.println("When unlocked: * to lock, hold # to reset");
    Serial.println("-----------------------------------------");
    
    lastActivityTime = millis();
}

void loop() {
    // inactivity sleep check
    if (currentState != SLEEP && currentState != UNLOCKED) {
        if (millis() - lastActivityTime > INACTIVITY_TIMEOUT) {
            currentState = SLEEP;
            Serial.println("\n[STATE] -> SLEEP");
            enter_deep_sleep();
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
        case PASSWORD_INPUT: {
            set_clock_speed(true);
            char key = keypad_get_key();
            if (key >= '0' && key <= '9') {
                inputBuffer += key;
                lastActivityTime = millis();
                Serial.print("[INPUT] ");
                Serial.println(key);
            }
            else if (key == '#') {
                set_clock_speed(false);
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
                servo_unlock();
                currentState = UNLOCKED;
                Serial.println("\n[STATE] -> UNLOCKED");
                Serial.println("Press * to lock | Hold # 3s to reset password");
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
                    servo_lock();
                    currentState = ACTIVE;
                    hashHoldStart = 0;
                    
                    Serial.println("[SETUP] Enter new 4-digit password on KEYPAD:");
                    Serial.println("[SETUP] Press # when done");
                    
                    char newPassword[PASSWORD_LENGTH + 1];
                    int digitCount = 0;
                    
                    while (digitCount < PASSWORD_LENGTH) {
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
                        char k = keypad_get_key();
                        if (k == '#') break;
                    }
                    
                    savePassword(newPassword);
                    Serial.println("[SETUP] New password set!");
                    servo_lock();
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
            servo_lock();
            currentState = ACTIVE;
            lastActivityTime = millis();
            Serial.println("\n[STATE] -> ACTIVE");
            Serial.println("System LOCKED - enter password:");
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
