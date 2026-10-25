# IR Remote Clone: Hardware and Firmware Guide

A practical guide to reproducing a Panasonic-family infrared remote using an ATtiny44A. It covers protocol identification, signal capture, component selection, hardware wiring, low-power firmware configuration, programming, verification, and troubleshooting.

> **Hardware schematic:** See [`IRremote.pdf`](IRremote.pdf).
>
> **Firmware files in this repository:** `IRremoteClone.ino`, `Config.h`, and `IRProtocols.h`.
>
> **Validation status**
> - The original remote was identified as AEHA (Panasonic/Kaseikyo family), with customer code `0x2002`. Its frame bytes, timing, and repeat behavior were derived from real captures.
> - An earlier single-protocol AEHA implementation was tested on hardware. The multi-protocol v2 firmware was syntax-checked and its frame generation was simulated on a computer, but the guide's original test record says that v2 had not yet been fully tested on the ATtiny44A board.
> - NEC, Samsung, Sony SIRC, and RC5 encoders are included as starting points and have not been confirmed with a physical target device. Treat these as requiring hardware validation.

## 1. Project overview

| Item | Design |
|---|---|
| Goal | Reproduce a legacy Panasonic remote using AEHA; other protocol encoders are available by compile-time selection |
| MCU | ATtiny44A-SSU, SOIC-14 |
| Development environment | Arduino IDE + ATTinyCore |
| Keypad | 3 × 7 matrix, 21 keys, 10 GPIO lines |
| Main supply | Two AAA cells, nominal 3 V |
| IR output | 940 nm IR LED with an NPN transistor driver |
| Power management | Power-down sleep; wake-up using pin-change interrupt |
| Firmware structure | Main sketch, `Config.h`, and `IRProtocols.h` |

## 2. Identifying the original protocol

### 2.1 AEHA / Panasonic Kaseikyo family

The captured original remote uses **AEHA**, in the Panasonic/Kaseikyo family—not NEC. Its customer code is `0x2002`.

### 2.2 Timing comparison

`T` is the base timing unit. The nominal AEHA value is approximately 425 µs; the captured remote measured about 431.7 µs, so the firmware uses `AEHA_T_US = 432`.

| Parameter | NEC | AEHA (this remote) |
|---|---:|---:|
| Carrier | 38 kHz | 37 kHz |
| Header mark | 9000 µs | 8T = 3456 µs |
| Header space | 4500 µs | 4T = 1728 µs |
| Bit mark | 560 µs | T = 432 µs |
| Space for 0 | 560 µs | T = 432 µs |
| Space for 1 | 1690 µs | 3T = 1296 µs |
| Frame | 32 bits | 48 bits (6 bytes) |
| Bit order | LSB first | LSB first |
| Repeat while held | Short repeat frame, typically about every 108 ms | Full frame repeated every 130 ms, start-to-start, as captured |

Values are nominal/derived values. Receiver demodulation can shorten observed marks and lengthen spaces; use the original capture as the reference.

### 2.3 AEHA frame layout

Each frame contains six bytes; each byte is transmitted LSB first.

| Byte | Meaning | Captured value / example |
|---|---|---|
| 0 | Customer code, low byte | `0x02` |
| 1 | Customer code, high byte | `0x20` |
| 2 | `(DATA0 << 4) | customer-code nibble parity` | `0x80` |
| 3 | Fixed system byte | `0x00` |
| 4 | Command, low byte of 16-bit key code | `0x3D` for Power |
| 5 | Parity byte / high byte of key code | `0xBD` for Power |

For the Power key, `KEY_PWR = 0xBD3D` produces the frame `02 20 80 00 3D BD`.

Across the 21 captured keys, the high byte follows the relation `high = low XOR 0x80`, consistent with the observed parity relationship. Examples of command bytes include `0x3D` (Power), `0x20`/`0x21` (volume), `0x34`/`0x35` (channel/navigation), `0x05` (TV/AV), and `0x10`–`0x18` (digits 1–9).

### 2.4 Captured Power-key verification

The original Power key was captured using an ESP32 and the `IRrecvDumpV2` example from IRremoteESP8266. Twelve frames were manually decoded.

| Check | Result |
|---|---|
| Decoded bytes | `02 20 80 00 3D BD` in all 12 frames |
| Byte 2 / byte 3 | `0x80` / `0x00` |
| Base time `T` | About 431.7 µs |
| Header | About 3400 µs mark and 1750 µs space in the demodulated capture |
| Hold repeat | Full frame every 130 ms, start-to-start |
| Short press | Two frames were captured |
| Frame duration | About 59 ms for the captured Power frame |

The receiver library reported `UNKNOWN` and a value such as `0xF61E2A57`; that value is a hash, not the actual payload. The bytes above were manually decoded from raw timings, treating longer spaces as logical ones and reading each byte LSB first.

A demodulating IR receiver cannot measure the LED carrier frequency. The firmware defaults to 37 kHz; use an oscilloscope or suitable photodiode/instrumentation to measure the original carrier accurately. For the captured behavior “one short press = two frames,” set `IR_MIN_FRAMES` to `2` in `Config.h` (the default is `1`).

## 3. Capturing an original remote

### 3.1 Parts and wiring

Use a 38 kHz demodulating IR receiver such as TSOP4838 or VS1838, an Arduino-compatible board, and optionally a logic analyzer.

| Receiver pin | Connection |
|---|---|
| OUT | Digital input, e.g. D2 |
| GND | Ground |
| VCC | 5 V or 3.3 V as permitted by the receiver datasheet |

Run an IRremote receive example such as `ReceiveDump` or `SimpleReceiver`. Record the reported protocol, address/command or raw data, and raw mark/space timings. Capture both a short key press and a held press to determine whether the remote repeats a full frame or sends a dedicated repeat frame.

A TSOP-style receiver filters the carrier; its output does not expose the carrier waveform. Infer a likely frequency from the protocol only as an estimate, or measure the original LED with an oscilloscope.

### 3.2 Notes on the legacy `IRCdump` capture tool

The original capture utility was based on a ChaN-style IR receiver for an ATmega128. It used Timer1 input capture (ICP1 / PD4) with a timer tick of 0.5 µs, and recognized NEC, AEHA, Sony, RC5, and Samsung. For AEHA it expected a 425 µs unit, an 8T mark header, and either a 4T data-frame space or an 8T repeat-frame space. It accepted frames of at least 48 bits in whole-byte increments and stored each byte LSB first.

Its serial output (115200 baud) printed received bytes, for example `>AEHA 0x02 0x20 ...`, or `>AEHA repeat`. The utility did not measure the optical carrier frequency or provide a precision measurement of `T`; the IR receiver removes the carrier. Its output also did not include the observed bytes 2 and 3 in the user's original text file.

The utility could send a test frame using a command such as `A 0x02 0x20 0x80 0x00 0x3D 0xBD`, but its nominal 38 kHz output should be measured before using it as a timing reference.

## 4. Component selection

### 4.1 Why ATtiny44A?

The original remote IC, marked `SH10P43SANJ`, did not have an accessible datasheet and appeared to be a masked/OTP or locked device, making signal capture the practical route to reproduction. Fixed-protocol remote encoder ICs such as HT6221 do not offer the flexibility needed for an accurate AEHA clone.

The ATtiny44A is a compact SOIC-14 MCU with 4 KB flash, 256 bytes RAM, low-power sleep, and pin-change interrupts on Port A. Its I/O count supports the 21-key matrix when RESET and the IR carrier output are reserved.

| MCU | Advantages | Limitations |
|---|---|---|
| ATmega328P | Familiar, more memory | Larger; good for prototyping |
| ATtiny24A | Small and inexpensive | 2 KB flash, 128 bytes RAM |
| **ATtiny44A** | Good balance and standard ISP | Limited memory; pin budgeting matters |
| ATtiny1614 | Modern, 16 KB flash | UPDI programming and more capacity than this design needs |

### 4.2 Bill of materials

| Part | Suggested specification | Qty |
|---|---|---:|
| MCU | ATtiny44A-SSU | 1 |
| IR LED | 940 nm, e.g. TSAL6200 or IR333 | 1 |
| NPN transistor | BC337 or equivalent small-signal transistor | 1 |
| Base resistor | 1 kΩ | 1 |
| IR LED series resistor | 27 Ω for two AAA cells; 68 Ω for CR2032 low-range design | 1 |
| Bulk capacitor | 100–470 µF, preferably low ESR | 1 |
| MCU decoupling capacitor | 100 nF close to VCC/GND | 1 |
| RESET pull-up (optional) | 10 kΩ | 1 |
| Push buttons | As required by the remote | Up to 21 |
| Battery holder | 2 × AAA | 1 |
| ISP header | 6-pin | 1 |

These values are starting points, not a substitute for checking LED pulse-current limits, transistor saturation, and supply droop on the assembled circuit.

## 5. Hardware design

The complete schematic is provided in [`IRremote.pdf`](IRremote.pdf). The pin assignment and functional connections are summarized below.

### 5.1 ATtiny44A SOIC-14 pinout

```text
           ATtiny44A
         ┌────U────┐
  VCC  1 │         │ 14 GND
  PB0  2 │         │ 13 PA0
  PB1  3 │         │ 12 PA1
RESET  4 │         │ 11 PA2
  PB2  5 │         │ 10 PA3
  PA7  6 │         │  9 PA4
  PA6  7 │         │  8 PA5
         └─────────┘
```

### 5.2 GPIO assignment

PB3/RESET is reserved for programming and PB2/OC0A generates the IR carrier. The remaining ten GPIO lines form a 3-column × 7-row keypad.

| MCU signal | Physical pin | Function |
|---|---:|---|
| PB2 / OC0A | 5 | IR carrier output, transistor driver |
| PA0 | 13 | Column 0 |
| PA1 | 12 | Column 1 |
| PA2 | 11 | Column 2 |
| PA3 | 10 | Row 0 |
| PA4 | 9 | Row 1; ISP SCK line |
| PA5 | 8 | Row 2; ISP MISO line |
| PA6 | 7 | Row 3; ISP MOSI line |
| PA7 | 6 | Row 4 |
| PB0 | 2 | Row 5 |
| PB1 | 3 | Row 6 |
| RESET / PB3 | 4 | RESET / ISP |

Key index = `row * 3 + column`, from 0 to 20.

This arrangement keeps all three columns on PA0–PA2, allowing a single Port A pin-change interrupt group to wake the MCU. The rows use PA3–PA7 plus PB0/PB1. The firmware first drives the rows LOW and checks whether any column went LOW; if none did, it can return without scanning each row individually.

**ISP warning:** Rows 1–3 share PA4–PA6 with ISP SCK/MISO/MOSI. Do not press any key during programming, and avoid circuitry that loads these ISP lines. Never use RESET as a keypad line. The same ten GPIO lines could support up to 25 keys with a 5 × 5 matrix, but the provided firmware is configured for 21 keys.

### 5.3 3 × 7 matrix

```text
                 Column 0  Column 1  Column 2
                  (PA0)     (PA1)     (PA2)
Row 0 (PA3)         K0        K1        K2
Row 1 (PA4)         K3        K4        K5
Row 2 (PA5)         K6        K7        K8
Row 3 (PA6)         K9        K10       K11
Row 4 (PA7)         K12       K13       K14
Row 5 (PB0)         K15       K16       K17
Row 6 (PB1)         K18       K19       K20
```

Each button connects one row to one column. Columns are inputs with internal pull-ups. In sleep, rows are LOW; pressing a key pulls its column LOW and wakes the MCU. During scanning, each row is selected LOW in turn while other rows are left high-impedance.

While a key is held, current through a column pull-up is estimated at roughly 60–100 µA. The firmware limits repeat duration and then returns to sleep if a key remains stuck, avoiding continuous high current.

**Ghosting:** Multiple simultaneous key presses can be misread in a diode-less matrix. If multi-key input is required, add one 1N4148 signal diode per key; the documented orientation is anode toward the column and cathode toward the row.

### 5.4 IR LED driver

Use PB2/OC0A to drive the base of the BC337 through a 1 kΩ resistor. The transistor switches the IR LED current; do not drive a high-current IR LED directly from the MCU.

For the two-AAA design, the guide uses a 27 Ω LED series resistor. Place a 100–470 µF bulk capacitor near the LED/MCU supply path to reduce battery voltage dips during pulses. The estimated peak LED current with fresh AAA cells is around 50–60 mA, falling as the cells discharge. The carrier output uses approximately 50% duty cycle, higher than the 33% sometimes used in IR transmitters; adjust the resistor only after checking LED and transistor limits.

### 5.5 Power supply

- Two AAA cells provide about 3.2 V when fresh and may fall toward 2 V near end of life.
- Fit a 100 nF decoupling capacitor close to the MCU VCC/GND pins.
- A 10 kΩ RESET-to-VCC pull-up is optional.

## 6. Power management

### 6.1 Clock and voltage

| Supply voltage | ATtiny44A clock range stated in the design notes |
|---|---|
| 1.8–5.5 V | Up to 4 MHz |
| 2.7–5.5 V | Up to 10 MHz |

Because two AAA cells can fall toward 2 V, the design uses the internal 4 MHz clock.

### 6.2 Firmware power-saving measures

- `SLEEP_MODE_PWR_DOWN`
- ADC, analog comparator, USI, and Timer1 disabled
- Brown-out detection (BOD) disabled in the recommended fuse configuration to reduce sleep current
- All keypad pins are driven to defined states; no matrix pins are intentionally left floating

### 6.3 Battery-life estimate for two AAA cells

Reported measurements from the original design notes:

- Sleep current: 0.4 µA at 3.125 V and 1.1 µA at 5 V.
- During a held-key transmission: 2.76 mA at 3.125 V and 4.3 mA at 5 V on the meter. The measured voltage across the 22 Ω resistor was about 150 mV, implying approximately 74 mA peak and an estimated average transmission current near 8 mA. The estimate uses 8 mA conservatively.

Example usage model: ten adjustment sessions per day. Each session includes approximately 15 seconds of sustained transmission and twelve short presses totalling about 3 seconds.

| Metric | Approximate value |
|---|---:|
| Transmission per session | 18 s |
| Charge per session at 8 mA | 0.04 mAh |
| Annual transmission charge, 10 sessions/day | 146 mAh |
| Annual sleep charge | 3.5 mAh |
| Total annual estimate | About 150 mAh |

| Battery | Effective capacity assumed | Estimated life |
|---|---:|---|
| Two alkaline AAA cells | About 1000 mAh | About 5–7 years, likely limited by self-discharge |
| CR2032 | About 100–150 mAh at this load | About 8–12 months, with pulse droop/reset risk |

These are rough estimates. Long held presses dominate the energy use, and most transmission energy is consumed by the IR LED. Lowering LED pulse current can extend battery life approximately in proportion, but reduces range. Two AAA cells are recommended for the documented use pattern.

`MAX_HOLD_MS` defaults to 15000 ms. This allows an 8-second hold required by the original use case; a prior repeat-count limit stopped transmission too early. The time-based limit does not depend on the protocol repeat period.

### 6.4 CR2032 option for a short range

For a maximum range around 1 m, the guide reports a 68 Ω LED series resistor. It worked well at 1 m with a used CR2032 measuring 2.75 V.

Estimated peak LED current, assuming LED Vf ≈ 1.3 V and transistor VCE ≈ 0.2 V:

| Battery voltage | Estimated peak LED current |
|---|---:|
| 3.0 V | 22 mA |
| 2.75 V | 18 mA |
| 2.4 V | 12 mA |

With ten sessions per day and about 18 seconds of transmission per session, the guide estimates annual usage around 65–70 mAh and a possible life of 10 months to 2 years depending on effective capacity. These are estimates, not guaranteed service-life figures.

Remaining validation work noted in the original guide: repeat the 1 m range test at 2.4–2.5 V and observe MCU VCC during a long key hold with an oscilloscope to check for resets or brownout-like behavior.

## 7. Firmware setup and programming

### 7.1 Install ATTinyCore

Install the Arduino IDE and ATTinyCore using the core's official instructions: <https://github.com/SpenceKonde/ATTinyCore>.

### 7.2 Arduino IDE board settings

| Setting | Value |
|---|---|
| Board | ATtiny24/44/84 (No bootloader) |
| Chip | ATtiny44(a) |
| Clock | 4 MHz (internal) |
| B.O.D. | Disabled |
| `millis()` / `micros()` | Disabled if available |
| Programmer | Match the ISP programmer being used |

The IDE supplies `F_CPU` from the selected clock; do not redefine it manually. Select 4 MHz before exporting the compiled binary. A bootloader is not required. Use **Upload Using Programmer**, or export the HEX and program it with an ISP tool.

### 7.3 ISP connections

| ISP signal | ATtiny44A pin |
|---|---|
| MOSI | PA6, physical pin 7 |
| MISO | PA5, physical pin 8 |
| SCK | PA4, physical pin 9 |
| RESET | Physical pin 4 |
| VCC | Physical pin 1 |
| GND | Physical pin 14 |

When using an Arduino Uno as ISP, run the `ArduinoISP` example and place about a 10 µF capacitor between the Uno RESET and GND to prevent auto-reset.

### 7.4 Project structure and configuration

Keep the `.ino` and both headers in the same Arduino sketch directory. The sketch filename should match the directory name.

| File | Purpose |
|---|---|
| `IRremoteClone.ino` | Matrix scan, sleep/wake, key handling, repeat management, `setup()` |
| `Config.h` | Protocol selection, per-protocol options, key map, pins, test mode, hold limit |
| `IRProtocols.h` | Timer0 carrier engine and protocol encoders |

Select one protocol at compile time. The default is AEHA:

```cpp
#define PROTO_NEC      1
#define PROTO_AEHA     2
#define PROTO_SAMSUNG  3
#define PROTO_SIRC     4
#define PROTO_RC5      5

#define PROTO          PROTO_AEHA
```

Useful options in `Config.h`:

| Option | Meaning |
|---|---|
| `HARDWARE_TEST` | `1` enables a hardware test that toggles PB2; `0` is normal operation |
| `MAX_HOLD_MS` | Maximum held-key repeat time; default 15000 ms |
| `IR_CARRIER_HZ` | Optional carrier override; each protocol has a default |
| `IR_MIN_FRAMES` | Minimum frames sent per press; default 1, set to 2 to match the captured AEHA short-press behavior |

AEHA options for the captured Panasonic remote:

```cpp
#define AEHA_CUSTOMER     0x2002
#define AEHA_DATA0        0x8
#define AEHA_BYTE3        0x00
#define AEHA_T_US         432
#define AEHA_REPEAT_FRAME 0
```

The 21-entry `KEYS` table is indexed by `row * 3 + column`. Its order must match the physical keypad wiring. For AEHA, each 16-bit key value is sent as the command low byte and parity/high byte. The Power code `0xBD3D` produces `02 20 80 00 3D BD`.

By default, AEHA repeats the full frame every 130 ms, start-to-start, matching the captured remote. Set `AEHA_REPEAT_FRAME` to `1` only if your original remote uses an AEHA-specific short repeat frame. Set `IR_MIN_FRAMES` to `2` to reproduce the captured “short press = two frames” behavior.

### 7.5 Firmware flow

1. Scan the keypad; if no key is down, enter Power-down sleep.
2. Wake on a pin-change interrupt, wait 20 ms for debounce, and scan again.
3. Call `IR_NEW_PRESS()` (used by RC5 to toggle its toggle bit) and transmit the selected key.
4. While held, repeat according to the protocol: NEC uses its short repeat frame; the other included encoders repeat their frame by default. Stop at `MAX_HOLD_MS`.
5. If the key remains stuck, return to deep sleep to avoid draining the battery.
6. Return to sleep after the key is released and the minimum-frame requirement is satisfied.

### 7.6 Exporting HEX and programming with USBasp

In Arduino IDE, choose ATtiny44(a), internal 4 MHz, then use **Sketch → Export Compiled Binary**. Program the HEX without `with_bootloader`.

The original guide records these fuse values:

| Fuse | Value | Notes |
|---|---|---|
| LFUSE | `0x62` | Internal 8 MHz oscillator with CKDIV8; starts at 1 MHz before firmware changes the prescaler |
| HFUSE | `0xDF` | SPI programming enabled; RESET retained; BOD disabled |
| EFUSE | `0xFF` | SELFPRGEN disabled |

The firmware changes the clock prescaler using `CLKPR`. Do **not** enable CKOUT because it outputs the clock on PB2, which is also the IR output. Never program `RSTDISBL` or enable `DWEN` unless you understand the recovery implications; doing so can disable ordinary ISP access.

Example command:

```sh
avrdude -c usbasp -p t44 -B 10 \
  -U flash:w:IRremoteClone.ino.hex:i \
  -U lfuse:w:0x62:m -U hfuse:w:0xDF:m -U efuse:w:0xFF:m
```

`-B 10` slows the ISP clock. A graphical tool such as AVRDUDESS can be used with the same values. Verify fuse values after programming if needed.

During programming, do not press keypad buttons because some matrix rows share the ISP lines. Power the board from the USBasp **or** the battery, not both at once. Lock bits are optional; enabling them prevents flash readback and can only be undone by a chip erase.

### 7.7 Carrier generation and mark synchronization

The earlier implementation allowed Timer0 to free-run, so the first carrier transition could occur at an arbitrary point in the carrier cycle. This could vary mark duration by up to roughly 14 µs and truncate the first/last pulse.

The updated carrier engine synchronizes each mark using a force-compare operation (`FOC0A`), resets the timer phase, emits an integer number of full carrier pulses, and ends immediately after the last pulse. Pulse count is calculated at compile time.

For AEHA at 4 MHz and a carrier near 37.04 kHz:

| Mark | Pulses | Burst duration | Shorter than nominal |
|---|---:|---:|---:|
| Data/end mark, nominal 432 µs | 16 | 418 µs | 14 µs |
| Header mark, nominal 3456 µs | 128 | 3442 µs | 14 µs |

The original hardware test notes report that this synchronization fixed range instability. An ESP32 receiver test reportedly decoded valid frames at 0.5, 1, 1.5, and 2 m, with the library identifying about 88% of clone frames as Panasonic. These results refer to the tested earlier AEHA implementation; repeat them on your own board.

Approximate carrier results from the design notes:

| Protocol | Target carrier | Actual at 4 MHz | Actual at 8 MHz | Example mark at 4 MHz |
|---|---:|---:|---:|---|
| NEC | 38 kHz | 37.74 kHz | 38.10 kHz | 560 µs → 21 pulses / 543 µs |
| Samsung | 38 kHz | 37.74 kHz | 38.10 kHz | 560 µs → 21 pulses / 543 µs |
| AEHA | 37 kHz | 37.04 kHz | 37.04 kHz | 432 µs → 16 pulses / 418 µs |
| Sony SIRC | 40 kHz | 40.00 kHz | 40.00 kHz | 600 µs → 24 pulses / 587 µs |
| RC5 | 36 kHz | 35.71 kHz | 36.04 kHz | 889 µs → 32 pulses / 882 µs |

`IR_MARK(us)` requires a compile-time constant because the pulse count is calculated by the compiler. Mark bursts may be slightly shorter than nominal because only whole carrier pulses are emitted; the bit period is maintained by the protocol timing logic.

### 7.8 Supported protocol encoders

| Protocol | Frame summary | Repeat behavior | Validation status |
|---|---|---|---|
| NEC | 32 bits: address, inverse address, command, inverse command; LSB first | Short repeat frame, typically 108 ms | Not tested with a real target |
| AEHA | 48 bits / 6 bytes, LSB first, `T = 432 µs` | Full frame every 130 ms for this remote | Timings/data derived from original capture; earlier version tested |
| Samsung32 | 32 bits, Samsung-style address/command fields | Full-frame repeat, approximately 110 ms | Not tested |
| Sony SIRC | 12, 15, or 20 bits; command first, LSB first | Typically 45 ms; minimum 3 frames | Not tested |
| Philips RC5/RC5X | 14-bit Manchester encoding; half-bit about 889 µs | Approximately 114 ms | Not tested |

Configure addresses and key codes in `Config.h`. NEC and Samsung use `{address, command}` entries. AEHA uses 16-bit key values. Sony uses `SIRC_BITS` and `SIRC_ADDRESS`; RC5 uses `RC5_ADDRESS`. The current NEC, Samsung, SIRC, and RC5 key entries are placeholders (commands 0–20) and must be replaced with values captured from the intended remote.

### 7.9 Adding a protocol

1. Add a protocol ID and a key table in `Config.h`.
2. Define carrier frequency, repeat period, minimum frames, and timing constants in `IRProtocols.h`.
3. Implement a frame encoder. Pulse-distance protocols can reuse the shared `irSendPD()` engine; other formats can emit `IR_MARK(...)` and `IR_SPACE(...)` sequences.
4. Return the actual frame duration in milliseconds so repeat timing can be maintained.
5. If the protocol has a toggle bit, implement `IR_NEW_PRESS()`. If it uses a distinct repeat frame, define `IR_HAS_OWN_REPEAT` and implement `irSendRepeat()`.

`IR_MARK()` and `IR_SPACE()` arguments must be compile-time constants. For variable timing, use a finite set of constant branches.

### 7.10 Software simulation status

The original development notes report that syntax was checked for all five protocols at 4 MHz and 8 MHz using substitute AVR headers. Mark/space sequences were simulated for all 21 keys and decoded with independent protocol decoders. The AEHA Power frame decoded to `02 20 80 00 3D BD`, parity was consistent, and RC5 toggle behavior changed on each new press.

The notes also explicitly state that the multi-protocol v2 firmware had not yet been run on the actual ATtiny44A and flash/RAM usage had not been measured with the AVR compiler. NEC, Samsung, Sony, and RC5 still require testing with a real receiver/device.

## 8. Bring-up checklist

1. Assemble the MCU and ISP header first; verify supply and programming.
2. Check for IR output using a phone camera that can see infrared (some cameras have strong IR filters).
3. Capture the clone with an ESP32 and `IRrecvDumpV2`. For the original AEHA Power key, expect `02 20 80 00 3D BD`, `T ≈ 432 µs`, and about 130 ms start-to-start repeat.
4. Compare usable range and angle with the original remote.
5. Measure sleep current with a microamp-capable meter; the design target is a few microamps or less.
6. Test operation at 2.0–2.2 V using a current-limited bench supply.
7. Test held-key behavior and the stuck-key safety limit.

## 9. Troubleshooting

| Symptom | Possible cause | Suggested check |
|---|---|---|
| No IR output | LED driver or PB2 wiring | Check LED polarity and use a camera/photodetector |
| Target does not respond | Wrong protocol, carrier, or key data | Compare raw captures; check `PROTO` and `KEYS` |
| Works only at very short range | Low LED pulse current or weak battery | Check current within LED limits and add bulk capacitance |
| MCU resets during transmission | Supply droop from LED pulses | Improve bulk capacitance/ESR or reduce pulse current |
| High sleep current | BOD enabled, floating pin, stuck key | Disable BOD as appropriate; ensure pins have defined states |
| Wrong key code | Wrong table order or byte order | Verify row/column index and LSB-first encoding |
| AEHA frame ignored | Bytes 2/3, `T`, or carrier differs | Compare against `02 20 B2 B3 command parity`; tune AEHA options and measure carrier |
| ISP programming fails | Key held or excessive loading on PA4–PA6 | Release keys and check ISP wiring/slow-SCK setting |
| Simultaneous keys misread | Matrix ghosting | Add one diode per key if multi-key presses are required |
| Short press repeats too much | Gap/repeat timing configuration | Adjust `GAP_AFTER_FRAME` and compare with original |
| Timing inaccurate | Wrong clock/F_CPU or fuse setting | Compile at 4 MHz and inspect PB2 with a logic analyzer/scope |

## 10. Possible extensions

- **More keys:** The ten matrix GPIOs can support up to 25 keys in a 5 × 5 matrix with firmware changes. More keys require using RESET as I/O (which needs high-voltage programming for recovery), more complex charlieplexing, or a larger MCU such as ATtiny1614/816.
- **Raw protocol mode:** For an unknown protocol, store raw mark/space timings in `PROGMEM` and replay them with the measured carrier. The 4 KB flash and 256-byte RAM constrain the size.
- **More protocol encoders:** JVC and Sharp may be added using similar pulse-distance techniques; RC6 is more complex. Only one protocol is compiled in at a time to control flash/RAM usage. Runtime selection of multiple protocols increases code size.
- **Larger MCU:** ATtiny1614 or ATtiny816 can support more keys or larger tables.

## 11. Safety and limitations

- Never connect an IR LED directly to an MCU pin without an appropriate current-limiting/driver arrangement.
- Stay within the LED's pulsed-current and duty-cycle ratings and the transistor's limits.
- PA4–PA6 are shared with ISP; do not press keys during programming.
- Do not use PB3/RESET as a matrix pin unless you deliberately redesign programming/recovery.
- Current, timing, range, and battery-life values are estimates or measurements from a specific prototype; validate them on the actual board and components.

## 12. Final checklist

- [x] Original remote identified as AEHA with customer code `0x2002`.
- [x] Original Power-key frame captured and decoded as `02 20 80 00 3D BD`.
- [x] Original timing measured at approximately `T = 432 µs`, with full-frame repeat every 130 ms.
- [ ] Verify the `KEYS` table order matches the actual keypad wiring.
- [ ] Test all 21 physical keys and confirm each index.
- [ ] Test the multi-protocol v2 firmware on an ATtiny44A board.
- [ ] Select `PROTO`, configure `KEYS`, set `HARDWARE_TEST = 0`, and choose an appropriate `MAX_HOLD_MS`.
- [ ] For any non-AEHA protocol, verify address, key map, repeat period, and minimum frame count against a real remote.
- [ ] Compile at 4 MHz and verify fuse settings (`0x62 / 0xDF / 0xFF` as documented).
- [ ] Verify the LED driver and bulk capacitor.
- [ ] Compare the emitted signal with the original using an IR receiver.
- [ ] Measure sleep current, operating range, and low-battery behavior.
