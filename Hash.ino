const int HASH_SEED_PIN = A1;
const uint32_t HASH_FALLBACK_STATE = 0x6D2B79F5UL;

uint32_t xorshift32_next(uint32_t &state) {
    if (state == 0) {
        state = HASH_FALLBACK_STATE;
    }

    state ^= (state << 13);
    state ^= (state >> 17);
    state ^= (state << 5);
    return state;
}

uint32_t xorshift32_mix(uint32_t value) {
    uint32_t state = value;
    return xorshift32_next(state);
}

uint32_t generatePasswordSeed() {
    pinMode(HASH_SEED_PIN, INPUT);

    uint32_t seed = 0xA5C39E7DUL;
    for (int i = 0; i < 32; i++) {
        uint32_t reading = (uint32_t)analogRead(HASH_SEED_PIN);
        seed ^= reading << (i % 16);
        seed ^= (reading * (uint32_t)(i + 1)) << (i % 7);
        seed ^= ((uint32_t)micros()) << (i % 5);
        seed = xorshift32_mix(seed);
        delay(1);
    }

    if (seed == 0 || seed == 0xFFFFFFFFUL) {
        seed = HASH_FALLBACK_STATE ^ (uint32_t)analogRead(HASH_SEED_PIN);
    }

    Serial.println("[HASH] Random seed generated from A1");
    return seed;
}

uint32_t hashPasswordBuffer(const char *password, int length, uint32_t seed) {
    uint32_t prngState = seed;
    uint32_t hash = 0x811C9DC5UL ^ seed;

    for (int i = 0; i < length; i++) {
        uint32_t stream = xorshift32_next(prngState);
        uint8_t mixedByte = ((uint8_t)password[i]) ^ (uint8_t)(stream & 0xFF);

        hash ^= (uint32_t)mixedByte;
        hash *= 16777619UL;
        hash ^= (stream >> ((i % 4) * 8));
        hash = xorshift32_mix(hash ^ ((uint32_t)i * 0x9E3779B9UL));
    }

    hash ^= ((uint32_t)length * 0x85EBCA6BUL);
    hash ^= seed;
    hash = xorshift32_mix(hash);

    if (hash == 0 || hash == 0xFFFFFFFFUL) {
        hash ^= 0xA5A5A5A5UL;
    }

    return hash;
}

uint32_t hashPassword(const char *password, uint32_t seed) {
    return hashPasswordBuffer(password, PASSWORD_LENGTH, seed);
}

uint32_t hashPasswordString(const String &input, uint32_t seed) {
    char buffer[PASSWORD_LENGTH];

    for (int i = 0; i < PASSWORD_LENGTH; i++) {
        buffer[i] = input[i];
    }

    return hashPasswordBuffer(buffer, PASSWORD_LENGTH, seed);
}
