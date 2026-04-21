// power_measure.h — Milestone 4d: current measurement + battery life
//
// HARDWARE: Servo GND ── wire ── A3 ── 1Ω resistor ── system GND
//
// Servo current:  MEASURED via 1Ω shunt on A3
// ATmega active:  12 mA  (datasheet Table 33-1, 16MHz/5V)
// LED current:    3 mA   (Ohm's law: (5V-2V)/1kΩ)
// Sleep current:  8.2 µA (datasheet quiescent sum)

#define SERVO_SENSE_PIN    A3
#define SHUNT_RESISTANCE   1.0f
#define ADC_TO_MV          (5000.0f / 1023.0f)

#define ATMEGA_ACTIVE_MA   12.0f
#define LED_ON_MA          3.0f
#define SLEEP_CURRENT_UA   8.2f
#define SERVO_DATASHEET_MA 150.0f  // SG90 typical running current

#define BATTERY_CAPACITY_MAH  600.0f
#define CYCLES_PER_DAY        5.0f

static unsigned long phase_active_ms = 0;
static unsigned long phase_servo_ms  = 0;
static unsigned long phase_led_ms    = 0;
static unsigned long phase_sleep_ms  = 0;

static float         servo_mA_sum    = 0.0f;
static unsigned long servo_samples   = 0;

float read_servo_current_mA() {
  int raw = analogRead(SERVO_SENSE_PIN);
  return (raw * ADC_TO_MV) / SHUNT_RESISTANCE;
}

void measure_servo_sample(unsigned long delta_ms) {
  servo_mA_sum += read_servo_current_mA();
  servo_samples++;
  phase_servo_ms += delta_ms;
}

void measure_mark_active(unsigned long ms) { phase_active_ms += ms; }
void measure_mark_led(unsigned long ms)    { phase_led_ms    += ms; }
void measure_mark_sleep(unsigned long ms)  { phase_sleep_ms  += ms; }

float avg_servo_mA() {
  if (servo_samples == 0) return 0.0f;
  return servo_mA_sum / servo_samples;
}

void measure_reset() {
  phase_active_ms = 0;
  phase_servo_ms  = 0;
  phase_led_ms    = 0;
  phase_sleep_ms  = 0;
  servo_mA_sum    = 0.0f;
  servo_samples   = 0;
}

float cycle_energy_mAh() {
  float active = (ATMEGA_ACTIVE_MA * phase_active_ms) / 3600000.0f;
  float servo  = (avg_servo_mA()   * phase_servo_ms)  / 3600000.0f;
  float led    = (LED_ON_MA        * phase_led_ms)    / 3600000.0f;
  return active + servo + led;
}

float sleep_energy_mAh() {
  return (SLEEP_CURRENT_UA / 1000.0f * phase_sleep_ms) / 3600000.0f;
}

// REPORT 1: after waking
void report_after_wake() {
  Serial.println(F("\n[POWER] === SLEEP REPORT ==="));
  Serial.println(F("[POWER] ATmega328P power-down:  0.1 uA (datasheet T33-2)"));
  Serial.println(F("[POWER] MCP1702 quiescent:      2.0 uA (MCP1702 datasheet)"));
  Serial.println(F("[POWER] Voltage divider:        6.1 uA (9V / 1.47M)"));
  Serial.print(F("[POWER] TOTAL SLEEP CURRENT:    "));
  Serial.print(SLEEP_CURRENT_UA, 1);
  Serial.println(F(" uA"));
  Serial.print(F("[POWER] Slept for: "));
  Serial.print(phase_sleep_ms / 1000);
  Serial.println(F(" seconds"));
  Serial.print(F("[POWER] Sleep energy: "));
  Serial.print(sleep_energy_mAh() * 1000.0f, 2);
  Serial.println(F(" uAh"));
}

// REPORT 2: after unlocking
void report_after_unlock() {
  Serial.println(F("\n[POWER] === UNLOCK REPORT ==="));
  Serial.print(F("[POWER] Active (keypad): "));
  Serial.print(phase_active_ms);
  Serial.print(F(" ms x "));
  Serial.print(ATMEGA_ACTIVE_MA, 1);
  Serial.println(F(" mA (datasheet T33-1)"));
  Serial.print(F("[POWER] LED on: "));
  Serial.print(phase_led_ms);
  Serial.print(F(" ms x "));
  Serial.print(LED_ON_MA, 1);
  Serial.println(F(" mA (Ohm's law)"));
  Serial.print(F("[POWER] Servo unlock: "));
  Serial.print(phase_servo_ms);
  Serial.print(F(" ms x "));
  Serial.print(avg_servo_mA(), 1);
  Serial.println(F(" mA (MEASURED A3 shunt)"));
  Serial.print(F("[POWER] Energy so far: "));
  Serial.print(cycle_energy_mAh() * 1000.0f, 2);
  Serial.println(F(" uAh"));
}

// REPORT 3: after locking — full cycle + battery life
void report_after_lock() {
  float per_cycle = cycle_energy_mAh();
  float sleep_mA  = SLEEP_CURRENT_UA / 1000.0f;

  Serial.println(F("\n[POWER] === FULL CYCLE REPORT ==="));
  Serial.print(F("[POWER] Total active:  "));
  Serial.print(phase_active_ms);
  Serial.println(F(" ms"));
  Serial.print(F("[POWER] Total servo:   "));
  Serial.print(phase_servo_ms);
  Serial.println(F(" ms"));
  Serial.print(F("[POWER] Total LED:     "));
  Serial.print(phase_led_ms);
  Serial.println(F(" ms"));
  Serial.print(F("[POWER] Servo avg current: "));
  Serial.print(avg_servo_mA(), 1);
  Serial.println(F(" mA (MEASURED)"));
  Serial.print(F("[POWER] Energy/cycle:  "));
  Serial.print(per_cycle * 1000.0f, 2);
  Serial.println(F(" uAh"));

  Serial.println(F("\n[POWER] --- BATTERY LIFE ---"));

  float idle_days = BATTERY_CAPACITY_MAH / (sleep_mA * 24.0f);
  Serial.print(F("[POWER] Sleep-only: "));
  Serial.print(idle_days, 1);
  Serial.print(F(" days ("));
  Serial.print(idle_days / 365.0f, 2);
  Serial.println(F(" years)"));

  if (per_cycle > 0.0f) {
    float max_cycles = BATTERY_CAPACITY_MAH / per_cycle;
    Serial.print(F("[POWER] Max cycles: "));
    Serial.println((long)max_cycles);

    float daily_mAh = (sleep_mA * 24.0f) + (per_cycle * CYCLES_PER_DAY);
    float real_days = BATTERY_CAPACITY_MAH / daily_mAh;
    Serial.print(F("[POWER] At "));
    Serial.print(CYCLES_PER_DAY, 0);
    Serial.print(F(" cycles/day: "));
    Serial.print(real_days, 1);
    Serial.print(F(" days ("));
    Serial.print(real_days / 365.0f, 2);
    Serial.println(F(" years)"));
  }
  Serial.println(F("[POWER] =========================="));
}

// ── HARDCODED REPORTS (datasheet values only, no A3 measurement) ──

void report_after_wake_hardcoded() {
  float sleep_mA = SLEEP_CURRENT_UA / 1000.0f;
  float sleep_uAh = (sleep_mA * phase_sleep_ms) / 3600.0f;

  Serial.println(F("\n[POWER-HC] === SLEEP REPORT (DATASHEET) ==="));
  Serial.println(F("[POWER-HC] ATmega328P power-down:  0.1 uA (datasheet T33-2)"));
  Serial.println(F("[POWER-HC] MCP1702 quiescent:      2.0 uA (MCP1702 datasheet)"));
  Serial.println(F("[POWER-HC] Voltage divider:        6.1 uA (9V / 1.47M)"));
  Serial.print(F("[POWER-HC] TOTAL SLEEP CURRENT:    "));
  Serial.print(SLEEP_CURRENT_UA, 1);
  Serial.println(F(" uA"));
  Serial.print(F("[POWER-HC] Slept for: "));
  Serial.print(phase_sleep_ms / 1000);
  Serial.println(F(" seconds"));
  Serial.print(F("[POWER-HC] Sleep energy: "));
  Serial.print(sleep_uAh, 2);
  Serial.println(F(" uAh"));
}

void report_after_unlock_hardcoded() {
  float active_uAh = (ATMEGA_ACTIVE_MA * phase_active_ms) / 3600.0f;
  float led_uAh    = (LED_ON_MA * phase_led_ms) / 3600.0f;
  float servo_uAh  = (SERVO_DATASHEET_MA * phase_servo_ms) / 3600.0f;
  float total_uAh  = active_uAh + led_uAh + servo_uAh;

  Serial.println(F("\n[POWER-HC] === UNLOCK REPORT (DATASHEET) ==="));
  Serial.print(F("[POWER-HC] Active (keypad): "));
  Serial.print(phase_active_ms);
  Serial.print(F(" ms x "));
  Serial.print(ATMEGA_ACTIVE_MA, 1);
  Serial.println(F(" mA (datasheet T33-1)"));
  Serial.print(F("[POWER-HC] LED on: "));
  Serial.print(phase_led_ms);
  Serial.print(F(" ms x "));
  Serial.print(LED_ON_MA, 1);
  Serial.println(F(" mA (Ohm's law)"));
  Serial.print(F("[POWER-HC] Servo unlock: "));
  Serial.print(phase_servo_ms);
  Serial.print(F(" ms x "));
  Serial.print(SERVO_DATASHEET_MA, 1);
  Serial.println(F(" mA (SG90 datasheet)"));
  Serial.print(F("[POWER-HC] Energy so far: "));
  Serial.print(total_uAh, 2);
  Serial.println(F(" uAh"));
}

void report_after_lock_hardcoded() {
  float sleep_mA  = SLEEP_CURRENT_UA / 1000.0f;
  float active_mAh = (ATMEGA_ACTIVE_MA * phase_active_ms) / 3600000.0f;
  float servo_mAh  = (SERVO_DATASHEET_MA * phase_servo_ms) / 3600000.0f;
  float led_mAh    = (LED_ON_MA * phase_led_ms) / 3600000.0f;
  float per_cycle   = active_mAh + servo_mAh + led_mAh;

  Serial.println(F("\n[POWER-HC] === FULL CYCLE REPORT (DATASHEET) ==="));
  Serial.print(F("[POWER-HC] Total active:  "));
  Serial.print(phase_active_ms);
  Serial.print(F(" ms x 12.0 mA"));
  Serial.println(F(" (datasheet T33-1)"));
  Serial.print(F("[POWER-HC] Total servo:   "));
  Serial.print(phase_servo_ms);
  Serial.print(F(" ms x 150.0 mA"));
  Serial.println(F(" (SG90 datasheet)"));
  Serial.print(F("[POWER-HC] Total LED:     "));
  Serial.print(phase_led_ms);
  Serial.print(F(" ms x 3.0 mA"));
  Serial.println(F(" (Ohm's law)"));
  Serial.print(F("[POWER-HC] Energy/cycle:  "));
  Serial.print(per_cycle * 1000.0f, 2);
  Serial.println(F(" uAh"));

  Serial.println(F("\n[POWER-HC] --- BATTERY LIFE ---"));

  float idle_days = BATTERY_CAPACITY_MAH / (sleep_mA * 24.0f);
  Serial.print(F("[POWER-HC] Sleep-only: "));
  Serial.print(idle_days, 1);
  Serial.print(F(" days ("));
  Serial.print(idle_days / 365.0f, 2);
  Serial.println(F(" years)"));

  if (per_cycle > 0.0f) {
    float max_cycles = BATTERY_CAPACITY_MAH / per_cycle;
    Serial.print(F("[POWER-HC] Max cycles: "));
    Serial.println((long)max_cycles);

    float daily_mAh = (sleep_mA * 24.0f) + (per_cycle * CYCLES_PER_DAY);
    float real_days = BATTERY_CAPACITY_MAH / daily_mAh;
    Serial.print(F("[POWER-HC] At "));
    Serial.print(CYCLES_PER_DAY, 0);
    Serial.print(F(" cycles/day: "));
    Serial.print(real_days, 1);
    Serial.print(F(" days ("));
    Serial.print(real_days / 365.0f, 2);
    Serial.println(F(" years)"));
  }
  Serial.println(F("[POWER-HC] =============================="));
}