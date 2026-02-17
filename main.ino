// hardware registers for EEPROM
#define REG_EECR *((volatile unsigned char*)0x3F)
#define REG_EEDR *((volatile unsigned char*)0x40)
#define REG_EEARL*((volatile unsigned char*)0x41)
#define REG_SREG *((volatile unsigned char*)0x5F)

#define BIT_EERE 0 
#define BIT_EEPE 1 
#define BIT_EEMPE 2 

const int SERVO_PIN = 9;
const int KEYPAD_ANALOG_PIN = A0; 
const int COL_PINS[] = {2, 3, 4}; 

// states
enum State { SLEEP, ACTIVE, PASSWORD_INPUT, VERIFICATION, UNLOCKED, LOCKING, ERROR };
State currentState = ACTIVE; 

String inputBuffer = "";
unsigned long lastActivityTime = 0;
const unsigned long INACTIVITY_TIMEOUT = 30000; 

// eeprom write function
void raw_eeprom_write(unsigned int address, unsigned char data) {
    // pre-read to save 1 write cycle if data is identical
    if (raw_eeprom_read(address) == data) {
        Serial.print("Endurance Save: Address ");
        Serial.print(address);
        Serial.println(" already contains this value. Skipping write.");
        return;
    }

    while (REG_EECR & (1 << BIT_EEPE)); // wait for previous write
    REG_EEARL = (unsigned char)address;
    REG_EEDR = data;
    unsigned char sreg_backup = REG_SREG;
    REG_SREG &= ~(1 << 7); // disable interrupts for 4-cycle window safety
    REG_EECR |= (1 << BIT_EEMPE);
    REG_EECR |= (1 << BIT_EEPE);
    REG_SREG = sreg_backup; 
    Serial.println("Silicon Write Executed.");
}

// eeprom read function
unsigned char raw_eeprom_read(unsigned int address) {
    while (REG_EECR & (1 << BIT_EEPE));
    REG_EEARL = (unsigned char)address;
    REG_EECR |= (1 << BIT_EERE); 
    return REG_EEDR;
}

void setup() {
    Serial.begin(9600);
    pinMode(SERVO_PIN, OUTPUT);
    for (int i=0; i<3; i++) {
        pinMode(COL_PINS[i], OUTPUT);
        digitalWrite(COL_PINS[i], HIGH);
    }
    
    Serial.println("--- System Initialized ---");
    // password persistence check
    if (raw_eeprom_read(0) == 0xFF) {
        Serial.println("NEW CHIP DETECTED: Set 4-digit Password...");
        while (Serial.available() < 4);
        for (int i = 0; i < 4; i++) {
            raw_eeprom_write(i, Serial.read());
        }
    } else {
        Serial.println("Password loaded from non-volatile memory.");
    }
    lastActivityTime = millis();
}

void loop() {
    if (currentState != SLEEP && (millis() - lastActivityTime > INACTIVITY_TIMEOUT)) {
        currentState = SLEEP;
        Serial.println("Transition: SLEEP (Power Save Mode Active)");
    }

    switch (currentState) {
        case SLEEP:
            if (checkForKeypress() != '\0') {
                currentState = ACTIVE;
                Serial.println("Wake Up: Transition to ACTIVE");
                lastActivityTime = millis();
            }
            break;

        case ACTIVE:
            char key = checkForKeypress();
            if (key >= '0' && key <= '9') {
                inputBuffer = String(key);
                currentState = PASSWORD_INPUT;
                Serial.print("Input: "); Serial.println(key);
                lastActivityTime = millis();
            }
            break;

        case PASSWORD_INPUT:
            char input = checkForKeypress();
            if (input >= '0' && input <= '9') {
                inputBuffer += input;
                Serial.print("Input: "); Serial.println(input);
                lastActivityTime = millis();
            } else if (input == '#') {
                currentState = VERIFICATION;
                Serial.println("Transition: VERIFICATION");
            }
            break;

        case VERIFICATION:
            bool match = true;
            if (inputBuffer.length() != 4) {
                match = false;
            } else {
                for (int i = 0; i < 4; i++) {
                    if (inputBuffer[i] != raw_eeprom_read(i)) match = false;
                }
            }

            if (match) {
                Serial.println("MATCH FOUND: Transition to UNLOCKED");
                moveServo(1500); // Pulse for Unlocking [cite: 131]
                currentState = UNLOCKED;
            } else {
                Serial.println("ACCESS DENIED: Error State");
                currentState = ERROR;
            }
            inputBuffer = "";
            break;

        case UNLOCKED:
            if (checkForKeypress() == '*') { 
                currentState = LOCKING;
                Serial.println("Lock Triggered: Transition to LOCKING");
            }
            break;

        case LOCKING:
            moveServo(600);
            currentState = ACTIVE;
            lastActivityTime = millis();
            break;

        case ERROR:
            delay(1000);
            currentState = ACTIVE;
            Serial.println("Error Reset: Transition to ACTIVE");
            break;
    }
}

// add stuff 
void moveServo(int pulseWidthUs) {
    
}

// add stuff
char checkForKeypress() {
    
}