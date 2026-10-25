/*
  IR remote - ATtiny44A, 3x7 key matrix (21 keys), Power-down sleep           (project IRremoteClone)

  Files of the sketch:
    IRremoteClone.ino   					main program: matrix scan, sleep / wake-up, key handling
    Config.h           						THIS FILE: protocol selection, your settings and key codes
    IRProtocols.h      						carrier engine + protocol encoders (only the selected protocol is compiled)
    IR_Remote_Clone_Guide.md			user guide...

  Supported protocols (select exactly one with PROTO below):
    PROTO_NEC      				NEC, 32 bit
    PROTO_SAMSUNG  				Samsung32
    PROTO_AEHA     				AEHA / Panasonic Kaseikyo, 48 bit
    PROTO_SIRC     				Sony SIRC 12 / 15 / 20 bit
    PROTO_RC5      				Philips RC5 / RC5X

  Arduino IDE settings (ATTinyCore):
    https://github.com/spencekonde/attinycore
    http://drazzy.com/package_drazzy.com_index.json

    Chip:              		ATtiny44(a)
    Clock:             		4 MHz (internal)  <- recommended for CR2032 / 2x AAA (1.8-2.7 V spec limit)
    B.O.D.:            		Disabled
    millis()/micros(): 		Disabled (if the option exists)
    Fuses: 								the factory defaults are correct, nothing has to be changed:
           										LFUSE = 0x62, HFUSE = 0xDF, EFUSE = 0xFF
           										(the sketch sets the clock itself with CLKPR, so the CKDIV8 fuse does not matter)

    Export the HEX with: 	Sketch -> Export Compiled Binary (use the file WITHOUT "with_bootloader")

  Matrix: 3 COLUMNS (inputs) x 7 ROWS (outputs) = 21 keys, 10 pins in total.
  Pin assignment (every mask is contiguous and wake-up needs one interrupt):

    PB2 (OC0A)           -> IR LED driver (resistor -> NPN), carrier from Timer0
    PA0, PA1, PA2        -> COLUMNS 0..2  (inputs, pull-up, wake-up pins, mask 0x07)
    PA3,PA4,PA5,PA6,PA7  -> ROWS 0..4     (outputs, LOW in sleep, mask 0xF8)
    PB0, PB1             -> ROWS 5..6     (outputs, LOW in sleep)
    PB3 (RESET)          -> untouched (ISP)

    NOTE: PA4/PA5/PA6 (rows 1..3) are also the ISP lines SCK/MISO/MOSI
          Do not press any key while programming!

  Key index = row*3 + col  (0..20)
*/

// F_CPU is passed to the compiler by the Arduino IDE from Tools -> Clock (do NOT redefine it here)
// The fallback below is only used when building outside the Arduino IDE without -DF_CPU=...
#ifndef F_CPU
  #define F_CPU 					4000000UL
#endif

#if F_CPU == 4000000UL
  #define CLK_DIV_BITS  	1      // 8 MHz internal RC / 2 = 4 MHz
#elif F_CPU == 8000000UL
  #define CLK_DIV_BITS  	0      // 8 MHz internal RC / 1
#else
  #error "Select 4 MHz or 8 MHz internal clock (Tools -> Clock)"
#endif

// 0: normal firmware
// 1: blink #1 PB2 forever (hardware bring-up)
// 2: blink #2 PB2 forever (hardware bring-up)
#define HARDWARE_TEST    0


// ---------------- protocol selection (only ONE protocol is compiled) ----------------
#define PROTO_NEC         1
#define PROTO_AEHA        2
#define PROTO_SAMSUNG     3
#define PROTO_SIRC        4
#define PROTO_RC5         5

#define PROTO             PROTO_AEHA

// Optional overrides (defaults are in IRProtocols.h):
//#define IR_CARRIER_HZ   38000UL     // carrier frequency
//#define IR_MIN_FRAMES   2           // minimum frames per key press (AEHA capture: a single click = 2 frames)


// ---------------- protocol settings ----------------
#define REMOTE_CODE				0x2002		// NEC:								0x00
																		// SAMSUNG:						0x07
																		// Sony SIRC:					0x01				address 5 bits for 12 bit frames, 8 bits for 15, 13 bits for 20
																		// Philips RC5/RC5X:	0x00				address 5 bits, command 6 bits, or 7 bits for RC5X
																		// Panasonic AEHA:		0x2002


// ---------------- hardware definitions - fixed! ----------------
#define IR_PIN        		PB2
#define COL_MASK      		0x07                                     // PA0..PA2 (columns, wake-up pins)
#define ROWA_MASK     		0xF8                                     // PA3..PA7 = rows 0..4
#define ROWB_MASK     		((1 << PB0) | (1 << PB1))                // rows 5..6

#define DEBOUNCE_MS   		20
#define MAX_HOLD_MS   		15000                                    // stop repeating after ~15 s of holding (a stuck key must not drain the battery)


// ---------------- matrix size - fixed! ----------------
#define N_COLS  					3
#define N_ROWS  					7
#define N_KEYS  					(N_ROWS * N_COLS)


// ---------------- key codes ----------------
// Index = row*3 + col. Re-order the entries to match the real wiring of your keyboard.
#if PROTO == PROTO_NEC
  // NEC: { address, command }   (PLACEHOLDERS - replace with captured codes)
  static const uint8_t KEYS[N_KEYS][2] PROGMEM = {
    {REMOTE_CODE, 0x00}, {REMOTE_CODE, 0x01}, {REMOTE_CODE, 0x02},   // row 0
    {REMOTE_CODE, 0x03}, {REMOTE_CODE, 0x04}, {REMOTE_CODE, 0x05},   // row 1
    {REMOTE_CODE, 0x06}, {REMOTE_CODE, 0x07}, {REMOTE_CODE, 0x08},   // row 2
    {REMOTE_CODE, 0x09}, {REMOTE_CODE, 0x0A}, {REMOTE_CODE, 0x0B},   // row 3
    {REMOTE_CODE, 0x0C}, {REMOTE_CODE, 0x0D}, {REMOTE_CODE, 0x0E},   // row 4
    {REMOTE_CODE, 0x0F}, {REMOTE_CODE, 0x10}, {REMOTE_CODE, 0x11},   // row 5
    {REMOTE_CODE, 0x12}, {REMOTE_CODE, 0x13}, {REMOTE_CODE, 0x14}    // row 6
  };

#elif PROTO == PROTO_SAMSUNG
  // Samsung32: { address, command }   (PLACEHOLDERS - replace with captured codes)
  static const uint8_t KEYS[N_KEYS][2] PROGMEM = {
    {REMOTE_CODE, 0x00}, {REMOTE_CODE, 0x01}, {REMOTE_CODE, 0x02},   // row 0
    {REMOTE_CODE, 0x03}, {REMOTE_CODE, 0x04}, {REMOTE_CODE, 0x05},   // row 1
    {REMOTE_CODE, 0x06}, {REMOTE_CODE, 0x07}, {REMOTE_CODE, 0x08},   // row 2
    {REMOTE_CODE, 0x09}, {REMOTE_CODE, 0x0A}, {REMOTE_CODE, 0x0B},   // row 3
    {REMOTE_CODE, 0x0C}, {REMOTE_CODE, 0x0D}, {REMOTE_CODE, 0x0E},   // row 4
    {REMOTE_CODE, 0x0F}, {REMOTE_CODE, 0x10}, {REMOTE_CODE, 0x11},   // row 5
    {REMOTE_CODE, 0x12}, {REMOTE_CODE, 0x13}, {REMOTE_CODE, 0x14}    // row 6
  };

#elif PROTO == PROTO_AEHA
	// AEHA (Panasonic "Kaseikyo" family)
	// 48-bit frame, every byte is sent LSB first, bytes in this order:
	//   byte0, byte1 : customer code, low byte first            (0x2002 -> 0x02, 0x20)
	//   byte2        : (AEHA_DATA0 << 4) | customer parity      (parity = XOR of the 4 nibbles of the customer code)
	//   byte3        : AEHA_BYTE3 (fixed system byte)
	//   byte4, byte5 : key code, LOW byte first: command, then parity byte (= byte2 ^ byte3 ^ command)
	//
	// Example: KEY_PWR = 0xBD3D  ->  02 20 80 00 3D BD
	//
	#define AEHA_DATA0        		0x8       // 4-bit field after the parity nibble      (confirmed by capture)
	#define AEHA_BYTE3        		0x00      // fixed system byte                        (confirmed by capture)
	#define AEHA_T_US         		432       // time unit T in us (captured: 431.7; AEHA nominal is 425)

	#define AEHA_REPEAT_FRAME 		0         // 0: the whole frame is repeated while the key is held (confirmed by capture)
	                                		    // 1: AEHA repeat frame (8T mark + 8T space + stop), IRCdump prints ">AEHA repeat"

	#define AEHA_PARITY       		((((REMOTE_CODE) >> 12) ^ ((REMOTE_CODE) >> 8) ^ ((REMOTE_CODE) >> 4) ^ (REMOTE_CODE)) & 0x0F)
	#define AEHA_BYTE2        		((AEHA_DATA0 << 4) | AEHA_PARITY)
 
  // 16-bit key codes extracted from the original remote (customer code 0x2002).
  static const uint16_t KEYS[N_KEYS] PROGMEM = {
    0x9010, 0x9111, 0x9212,   // row 0:   KEY_N01   KEY_N02    KEY_N03
    0x9313, 0x9414, 0x9515,   // row 1:   KEY_N04   KEY_N05    KEY_N06
    0x9616, 0x9717, 0x9818,   // row 2:   KEY_N07   KEY_N08    KEY_N09
    0xA020, 0xA121, 0xB434,   // row 3:   KEY_PLUS  KEY_MINUS  KEY_UP
    0xB535, 0x8C0C, 0xB939,   // row 4:   KEY_DOWN  KEY_N      KEY_CHN
    0xBD3D, 0xBB3B, 0x8505,   // row 5:   KEY_PWR   KEY_NNN    KEY_TVAV
    0x8F0F, 0xB131, 0xB333    // row 6:   KEY_TIME  KEY_SURND  KEY_III
  };

#elif PROTO == PROTO_SIRC
	// Sony SIRC: number of bits (12 = 7 command + 5 address, 15 = 7 + 8, 20 = 7 + 5 + 8 extension)
	#define SIRC_BITS         12
  
  // data word = command | (address << 7)   (PLACEHOLDERS - replace with captured codes)
  #define SIRC_DATA(a, c)  (((uint32_t)(a) << 7) | ((c) & 0x7F))
  static const uint32_t KEYS[N_KEYS] PROGMEM = {
    SIRC_DATA(REMOTE_CODE, 0x00), SIRC_DATA(REMOTE_CODE, 0x01), SIRC_DATA(REMOTE_CODE, 0x02),   // row 0
    SIRC_DATA(REMOTE_CODE, 0x03), SIRC_DATA(REMOTE_CODE, 0x04), SIRC_DATA(REMOTE_CODE, 0x05),   // row 1
    SIRC_DATA(REMOTE_CODE, 0x06), SIRC_DATA(REMOTE_CODE, 0x07), SIRC_DATA(REMOTE_CODE, 0x08),   // row 2
    SIRC_DATA(REMOTE_CODE, 0x09), SIRC_DATA(REMOTE_CODE, 0x0A), SIRC_DATA(REMOTE_CODE, 0x0B),   // row 3
    SIRC_DATA(REMOTE_CODE, 0x0C), SIRC_DATA(REMOTE_CODE, 0x0D), SIRC_DATA(REMOTE_CODE, 0x0E),   // row 4
    SIRC_DATA(REMOTE_CODE, 0x0F), SIRC_DATA(REMOTE_CODE, 0x10), SIRC_DATA(REMOTE_CODE, 0x11),   // row 5
    SIRC_DATA(REMOTE_CODE, 0x12), SIRC_DATA(REMOTE_CODE, 0x13), SIRC_DATA(REMOTE_CODE, 0x14)   // row 6
  };

#elif PROTO == PROTO_RC5
  // Philips RC5: (address << 7) | command   (PLACEHOLDERS - replace with captured codes)
  #define RC5_DATA(a, c)   ((uint16_t)((((a) & 0x1F) << 7) | ((c) & 0x7F)))
  static const uint16_t KEYS[N_KEYS] PROGMEM = {
    RC5_DATA(REMOTE_CODE, 0x00), RC5_DATA(REMOTE_CODE, 0x01), RC5_DATA(REMOTE_CODE, 0x02),   // row 0
    RC5_DATA(REMOTE_CODE, 0x03), RC5_DATA(REMOTE_CODE, 0x04), RC5_DATA(REMOTE_CODE, 0x05),   // row 1
    RC5_DATA(REMOTE_CODE, 0x06), RC5_DATA(REMOTE_CODE, 0x07), RC5_DATA(REMOTE_CODE, 0x08),   // row 2
    RC5_DATA(REMOTE_CODE, 0x09), RC5_DATA(REMOTE_CODE, 0x0A), RC5_DATA(REMOTE_CODE, 0x0B),   // row 3
    RC5_DATA(REMOTE_CODE, 0x0C), RC5_DATA(REMOTE_CODE, 0x0D), RC5_DATA(REMOTE_CODE, 0x0E),   // row 4
    RC5_DATA(REMOTE_CODE, 0x0F), RC5_DATA(REMOTE_CODE, 0x10), RC5_DATA(REMOTE_CODE, 0x11),   // row 5
    RC5_DATA(REMOTE_CODE, 0x12), RC5_DATA(REMOTE_CODE, 0x13), RC5_DATA(REMOTE_CODE, 0x14)    // row 6
  };

#endif //PROTO
