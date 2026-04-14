// keypad layouts
const char KEYPAD_KEYS[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};

// debounce state
static char _lastKey = 0;

// reading A0 at every column
int readA0ForColumn(int c) {
    for (int i = 0; i < 3; i++) digitalWrite(COL_PINS[i], HIGH);
    digitalWrite(COL_PINS[c], LOW);
    delayMicroseconds(KEYPAD_SETTLE_US);
    (void)analogRead(KEYPAD_ANALOG_PIN);
    delayMicroseconds(50);
    return analogRead(KEYPAD_ANALOG_PIN);
}

// getting row from the A0 value
int rowFromPressedVal(int pv) {
    if (pv > KEYPAD_CUT_0_1) return 0;
    if (pv > KEYPAD_CUT_1_2) return 1;
    if (pv > KEYPAD_CUT_2_3) return 2;
    if (pv > KEYPAD_ZERO_MAX) return 3;
    return -1;
}

char scanKeyOnce() {
    int v[3];
    for (int c = 0; c < 3; c++) v[c] = readA0ForColumn(c);

    // find index of minimum
    int col = 0;
    int minV = v[0];
    for (int i = 1; i < 3; i++) {
        if (v[i] < minV) { minV = v[i]; col = i; }
    }

    // no key is all zeros
    if (v[0] <= KEYPAD_ZERO_MAX && v[1] <= KEYPAD_ZERO_MAX && v[2] <= KEYPAD_ZERO_MAX) return 0;

    // require that the column is actually near zero
    if (minV > KEYPAD_ZERO_MAX) return 0;

    // pressed value = average of the other two entries
    int a = v[(col + 1) % 3];
    int b = v[(col + 2) % 3];
    int pressedVal = (a + b) / 2;

    int row = rowFromPressedVal(pressedVal);
    if (row == -1) return 0;

    return KEYPAD_KEYS[row][col];
}

char keypad_get_key() {
    char k = scanKeyOnce();
    
    if (k && k != _lastKey) {
        _lastKey = k;
        delay(KEYPAD_DEBOUNCE_MS);
        return k;
    }
    
    _lastKey = k;
    delay(100);
    return '\0';
}

void keypad_init() {
    for (int i = 0; i < 3; i++) {
        pinMode(COL_PINS[i], OUTPUT);
        digitalWrite(COL_PINS[i], HIGH);
    }
    Serial.println("[KEYPAD] Initialized");
    Serial.print("[KEYPAD] Columns on pins: ");
    Serial.print(COL_PINS[0]); Serial.print(", ");
    Serial.print(COL_PINS[1]); Serial.print(", ");
    Serial.println(COL_PINS[2]);
    Serial.print("[KEYPAD] Analog input: A");
    Serial.println(KEYPAD_ANALOG_PIN - A0);
}

void keypad_debug() {
    int readings[3];
    for (int c = 0; c < 3; c++) {
        readings[c] = readA0ForColumn(c);
    }
    Serial.print("[KEYPAD DEBUG] Col0=");
    Serial.print(readings[0]);
    Serial.print(" Col1=");
    Serial.print(readings[1]);
    Serial.print(" Col2=");
    Serial.println(readings[2]);
}
