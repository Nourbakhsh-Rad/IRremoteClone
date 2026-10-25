/*
  IR remote - ATtiny44A (ATTinyCore), 3x7 key matrix, Power-down sleep
  Multi-protocol version: the protocol is selected in Config.h (PROTO), see IRProtocols.h

  Settings, pin map, Arduino IDE options and fuses: see Config.h
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/pgmspace.h>
#include <util/delay_basic.h>

#include "Config.h"
#include "IRProtocols.h"        // carrier engine + the selected protocol encoder


// Gap that keeps the start-to-start period at IR_PERIOD_MS (frame length can depend on the data)
static inline uint16_t periodGap(uint16_t frameMs) {
  return (IR_PERIOD_MS > frameMs + 5) ? (uint16_t)(IR_PERIOD_MS - frameMs) : 5;
}

// ---- matrix row control (PORT bits of the rows are always 0) ----
static inline void rowsHiZ() {           // all rows: input, no pull-up
  DDRA  &= ~ROWA_MASK;  PORTA &= ~ROWA_MASK;
  DDRB  &= ~ROWB_MASK;  PORTB &= ~ROWB_MASK;
}

static inline void rowsLow() {           // all rows: output LOW
  PORTA &= ~ROWA_MASK;  PORTB &= ~ROWB_MASK;
  DDRA  |= ROWA_MASK;   DDRB  |= ROWB_MASK;
}

static inline void rowLow(uint8_t r) {   // drive one row LOW
  if (r < 5) DDRA |= (1 << (PA3 + r));
  else       DDRB |= (1 << (PB0 + (r - 5)));
}

// Returns key index 0..20, or -1 if no key is pressed.
static int8_t scanKey() {
  // Quick check: all rows LOW -> if no column is pulled LOW, nothing is pressed (1 read instead of 7)
  rowsLow();
  delayUs(10);
  if ((PINA & COL_MASK) == COL_MASK) {
    rowsHiZ();
    return -1;
  }

  // Full scan: one row at a time
  rowsHiZ();
  for (uint8_t r = 0; r < N_ROWS; r++) {
    rowLow(r);
    delayUs(10);
    uint8_t c = (~PINA) & COL_MASK;      // pressed = column pulled LOW
    rowsHiZ();
    if (c) {
      for (uint8_t col = 0; col < N_COLS; col++)
        if (c & (1 << col)) return (int8_t)(r * N_COLS + col);
    }
  }
  return -1;
}

ISR(PCINT0_vect) { /* wake-up only */ }

// Enter Power-down; wake on any column change
// skipIfPressed = true: don't sleep if a key is already pressed (avoids missing a press)
static void goSleep(bool skipIfPressed) {
  rowsLow();                              // rows LOW
  DDRA  &= ~COL_MASK;                     // columns input with pull-up
  PORTA |= COL_MASK;
  delayUs(20);

  PCMSK0 = COL_MASK;                      // all columns are on port A -> one interrupt
  GIFR   = (1 << PCIF0);
  GIMSK |= (1 << PCIE0);

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();
  if (!skipIfPressed || ((PINA & COL_MASK) == COL_MASK)) {
    sleep_enable();
    sei();
    sleep_cpu();
    sleep_disable();
  } else {
    sei();
  }
  GIMSK &= ~(1 << PCIE0);
}

static void handleKey(uint8_t key) {
  IR_NEW_PRESS();                                   // e.g. RC5 toggle bit
  uint16_t ms = irSendFrame(key);
  delayMs(periodGap(ms));

  for (uint16_t r = 0; r < MAX_REPEATS; r++) {
    // stop when the key is released, but only after the minimum number of frames
    if (scanKey() != (int8_t)key && (uint16_t)(r + 1) >= IR_MIN_FRAMES) break;
    ms = irSendRepeat(key);
    delayMs(periodGap(ms));
  }

  // wait for release without burning power (sleep wakes on release)
  while (scanKey() >= 0) {
    goSleep(false);
    delayMs(DEBOUNCE_MS);
  }
}

// Force a known system clock regardless of the CKDIV8 fuse (internal 8 MHz RC is assumed: CKSEL = 0010)
static void setClock() {
  uint8_t sreg = SREG;
  cli();
  CLKPR = (1 << CLKPCE);          // unlock, then write the divider within 4 cycles
  CLKPR = CLK_DIV_BITS;
  SREG = sreg;
}

// Hardware bring-up: blink the IR LED output (PB2) forever
void testHardware() {
	#if (HARDWARE_TEST ==1)
		DDRB  |= (1 << IR_PIN);
  
		while (true) {
			PORTB ^= (1 << IR_PIN);				// toggle LED state
			delay(500);
		}
		
	#elif (HARDWARE_TEST ==2)
		while (true) {
			delayMs(500);
			PORTB |= (1 << IR_PIN);				// LED ON
			
			delayMs(500);
			PORTB &= ~(1 << IR_PIN);			// LED OFF
		}
		
	#endif
}

void setup() {
  #if (HARDWARE_TEST ==1)
  	testHardware();
  #endif
  
  setClock();

  // columns: input + pull-up (must be enabled BEFORE the first scan), rows: high-Z
  DDRA  &= ~COL_MASK;
  PORTA |= COL_MASK;
  rowsHiZ();

  // IR output low
  DDRB  |= (1 << IR_PIN);
  PORTB &= ~(1 << IR_PIN);

  #if (HARDWARE_TEST ==2)
  	testHardware();
  #endif

  // Timer0: CTC, TOP = OCR0A, no prescaler. OC0A is connected only during a "mark"
  TIMSK0 = 0;
  TCCR0A = (1 << WGM01);
  OCR0A  = IR_CARRIER_OCR;
  TCCR0B = (1 << CS00);

  // power saving
  ACSR  |= (1 << ACD);      // analog comparator off
  ADCSRA = 0;               // ADC off
  power_adc_disable();
  power_usi_disable();
  power_timer1_disable();
}

void loop() {
  int8_t k = scanKey();
  if (k < 0) {
    goSleep(true);
    delayMs(DEBOUNCE_MS);
    k = scanKey();
  }
  if (k >= 0) handleKey((uint8_t)k);
}
