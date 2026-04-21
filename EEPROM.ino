// hardware registers for EEPROM
#define REG_EECR  *((volatile unsigned char*)0x3F)
#define REG_EEDR  *((volatile unsigned char*)0x40)
#define REG_EEARL *((volatile unsigned char*)0x41)
#define REG_SREG  *((volatile unsigned char*)0x5F)

#define BIT_EERE  0 // EEPROM Read Enable
#define BIT_EEPE  1 // EEPROM Program Enable
#define BIT_EEMPE 2 // EEPROM Master Program Enable

const uint8_t PASSWORD_RECORD_VERSION = 0xA5;
const unsigned int PASSWORD_VERSION_ADDR = PASSWORD_START_ADDR;
const unsigned int PASSWORD_SEED_ADDR = PASSWORD_START_ADDR + 1;
const unsigned int PASSWORD_HASH_ADDR = PASSWORD_START_ADDR + 5;
const unsigned int PASSWORD_RECORD_SIZE = 9;

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
    unsigned char sreg_backup = REG_SREG;
    REG_SREG &= ~(1 << 7);
    
    REG_EECR |= (1 << BIT_EEMPE);
    REG_EECR |= (1 << BIT_EEPE);
    REG_SREG = sreg_backup;
    
    Serial.print("[EEPROM] Write 0x");
    Serial.print(data, HEX);
    Serial.print(" to address ");
    Serial.println(address);
}

void eeprom_write_u32(unsigned int address, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        raw_eeprom_write(address + i, (uint8_t)(value >> (8 * i)));
    }
}

uint32_t eeprom_read_u32(unsigned int address) {
    uint32_t value = 0;

    for (int i = 0; i < 4; i++) {
        value |= ((uint32_t)raw_eeprom_read(address + i)) << (8 * i);
    }

    return value;
}

uint32_t getStoredPasswordHash() {
    return eeprom_read_u32(PASSWORD_HASH_ADDR);
}

void printStoredPasswordHash() {
    uint32_t hash = getStoredPasswordHash();

    Serial.print("[HASH] Stored password hash: 0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        Serial.print((hash >> shift) & 0x0FUL, HEX);
    }
    Serial.println();
}

bool isPasswordSet() {
    return raw_eeprom_read(PASSWORD_VERSION_ADDR) == PASSWORD_RECORD_VERSION;
}

void clearPassword() {
    for (unsigned int i = 0; i < PASSWORD_RECORD_SIZE; i++) {
        raw_eeprom_write(PASSWORD_START_ADDR + i, 0xFF);
    }
    Serial.println("[EEPROM] Password record cleared");
}

void initializeDefaultPassword() {
    savePassword("1234");
    Serial.println("[EEPROM] Default password initialized to 1234");
}

void savePassword(const char* password) {
    int length = 0;
    while (password[length] != '\0' && length < (PASSWORD_LENGTH + 1)) {
        length++;
    }

    if (length != PASSWORD_LENGTH || password[length] != '\0') {
        Serial.println("[EEPROM] Refusing to save invalid password length");
        return;
    }

    uint32_t seed = generatePasswordSeed();
    uint32_t hash = hashPassword(password, seed);

    raw_eeprom_write(PASSWORD_VERSION_ADDR, 0xFF);
    eeprom_write_u32(PASSWORD_SEED_ADDR, seed);
    eeprom_write_u32(PASSWORD_HASH_ADDR, hash);
    raw_eeprom_write(PASSWORD_VERSION_ADDR, PASSWORD_RECORD_VERSION);

    Serial.println("[EEPROM] Hashed password and seed saved to EEPROM");
}

bool verifyPassword(const String& input) {
    Serial.print("[EEPROM] Verifying password (length ");
    Serial.print(input.length());
    Serial.println(")");

    if (!isPasswordSet()) {
        Serial.println("[EEPROM] No valid password record found");
        return false;
    }
    
    if (input.length() != PASSWORD_LENGTH) {
        Serial.println("[EEPROM] Length mismatch!");
        return false;
    }

    uint32_t seed = eeprom_read_u32(PASSWORD_SEED_ADDR);
    uint32_t storedHash = eeprom_read_u32(PASSWORD_HASH_ADDR);
    uint32_t candidateHash = hashPasswordString(input, seed);

    if (candidateHash != storedHash) {
        Serial.println("[EEPROM] Hash mismatch!");
        return false;
    }

    Serial.println("[EEPROM] Password hash match!");
    return true;
}
