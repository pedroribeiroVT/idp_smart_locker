#ifndef SMCR
#define SMCR   (*(volatile uint8_t *)(0x53)) // sleep mode control register
#endif
#define SE     0                             // sleep enable bit
#define SM1    2                             // sleep mode bit 1

#define MCUCR  (*(volatile uint8_t *)(0x55)) // MCU control register
#define BODS   6                             // brown-out detector sleep
#define BODSE  5                             // brown-out detector sleep enable

#ifndef MCUSR
#define MCUSR  (*(volatile uint8_t *)(0x54)) // MCU status register
#endif
#define WDRF   3                             // watchdog reset flag

#define PRR    (*(volatile uint8_t *)(0x64)) // power reduction register

#define WDTCSR (*(volatile uint8_t *)(0x60)) // watchdog timer control register
#define WDIF   7                             // watchdog interrupt flag
#define WDIE   6                             // watchdog interrupt enable
#define WDCE   4                             // watchdog change enable
#define WDE    3                             // watchdog system reset enable
#define WDP3   5                             // watchdog prescaler bit 3

#define CLKPR  (*(volatile uint8_t *)(0x61)) // clock prescale register
#define CLKPCE 7                             // clock prescaler change enable
#define CLKPS2 2                             // clock prescaler select bit 2
#define CLKPS1 1                             // clock prescaler select bit 1

#define ADCSRA (*(volatile uint8_t *)(0x7A)) // ADC control and status register A
#define ADEN   7                             // ADC enable bit

#define SREG   (*(volatile uint8_t *)(0x5F)) // status register

#define BATT_SENSE_PIN A2
#define BATT_LED_PIN 2
 
// R1 = 470k / R2 = 1470k = 0.3197
// V_pin = V_batt × 0.3197, so V_batt = V_pin / 0.3197
#define DIVIDER_RATIO    0.3197f
#define ADC_REF_VOLTAGE  5.0f
 
#define BATT_LOW_VOLTAGE    6.5f
#define BATT_CRIT_VOLTAGE   5.8f
 
#define BATT_LED_FLASH_MS      100
#define BATT_LED_FLASHES       3
#define BATT_LED_CRIT_FLASHES  6
 
static float _last_batt_voltage = 0.0f;

volatile bool watchdogWakeTriggered = false;

ISR(WDT_vect) {
    watchdogWakeTriggered = true;
}

void enter_deep_sleep() {
    watchdogWakeTriggered = false;
    ADCSRA &= ~(1 << ADEN); // disable ADC to save power

    // configure sleep mode 
    SMCR = (1 << SM1); 
    SMCR |= (1 << SE);

    // disable brown-out detection
    MCUCR |= (1 << BODS) | (1 << BODSE);
    MCUCR &= ~(1 << BODSE);

    asm volatile("sleep"); // assembly command to sleep 

    // wake up 
    SMCR &= ~(1 << SE);
    ADCSRA |= (1 << ADEN);
}

void init_watchdog() {
    cli(); // disable interrupts during setup
    MCUSR &= ~(1 << WDRF);
    asm volatile("wdr"); // reset WDT timer

    // set WDCE and WDE to allow changes
    WDTCSR = (1 << WDCE) | (1 << WDE);

    // interrupt mode only: wake from sleep without rebooting the MCU
    WDTCSR = (1 << WDIE) | (1 << WDP3);
    
    sei(); // re-enable interrups
}

void reset_watchdog() {
    asm volatile("wdr"); // reset WDT timer
}

void set_clock_speed(bool slow) {
    uint8_t saveSREG = SREG;
    cli(); // disable interrupts for timed sequence

    // enable change
    CLKPR = (1 << CLKPCE);

    if (slow) { CLKPR = (1 << CLKPS2) | (1 << CLKPS1); } // set to 64MkHz
    else { CLKPR = 0; } // set to 250MHz

    SREG = saveSREG; // restores interrupts
}

// battery_monitor.h — milestone 4a: battery level check + low-battery alert
// Hardware: 1MΩ (batt+ to A2), 470kΩ (A2 to GND), 100nF cap (A2 to GND),
//           LED + 1kΩ on D8.
 
void battery_monitor_init() {
  pinMode(BATT_LED_PIN, OUTPUT);
  digitalWrite(BATT_LED_PIN, LOW);
}
 
// Average 4 ADC samples, convert to battery voltage via divider ratio.
// Costs ~50 µs total — called once per wake cycle.
float battery_read_voltage() {
  long sum = 0;
  for (int i = 0; i < 4; i++) {
    sum += analogRead(BATT_SENSE_PIN);
    delayMicroseconds(200);
  }
  float pin_v = ((float)sum / 4.0f / 1023.0f) * ADC_REF_VOLTAGE;
  _last_batt_voltage = pin_v / DIVIDER_RATIO;
  return _last_batt_voltage;
}
 
void battery_led_flash(int flashes) {
  for (int i = 0; i < flashes; i++) {
    digitalWrite(BATT_LED_PIN, HIGH);
    delay(BATT_LED_FLASH_MS);
    digitalWrite(BATT_LED_PIN, LOW);
    if (i < flashes - 1) delay(BATT_LED_FLASH_MS);
  }
}

uint8_t battery_check_and_alert() {
  float v = battery_read_voltage();
 
  Serial.print(("[BATT] Voltage: "));
  Serial.print(v, 2);
  Serial.println((" V"));
 
  if (v <= BATT_CRIT_VOLTAGE) {
    Serial.println(F("[BATT] *** CRITICAL — replace battery now ***"));
    battery_led_flash(BATT_LED_CRIT_FLASHES);
    return 2;
  }
  if (v <= BATT_LOW_VOLTAGE) {
    Serial.println(F("[BATT] * LOW — replace battery soon *"));
    battery_led_flash(BATT_LED_FLASHES);
    return 1;
  }
  return 0;
}
 
float battery_last_voltage() {
  return _last_batt_voltage;
}

