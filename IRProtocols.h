/*
  IRProtocols.h - carrier engine + IR protocol encoders for the ATtiny44A remote

  Only ONE protocol is compiled: the one selected by PROTO in Config.h
  Everything not selected disappears at compile time, so flash and RAM stay small

  Every protocol module provides the same small interface used by the main sketch:

    IR_CARRIER_HZ              carrier frequency (can be overridden in Config.h)
    IR_PERIOD_MS               start-to-start period of frames (or repeat codes) while a key is held
    IR_MIN_FRAMES              minimum number of frames per key press (can be overridden in Config.h)
    uint16_t irSendFrame(key)  sends the frame of a key, returns its duration in ms
    uint16_t irSendRepeat(key) sends what is sent while the key stays held (NEC: repeat code,
                               others: the same frame again), returns its duration in ms
    IR_NEW_PRESS()             called once at the beginning of every key press (RC5 toggle bit)

  Building blocks (all marks are synchronised with Timer0, so every burst has the same shape):

    IR_MARK(us)   carrier burst of about "us" microseconds, made of whole carrier pulses
    IR_SPACE(us)  no carrier for "us" microseconds
    "us" must be a compile-time constant (the pulse count is computed by the compiler, not at run time)

  How to add a protocol
    1. Add an ID in Config.h (#define PROTO_XXX n) and a key table for it (KEYS)
    2. Add a block in section 1 below with its timing constants (carrier, period, ...)
    3. Add its encoder in section 3 (use irSendPD() for pulse-distance protocols, or write the
       sequence of IR_MARK / IR_SPACE calls and return the frame duration in ms)
*/

#ifndef IR_PROTOCOLS_H
#define IR_PROTOCOLS_H

#include <stdint.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay_basic.h>

/* ====================================================================== */
/* 1. Protocol parameters (standard timings, all in microseconds)         */
/* ====================================================================== */

#if PROTO == PROTO_NEC
  /* NEC: 32 bits {address, ~address, command, ~command}, LSB first, short repeat code */
  #define IR_FAMILY_PD      1
  #ifndef IR_CARRIER_HZ
    #define IR_CARRIER_HZ   38000UL
  #endif
  #define IR_PERIOD_MS      108
  #define IR_NBYTES         4
  #define PD_HDR_MARK       9000
  #define PD_HDR_SPACE      4500
  #define PD_BIT_MARK       560
  #define PD_ZERO_SPACE     560
  #define PD_ONE_SPACE      1690
  #define IR_HAS_OWN_REPEAT 1
  #ifndef IR_MIN_FRAMES
    #define IR_MIN_FRAMES   1
  #endif

#elif PROTO == PROTO_SAMSUNG
  /* Samsung32: 32 bits {address, address, command, ~command}, LSB first, whole frame repeated */
  #define IR_FAMILY_PD      1
  #ifndef IR_CARRIER_HZ
    #define IR_CARRIER_HZ   38000UL
  #endif
  #define IR_PERIOD_MS      110       /* verify against a capture of the real remote */
  #define IR_NBYTES         4
  #define PD_HDR_MARK       4500
  #define PD_HDR_SPACE      4500
  #define PD_BIT_MARK       560
  #define PD_ZERO_SPACE     560
  #define PD_ONE_SPACE      1690
  #ifndef IR_MIN_FRAMES
    #define IR_MIN_FRAMES   1
  #endif

#elif PROTO == PROTO_AEHA
  /* AEHA / Panasonic "Kaseikyo": 48 bits, LSB first, whole frame repeated (see Config.h) */
  #define IR_FAMILY_PD      1
  #ifndef IR_CARRIER_HZ
    #define IR_CARRIER_HZ   37000UL
  #endif
  #define IR_PERIOD_MS      130
  #define IR_NBYTES         6
  #define PD_HDR_MARK       (8 * AEHA_T_US)
  #define PD_HDR_SPACE      (4 * AEHA_T_US)
  #define PD_BIT_MARK       (1 * AEHA_T_US)
  #define PD_ZERO_SPACE     (1 * AEHA_T_US)
  #define PD_ONE_SPACE      (3 * AEHA_T_US)
  #if AEHA_REPEAT_FRAME
    #define IR_HAS_OWN_REPEAT 1
  #endif
  #ifndef IR_MIN_FRAMES
    #define IR_MIN_FRAMES   1
  #endif

#elif PROTO == PROTO_SIRC
  /* Sony SIRC: 12 / 15 / 20 bits, command (7 bits) first, LSB first, pulse-width coding */
  #ifndef IR_CARRIER_HZ
    #define IR_CARRIER_HZ   40000UL
  #endif
  #define IR_PERIOD_MS      45
  #define SIRC_T            600
  #define SIRC_HDR_MARK     (4 * SIRC_T)
  #define SIRC_ONE_MARK     (2 * SIRC_T)
  #define SIRC_ZERO_MARK    (1 * SIRC_T)
  #define SIRC_SPACE        (1 * SIRC_T)
  #ifndef IR_MIN_FRAMES
    #define IR_MIN_FRAMES   3          /* Sony: a frame is sent at least 3 times */
  #endif

#elif PROTO == PROTO_RC5
  /* Philips RC5 / RC5X: 14 bits, Manchester coding, toggle bit changes on every new key press */
  #ifndef IR_CARRIER_HZ
    #define IR_CARRIER_HZ   36000UL
  #endif
  #define IR_PERIOD_MS      114
  #define RC5_HB            889       /* half bit time */
  #ifndef IR_MIN_FRAMES
    #define IR_MIN_FRAMES   1
  #endif

#else
  #error "Unknown PROTO: select one of the PROTO_xxx values in Config.h"
#endif

/* ====================================================================== */
/* 2. Carrier engine (Timer0 in CTC mode toggles OC0A = PB2)              */
/* ====================================================================== */

#define IR_CARRIER_OCR      (((F_CPU + IR_CARRIER_HZ) / (2UL * IR_CARRIER_HZ)) - 1UL)

#define CARRIER_HALF_CLK    (IR_CARRIER_OCR + 1UL)                 /* timer clocks per half carrier period */
#define CARRIER_PERIOD_CLK  (2UL * CARRIER_HALF_CLK)
#define CLK_PER_US          (F_CPU / 1000000UL)
#define CARRIER_PULSES(us)  ((uint16_t)((((uint32_t)(us)) * CLK_PER_US + CARRIER_PERIOD_CLK / 2) / CARRIER_PERIOD_CLK))
#define BURST_US(n)         ((uint16_t)(((uint32_t)(2UL * (n) - 1UL) * CARRIER_HALF_CLK) / CLK_PER_US))
#define MARK_REST_US(us)    ((uint16_t)(((us) > BURST_US(CARRIER_PULSES(us))) ? ((us) - BURST_US(CARRIER_PULSES(us))) : 0))

#define IR_MARK(us)         markPulses(CARRIER_PULSES(us), MARK_REST_US(us))
#define IR_SPACE(us)        delayUs(us)

#ifdef IR_TEST_MOCK
  /* host-side tests provide these two functions */
  static void markPulses(uint16_t pulses, uint16_t restUs);
  static inline void delayUs(uint16_t us);
#else
  /* Busy-wait independent of the Arduino core: _delay_loop_2 = 4 cycles per iteration */
  /* Works for F_CPU = 4 MHz (1 us/iteration) and 8 MHz (2 iterations per us)           */
  static inline void delayUs(uint16_t us) {
    _delay_loop_2((uint16_t)((uint32_t)us * (F_CPU / 4000000UL)));
  }

  /* A burst of "pulses" whole carrier pulses, then "restUs" of silence (rest of the nominal mark time) */
  static void markPulses(uint16_t pulses, uint16_t restUs) {
    if (pulses == 0) { delayUs(restUs); return; }

    TCCR0A = (1 << WGM01) | (1 << COM0A1);   /* clear OC0A on compare match */
    TCCR0B |= (1 << FOC0A);                  /* force OC0A low -> known start state */
    TCNT0  = 0;
    TCCR0A = (1 << WGM01) | (1 << COM0A0);   /* toggle OC0A on compare match */
    TCCR0B |= (1 << FOC0A);                  /* force a toggle: output goes high, first pulse starts now */
    TCNT0  = 0;                              /* restart the timer -> the first pulse is a full half period */
    TIFR0  = (1 << OCF0A);                   /* clear any pending compare flag */

    for (uint16_t i = 2 * pulses - 1; i; i--) {   /* each compare match toggles the output */
      while (!(TIFR0 & (1 << OCF0A))) { }
      TIFR0 = (1 << OCF0A);
    }

    TCCR0A = (1 << WGM01);                   /* disconnect OC0A (output is low after an odd number of toggles) */
    PORTB &= ~(1 << IR_PIN);
    delayUs(restUs);
  }
#endif

static void delayMs(uint16_t ms) {
  while (ms--) delayUs(1000);
}

/* ====================================================================== */
/* 3. Protocol encoders                                                   */
/* ====================================================================== */

#ifndef IR_NEW_PRESS
  #define IR_NEW_PRESS()    ((void)0)
#endif

/* ---- pulse-distance family (NEC, Samsung, AEHA): header, bits LSB first, stop mark ---- */
#ifdef IR_FAMILY_PD
static uint16_t irSendPD(const uint8_t *b) {
  uint32_t us = (uint32_t)PD_HDR_MARK + PD_HDR_SPACE + PD_BIT_MARK;   /* header + stop mark */
  IR_MARK(PD_HDR_MARK);
  IR_SPACE(PD_HDR_SPACE);
  for (uint8_t i = 0; i < IR_NBYTES; i++) {
    uint8_t v = b[i];
    for (uint8_t n = 0; n < 8; n++) {
      uint16_t sp = (v & 1) ? PD_ONE_SPACE : PD_ZERO_SPACE;
      IR_MARK(PD_BIT_MARK);
      IR_SPACE(sp);
      us += (uint32_t)PD_BIT_MARK + sp;
      v >>= 1;
    }
  }
  IR_MARK(PD_BIT_MARK);   /* stop bit */
  return (uint16_t)(us / 1000UL);
}
#endif

#if PROTO == PROTO_NEC
/* KEYS[key] = { address, command } */
static uint16_t irSendFrame(uint8_t key) {
  uint8_t a = pgm_read_byte(&KEYS[key][0]);
  uint8_t c = pgm_read_byte(&KEYS[key][1]);
  uint8_t b[IR_NBYTES] = { a, (uint8_t)~a, c, (uint8_t)~c };
  return irSendPD(b);
}

static uint16_t irSendRepeat(uint8_t key) {
  (void)key;
  IR_MARK(9000);
  IR_SPACE(2250);
  IR_MARK(560);
  return (uint16_t)((9000UL + 2250UL + 560UL) / 1000UL);
}

#elif PROTO == PROTO_SAMSUNG
/* KEYS[key] = { address, command } */
static uint16_t irSendFrame(uint8_t key) {
  uint8_t a = pgm_read_byte(&KEYS[key][0]);
  uint8_t c = pgm_read_byte(&KEYS[key][1]);
  uint8_t b[IR_NBYTES] = { a, a, c, (uint8_t)~c };
  return irSendPD(b);
}

#elif PROTO == PROTO_AEHA
/* KEYS[key] = 16-bit key code: low byte = command, high byte = parity (as printed by the dump tools) */
static uint16_t irSendFrame(uint8_t key) {
  uint16_t k = pgm_read_word(&KEYS[key]);
  uint8_t b[IR_NBYTES] = {
    (uint8_t)(REMOTE_CODE & 0xFF),
    (uint8_t)(REMOTE_CODE >> 8),
    (uint8_t)AEHA_BYTE2,
    (uint8_t)AEHA_BYTE3,
    (uint8_t)(k & 0xFF),       /* command */
    (uint8_t)(k >> 8)          /* parity byte */
  };
  return irSendPD(b);
}

#if AEHA_REPEAT_FRAME
/* AEHA repeat frame: leader mark 8T, space 8T, stop mark T (no data bits) */
static uint16_t irSendRepeat(uint8_t key) {
  (void)key;
  IR_MARK(PD_HDR_MARK);
  IR_SPACE(8 * AEHA_T_US);
  IR_MARK(PD_BIT_MARK);
  return (uint16_t)(((uint32_t)PD_HDR_MARK + 8UL * AEHA_T_US + PD_BIT_MARK) / 1000UL);
}
#endif

#elif PROTO == PROTO_SIRC
/* KEYS[key] = data word, transmitted LSB first: command (7 bits), then address (5 / 8 bits) [, extension] */
static uint16_t irSendFrame(uint8_t key) {
  uint32_t data = pgm_read_dword(&KEYS[key]);
  uint32_t us = (uint32_t)SIRC_HDR_MARK + SIRC_SPACE;
  IR_MARK(SIRC_HDR_MARK);
  IR_SPACE(SIRC_SPACE);
  for (uint8_t i = 0; i < SIRC_BITS; i++) {
    if (data & 1) { IR_MARK(SIRC_ONE_MARK);  us += SIRC_ONE_MARK;  }
    else          { IR_MARK(SIRC_ZERO_MARK); us += SIRC_ZERO_MARK; }
    IR_SPACE(SIRC_SPACE);
    us += SIRC_SPACE;
    data >>= 1;
  }
  return (uint16_t)(us / 1000UL);
}

#elif PROTO == PROTO_RC5
/* KEYS[key] = (address << 7) | command  (address 5 bits, command 7 bits; command bit 6 = RC5X field bit) */
static uint8_t rc5Toggle = 0;               /* kept in RAM during sleep */
#undef  IR_NEW_PRESS
#define IR_NEW_PRESS()      (rc5Toggle ^= 1)

static void rc5Run(uint8_t level, uint8_t halfBits) {
  if (level) { if (halfBits == 1) IR_MARK(RC5_HB);  else IR_MARK(2 * RC5_HB);  }
  else       { if (halfBits == 1) IR_SPACE(RC5_HB); else IR_SPACE(2 * RC5_HB); }
}

static uint16_t irSendFrame(uint8_t key) {
  uint16_t v    = pgm_read_word(&KEYS[key]);
  uint8_t  addr = (v >> 7) & 0x1F;
  uint8_t  cmd  = v & 0x7F;

  /* 14 bits, MSB first: S1 S2 T A4..A0 C5..C0 */
  uint16_t w = 0x2000;                       /* S1 = 1 */
  if (!(cmd & 0x40)) w |= 0x1000;            /* S2 = inverted command bit 6 (1 for plain RC5) */
  if (rc5Toggle)     w |= 0x0800;            /* toggle bit */
  w |= (uint16_t)addr << 6;
  w |= cmd & 0x3F;

  /* Manchester: bit 1 = space,mark   bit 0 = mark,space  (half bit = RC5_HB) */
  uint8_t  cur = 0, n = 0;
  uint16_t halves = 0;
  bool     started = false;
  for (int8_t i = 13; i >= 0; i--) {
    uint8_t bit = (w >> i) & 1;
    for (uint8_t h = 0; h < 2; h++) {
      uint8_t level = (h ^ bit ^ 1) & 1;
      if (!started) {                        /* the leading idle half bit is not transmitted */
        if (!level) continue;
        started = true; cur = level; n = 1; halves = 1;
        continue;
      }
      halves++;
      if (level == cur) n++;
      else { rc5Run(cur, n); cur = level; n = 1; }
    }
  }
  rc5Run(cur, n);
  return (uint16_t)(((uint32_t)halves * RC5_HB) / 1000UL);
}
#endif

/* protocols that repeat the whole frame while the key is held */
#ifndef IR_HAS_OWN_REPEAT
static uint16_t irSendRepeat(uint8_t key) { return irSendFrame(key); }
#endif

// Number of repeats allowed while a key is held (time limit MAX_HOLD_MS / frame period)
#define MAX_REPEATS   		((uint16_t)(MAX_HOLD_MS / IR_PERIOD_MS))

#endif /* IR_PROTOCOLS_H */
