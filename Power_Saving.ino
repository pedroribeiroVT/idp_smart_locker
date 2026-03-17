#ifndef SMCR
#define SMCR   (*(volatile uint8_t *)(0x53)) // sleep mode control register
#endif
#define SE     0                             // sleep enable bit
#define SM1    2                             // sleep mode bit 1

#define MCUCR  (*(volatile uint8_t *)(0x55)) // MCU control register
#define BODS   6                             // brown-out detector sleep
#define BODSE  5                             // brown-out detector sleep enable

#define PRR    (*(volatile uint8_t *)(0x64)) // power reduction register

#define WDTCSR (*(volatile uint8_t *)(0x60)) // watchdog timer control register
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

void enter_deep_sleep() {
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
    asm volatile("wdr"); // reset WDT timer

    // set WDCE and WDE to allow changes
    WDTCSR |= (1 << WDCE) | (1 << WDE);

    // 4 second timeout setup
    WDTCSR = (1 << WDE) | (1 << WDP3);
    
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