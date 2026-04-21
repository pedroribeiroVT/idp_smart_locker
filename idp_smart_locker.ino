const int SERVO_PIN = 10; // servo PWM output
const int KEYPAD_ANALOG_PIN = A0; // analog input for keypad rows
const int COL_PINS[] = {5, 4, 3}; // keypad column outputs
const int LED_PIN = 2; // led pin

// password config
#define PASSWORD_LENGTH 4
#define PASSWORD_START_ADDR 0

// servo config
#define SERVO_LOCKED_PULSE   1600
#define SERVO_UNLOCKED_PULSE 2560
#define SERVO_PERIOD_US      20000
#define SERVO_HOLD_TIME_MS   750

// keypad config
#define KEYPAD_SETTLE_US   800
#define KEYPAD_DEBOUNCE_MS 150
#define KEYPAD_ZERO_MAX    10
#define KEYPAD_CUT_0_1     700
#define KEYPAD_CUT_1_2     410
#define KEYPAD_CUT_2_3     290

// LED config
#define LED_BLINK_INTERVAL_MS 300
#define LED_PULSE_MS          500

enum State {
    SLEEP,
    ACTIVE,
    PASSWORD_INPUT,
    VERIFICATION,
    UNLOCKED,
    LOCKING,
    ERROR,
};

enum LedMode {
    LED_OFF_MODE,
    LED_ON_MODE,
    LED_BLINK_MODE
};

State currentState = ACTIVE;
LedMode ledMode = LED_OFF_MODE;

String inputBuffer = "";
unsigned long lastActivityTime = 0;
const unsigned long INACTIVITY_TIMEOUT = 30000;  // 30 seconds until SLEEP is activated

// hold # for 3 seconds while unlocked to reset password
unsigned long hashHoldStart = 0;
const unsigned long RESET_HOLD_TIME = 3000;  // 3 seconds to reset password

// LED timing/state
unsigned long lastLedToggleTime = 0;
bool ledBlinkState = false;
bool ledPulseActive = false;
unsigned long ledPulseStartTime = 0;

// ---------- Forward declarations ----------
void led_init();
void setLedMode(LedMode mode);
void triggerLedPulse();
void updateLed();

void setupNewPasswordFlow(const char *startMsg1, const char *startMsg2, const char *doneMsg);

// ---------- LED functions ----------
void led_init() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
}

void setLedMode(LedMode mode) {
    if (ledMode == mode) return;

    ledMode = mode;

    if (ledMode == LED_OFF_MODE) {
        ledBlinkState = false;
        digitalWrite(LED_PIN, LOW);
    }
    else if (ledMode == LED_ON_MODE) {
        ledBlinkState = true;
        digitalWrite(LED_PIN, HIGH);
    }
    else if (ledMode == LED_BLINK_MODE) {
        lastLedToggleTime = millis();
        ledBlinkState = true;
        digitalWrite(LED_PIN, HIGH);
    }
}

void triggerLedPulse() {
    ledPulseActive = true;
    ledPulseStartTime = millis();
    digitalWrite(LED_PIN, HIGH);
}

void updateLed() {
    // pulse only matters during blink mode password-setup flows,
    // but restoring the base mode here keeps behavior clean
    if (ledPulseActive) {
        if (millis() - ledPulseStartTime >= LED_PULSE_MS) {
            ledPulseActive = false;

            if (ledMode == LED_OFF_MODE) {
                digitalWrite(LED_PIN, LOW);
            }
            else if (ledMode == LED_ON_MODE) {
                digitalWrite(LED_PIN, HIGH);
            }
            else if (ledMode == LED_BLINK_MODE) {
                digitalWrite(LED_PIN, ledBlinkState ? HIGH : LOW);
            }
        }
        return;
    }

    if (ledMode == LED_BLINK_MODE) {
        if (millis() - lastLedToggleTime >= LED_BLINK_INTERVAL_MS) {
            lastLedToggleTime = millis();
            ledBlinkState = !ledBlinkState;
            digitalWrite(LED_PIN, ledBlinkState ? HIGH : LOW);
        }
    }
    else if (ledMode == LED_ON_MODE) {
        digitalWrite(LED_PIN, HIGH);
    }
    else {
        digitalWrite(LED_PIN, LOW);
    }
}


void setup() {
    Serial.begin(9600);
    delay(250);
    Serial.println("[BOOT] Setup started");

    // initialize hardware modules
    servo_init();
    keypad_init();
    led_init();
    battery_monitor_init();
    battery_check_and_alert();
    setLedMode(LED_OFF_MODE);

    init_watchdog();
    
    // check if password exists in EEPROM
    if (!isPasswordSet()) {
        Serial.println("[SETUP] Arduino Detected");
        Serial.println("[SETUP] Enter 4-digit password on KEYPAD:");
        Serial.println("[SETUP] Press # when done");
        
        // Collect password from keypad
        char newPassword[PASSWORD_LENGTH + 1];
        int digitCount = 0;
        
        while (digitCount < PASSWORD_LENGTH) {
            updateLed();
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
            updateLed();
            char key = keypad_get_key();
            if (key == '#') {
                break;
            }
        }
        savePassword(newPassword);
        Serial.println("[SETUP] Password set successfully!");
    } else {
        Serial.println("[SETUP] Password loaded from EEPROM");
        setLedMode(LED_OFF_MODE);
    }

    // Start in locked position
    servo_lock();
    setLedMode(LED_OFF_MODE);

    Serial.println("-----------------------------------------");
    Serial.println("System LOCKED");
    Serial.println("Enter password + # to unlock");
    Serial.println("When unlocked: * to lock, hold # to reset");
    Serial.println("-----------------------------------------");

    lastActivityTime = millis();
}

// ---------- Main loop ----------
void loop() {
    updateLed();

    // inactivity sleep check
    if (currentState != SLEEP && currentState != UNLOCKED) {
        if (millis() - lastActivityTime > INACTIVITY_TIMEOUT) {
            currentState = SLEEP;
            setLedMode(LED_OFF_MODE);
            Serial.println("\n[STATE] -> SLEEP");
            setLedMode(LED_OFF_MODE);
            enter_deep_sleep();
        }
    }

    switch (currentState) {
        // SLEEP: goes into low power
        case SLEEP: {
            setLedMode(LED_OFF_MODE);
            char key = keypad_get_key();

            if (key != '\0') {
                currentState = ACTIVE;
                lastActivityTime = millis();
                Serial.println("\n[STATE] -> ACTIVE");
                Serial.println("Enter password:");
            }
            else {
                enter_deep_sleep();
            }
            break;
        }

        case ACTIVE: {
            setLedMode(LED_OFF_MODE);
            char key = keypad_get_key();

            if (key >= '0' && key <= '9') {
                inputBuffer = String(key);
                currentState = PASSWORD_INPUT;
                lastActivityTime = millis();
                if (key != '\0') {
                    triggerLedPulse();
                }
                Serial.print("[INPUT] ");
                Serial.println(key);
            }
            break;
        }

        // PASSWORD_INPUT: collecting digits
        case PASSWORD_INPUT: {
            setLedMode(LED_OFF_MODE);
            char key = keypad_get_key();
            if (key != '\0') {
                triggerLedPulse();
            }
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

        case VERIFICATION: {
            setLedMode(LED_OFF_MODE);
            Serial.print("[VERIFY] Checking: ");
            Serial.println(inputBuffer);

            if (verifyPassword(inputBuffer)) {
                Serial.println("[VERIFY] ACCESS GRANTED");
                servo_unlock();
                currentState = UNLOCKED;
                setLedMode(LED_ON_MODE);
                Serial.println("\n[STATE] -> UNLOCKED");
                Serial.println("Press * to lock | Hold # 3s to reset password");
            } else {
                Serial.println("[VERIFY] ACCESS DENIED");
                currentState = ERROR;
                setLedMode(LED_OFF_MODE);
                Serial.println("\n[STATE] -> ERROR");
            }

            inputBuffer = "";
            break;
        }

        case UNLOCKED: {
            setLedMode(LED_ON_MODE);
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
                    hashHoldStart = 0;

                    currentState = ACTIVE;
                    setLedMode(LED_OFF_MODE);
                    lastActivityTime = millis();
                    Serial.println("\n[STATE] -> ACTIVE");
                    Serial.println("System LOCKED - enter password:");
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

        case LOCKING: {
            servo_lock();
            setLedMode(LED_OFF_MODE);
            currentState = ACTIVE;
            lastActivityTime = millis();
            Serial.println("\n[STATE] -> ACTIVE");
            Serial.println("System LOCKED - enter password:");
            break;
        }

        case ERROR: {
            setLedMode(LED_OFF_MODE);
            delay(2000);
            setLedMode(LED_OFF_MODE);
            currentState = ACTIVE;
            lastActivityTime = millis();
            Serial.println("\n[STATE] -> ACTIVE");
            Serial.println("Try again - enter password:");
            break;
        }
    }
}
