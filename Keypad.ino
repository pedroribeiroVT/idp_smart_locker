const int colPins[3] = {5, 4, 3};
const int analogPin  = A0;

char keys[4][3] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

const int SETTLE_US    = 800;
const int DEBOUNCE_MS  = 150;

// this is the deactivated
const int ZERO_MAX = 10;

const int CUT_0_1 = 700;
const int CUT_1_2 = 410;
const int CUT_2_3 = 290; 


// reading A0 at every column
int readA0ForColumn(int c) {
  for (int i = 0; i < 3; i++) digitalWrite(colPins[i], HIGH);
  digitalWrite(colPins[c], LOW);

  delayMicroseconds(SETTLE_US);
  (void)analogRead(analogPin);
  delayMicroseconds(50);

  return analogRead(analogPin);
}


// getting row from the A0 value
int rowFromPressedVal(int pv) {
  if (pv > CUT_0_1) return 0; 
  if (pv > CUT_1_2) return 1;
  if (pv > CUT_2_3) return 2;
  if (pv > ZERO_MAX) return 3;
  return -1;
}

// return 0 if no key
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
  if (v[0] <= ZERO_MAX && v[1] <= ZERO_MAX && v[2] <= ZERO_MAX) return 0;

  // require that the column is actually near zero
  if (minV > ZERO_MAX) return 0;

  // pressed value = average of the other two entries
  int a = v[(col + 1) % 3];
  int b = v[(col + 2) % 3];
  int pressedVal = (a + b) / 2;

  int row = rowFromPressedVal(pressedVal);
  if (row == -1) return 0;

  return keys[row][col];
}

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < 3; i++) {
    pinMode(colPins[i], OUTPUT);
    digitalWrite(colPins[i], HIGH);
  }
}

void loop() {
  static char last = 0;
  char k = scanKeyOnce();

  if (k && k != last) {
    Serial.println(k);
    delay(DEBOUNCE_MS);
  }

  last = k;
  delay(20);
}