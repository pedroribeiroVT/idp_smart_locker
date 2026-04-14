#ifndef SMCR
#define SMCR (*(volatile uint8_t *)(0x53))  // sleep mode control register
#endif
#define SE 0   // sleep enable bit
#define SM1 2  // sleep mode bit 1

#define MCUCR (*(volatile uint8_t *)(0x55))  // MCU control register
#define BODS 6                               // brown-out detector sleep
#define BODSE 5                              // brown-out detector sleep enable

#define PRR (*(volatile uint8_t *)(0x64))  // power reduction register
#define PRADC 0                             // ADC power bit
#define PRSPI 2                             // SPI power bit
#define PRTIM1 3                            // Timer1 power bit
#define PRTIM2 6                            // Timer2 power bit
#define PRTWI 7                             // TWI power bit

#define WDTCSR (*(volatile uint8_t *)(0x60))  // watchdog timer control register
#define WDCE 4                                // watchdog change enable
#define WDE 3                                 // watchdog system reset enable
#define WDP3 5                                // watchdog prescaler bit 3

#define CLKPR (*(volatile uint8_t *)(0x61))  // clock prescale register
#define CLKPCE 7                             // clock prescaler change enable
#define CLKPS2 2                             // clock prescaler select bit 2
#define CLKPS1 1                             // clock prescaler select bit 1

#define ADCSRA (*(volatile uint8_t *)(0x7A))  // ADC control and status register A
#define ADEN 7                                // ADC enable bit

#define SREG (*(volatile uint8_t *)(0x5F))  // status register

#define PCICR (*(volatile uint8_t *)(0x68))  // pin change interrupt control
#define PCIE2 2                              // enable PCINT[23:16] (Port D)

#define PCMSK2 (*(volatile uint8_t *)(0x6D))  // pin change mask register 2
#define PCINT19 3                             // pin D3
#define PCINT20 4                             // pin D4
#define PCINT21 5                             // pin D5

// measured currents from AD2 — update after probing
#define MEASURED_SLEEP_CURRENT_uA   25.0f
#define MEASURED_ACTIVE_CURRENT_mA  8.5f
#define MEASURED_SERVO_CURRENT_mA   180.0f
#define BATTERY_CAPACITY_mAh        600.0f

unsigned long _cycle_active_ms = 0;
unsigned long _cycle_servo_ms = 0;

// just needs to exist so the chip wakes instead of resetting
ISR(PCINT2_vect) {}

void enter_deep_sleep() {
  ADCSRA &= ~(1 << ADEN);  // disable ADC

  // set column pins as INPUT_PULLUP so a press can pull them low
  for (int i = 0; i < 3; i++) {
    pinMode(COL_PINS[i], INPUT_PULLUP);
  }

  // set A0 as digital output LOW — gives the row side a path to ground
  pinMode(KEYPAD_ANALOG_PIN, OUTPUT);
  digitalWrite(KEYPAD_ANALOG_PIN, LOW);

  // enable pin-change interrupts on pins 3, 4, 5
  PCMSK2 |= (1 << PCINT19) | (1 << PCINT20) | (1 << PCINT21);
  PCICR |= (1 << PCIE2);

  // entering power down mode
  SMCR = (1 << SM1);
  SMCR |= (1 << SE);

  MCUCR |= (1 << BODS) | (1 << BODSE);
  MCUCR &= ~(1 << BODSE);

  sei();  // interrupts must be enabled for wake
  asm volatile("sleep");

  // post-wakeup, reset everything back
  SMCR &= ~(1 << SE);

  // disable pin-change interrupts
  PCICR &= ~(1 << PCIE2);
  PCMSK2 &= ~((1 << PCINT19) | (1 << PCINT20) | (1 << PCINT21));

  // restore keypad pins to normal scanning mode
  for (int i = 0; i < 3; i++) {
    pinMode(COL_PINS[i], OUTPUT);
    digitalWrite(COL_PINS[i], HIGH);
  }
  pinMode(KEYPAD_ANALOG_PIN, INPUT);  // restore A0 as analog input

  ADCSRA |= (1 << ADEN);  // re-enable ADC
}

void init_watchdog() {
  cli();                // disable interrupts during setup
  asm volatile("wdr");  // reset WDT timer

  // set WDCE and WDE to allow changes
  WDTCSR |= (1 << WDCE) | (1 << WDE);

  // 4 second timeout setup
  WDTCSR = (1 << WDE) | (1 << WDP3);

  sei();  // re-enable interrupts
}

void reset_watchdog() {
  asm volatile("wdr");  // reset WDT timer
}

void set_clock_speed(bool slow) {
  uint8_t saveSREG = SREG;
  cli();  // disable interrupts for atimed sequence

  // enable change
  CLKPR = (1 << CLKPCE);

  if (slow) { CLKPR = (1 << CLKPS2) | (1 << CLKPS1); }  // set to 250kHz
  else {
    CLKPR = 0;
  }  // set to 16MHz

  SREG = saveSREG;  // restores interrupts
}

// shut down unused peripherals and tie floating pins to save current
void power_optimize_init() {
  PRR |= (1 << PRTWI) | (1 << PRSPI) | (1 << PRTIM1) | (1 << PRTIM2);

  // unused pins to INPUT_PULLUP to prevent leakage from floating inputs
  for (uint8_t pin = 7; pin <= 13; pin++) {
    pinMode(pin, INPUT_PULLUP);
  }
  pinMode(A2, INPUT_PULLUP);
  pinMode(A3, INPUT_PULLUP);

  Serial.println("[POWER] Peripherals reduced");
}

void adc_disable() {
  ADCSRA &= ~(1 << ADEN);
  PRR |= (1 << PRADC);
}

void adc_enable() {
  PRR &= ~(1 << PRADC);
  ADCSRA |= (1 << ADEN);
}

void cycle_timer_reset() {
  _cycle_active_ms = 0;
  _cycle_servo_ms = 0;
}

void cycle_mark_active(unsigned long ms) { _cycle_active_ms += ms; }
void cycle_mark_servo(unsigned long ms) { _cycle_servo_ms += ms; }

// mAh consumed by one full active cycle from measured currents and timed phases
float cycle_energy_mAh() {
  float active_h = _cycle_active_ms / 3600000.0f;
  float servo_h = _cycle_servo_ms / 3600000.0f;
  return (MEASURED_ACTIVE_CURRENT_mA * active_h) + (MEASURED_SERVO_CURRENT_mA * servo_h);
}

void battery_life_report(float cycles_per_day) {
  float sleep_mA   = MEASURED_SLEEP_CURRENT_uA / 1000.0f;
  float per_cycle_mAh = cycle_energy_mAh();

  Serial.println(F("[POWER] Last-cycle battery report "));
  Serial.print(F("[POWER] Active time : "));
  Serial.print(_cycle_active_ms);
  Serial.println(F(" ms"));
  Serial.print(F("[POWER] Servo time  : "));
  Serial.print(_cycle_servo_ms);
  Serial.println(F(" ms"));

  if (per_cycle_mAh <= 0.0f) {
    Serial.println(F("[POWER] No energy data for this cycle (no active/servo time recorded)."));
    return;
  }

  Serial.print(F("[POWER] Energy/cycle: ")); Serial.print(per_cycle_mAh * 1000.0f, 2); Serial.println(F(" uAh"));

  float idle_days  = BATTERY_CAPACITY_mAh / (sleep_mA * 24.0f);
  float max_cycles = BATTERY_CAPACITY_mAh / per_cycle_mAh;

  // daily mAh = sleep all day + N active cycles
  float daily_mAh  = (sleep_mA * 24.0f) + (per_cycle_mAh * cycles_per_day);
  float real_days  = BATTERY_CAPACITY_mAh / daily_mAh;
  float real_years = real_days / 365.0f;

  Serial.print(F("[POWER] Max cycles on full charge : ")); Serial.println((long)max_cycles);
  Serial.print(F("[POWER] Idle-only battery life    : ")); Serial.print(idle_days, 1);  Serial.println(F(" days"));
  Serial.print(F("[POWER] At ")); Serial.print(cycles_per_day, 0);
  Serial.print(F(" cycles/day -> ")); Serial.print(real_days, 1);
  Serial.print(F(" days (")); Serial.print(real_years, 2); Serial.println(F(" years)"));
  Serial.println(F("[POWER] -----------------------------------------"));
}