// hardware registers for EEPROM
#define REG_EECR  *((volatile unsigned char*)0x3F)
#define REG_EEDR  *((volatile unsigned char*)0x40)
#define REG_EEARL *((volatile unsigned char*)0x41)
#define REG_SREG  *((volatile unsigned char*)0x5F)

#define BIT_EERE  0 // EEPROM Read Enable
#define BIT_EEPE  1 // EEPROM Program Enable
#define BIT_EEMPE 2 // EEPROM Master Program Enable

unsigned char raw_eeprom_read(unsigned int address) {
    while (REG_EECR & (1 << BIT_EEPE)); // wait for previous write
    REG_EEARL = (unsigned char)address;
    REG_EECR |= (1 << BIT_EERE); // trigger read
    return REG_EEDR;
}

void raw_eeprom_write(unsigned int address, unsigned char data) {
    // optimization - skip write if data already matches
    if (raw_eeprom_read(address) == data) {
        Serial.print("[EEPROM] Skip write at ");
        Serial.print(address);
        Serial.println(" - value unchanged");
        return;
    }

    while (REG_EECR & (1 << BIT_EEPE));  // wait for previous write
    REG_EEARL = (unsigned char)address;
    REG_EEDR = data;
    
    // disable interrupts for 4-cycle write window
    // FIX(4): use direct assignment (=) not read-modify-write (|=).
    // |= compiles to 3+ instructions per line; the ATmega328P datasheet
    // (section 8.5.2) requires EEPE to be set within 4 clock cycles of
    // EEMPE. Writing both bits together in a single store satisfies that.
    unsigned char sreg_backup = REG_SREG;
    REG_SREG &= ~(1 << 7);

    REG_EECR = (1 << BIT_EEMPE);
    REG_EECR = (1 << BIT_EEMPE) | (1 << BIT_EEPE);
    REG_SREG = sreg_backup;
    
    Serial.print("[EEPROM] Write 0x");
    Serial.print(data, HEX);
    Serial.print(" to address ");
    Serial.println(address);
}

bool isPasswordSet() {
    return raw_eeprom_read(PASSWORD_START_ADDR) != 0xFF;
}

// write 0xFF to first address to mark as no password
void clearPassword() {
    raw_eeprom_write(PASSWORD_START_ADDR, 0xFF);
    Serial.println("[EEPROM] Password cleared!");
}

void savePassword(const char* password) {
    for (int i = 0; i < PASSWORD_LENGTH; i++) {
        raw_eeprom_write(PASSWORD_START_ADDR + i, password[i]);
    }
    Serial.println("[EEPROM] Password saved to non-volatile memory");
}

bool verifyPassword(const String& input) {
    Serial.print("[EEPROM] Verifying password (length ");
    Serial.print(input.length());
    Serial.println(")");
    
    if (input.length() != PASSWORD_LENGTH) {
        Serial.println("[EEPROM] Length mismatch!");
        return false;
    }
    for (int i = 0; i < PASSWORD_LENGTH; i++) {
        char stored = raw_eeprom_read(PASSWORD_START_ADDR + i);
        if (input[i] != stored) {
            Serial.print("[EEPROM] Mismatch at position ");
            Serial.println(i);
            return false;
        }
    }
    Serial.println("[EEPROM] Password match!");
    return true;
}
