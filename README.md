# IRremoteClone

A low-power, multi-protocol infrared remote-control project based on the **ATtiny44A**. The original use case is reproducing a legacy Panasonic-family remote using captured **AEHA/Kaseikyo** frames. The firmware also includes configurable encoders for NEC, Samsung32, Sony SIRC, and Philips RC5/RC5X.

> **Hardware schematic:** [`docs/IRremote_Sch.pdf`](docs/IRremote.pdf)  
> **Detailed technical guide:** [`docs/IR_Remote_Clone_Guide.md`](docs/IR_Remote_Clone_Guide_EN.md)  

## Highlights

- ATtiny44A (SOIC-14), intended for Arduino IDE + ATTinyCore.
- 3 × 7 keypad matrix for up to 21 keys using 10 GPIO pins.
- Timer0 hardware output-compare carrier generation on `PB2 / OC0A`.
- Power-down sleep and pin-change-interrupt wake-up.
- Protocol selection at compile time to keep the firmware small.
- Key-hold handling and configurable minimum frame count.
- IR LED driven through an NPN transistor rather than directly from an MCU pin.

## Hardware at a glance

| Item | Design choice |
|---|---|
| MCU | ATtiny44A-SSU |
| Clock | Internal 4 MHz |
| Keypad | 3 columns × 7 rows |
| IR emitter | 940 nm IR LED + BC337 (or equivalent NPN) |
| Main supply | 2 × AAA cells or CR2032 |
| IR output | `PB2 / OC0A` |
| ISP | Standard 6-pin ISP; PA4–PA6 are shared with keypad rows |

The complete circuit diagram is in [`docs/IRremote.pdf`](docs/IRremote.pdf). The English guide includes the pin map, BOM, current and battery-life estimates, programming/fuse notes, protocol details, testing steps, and troubleshooting.

## Supported protocols

Choose one protocol in `Config.h` at compile time.

| Protocol | Frame format | Default / validation notes |
|---|---|---|
| **AEHA / Panasonic Kaseikyo family** | 48-bit (6 bytes), LSB first | Default. Original remote customer code `0x2002`; Power frame captured as `02 20 80 00 3D BD`; full frame repeats every 130 ms in the captured remote |
| NEC | 32-bit | Encoder included; key/address values need configuration and hardware validation |
| Samsung32 | 32-bit | Encoder included; key/address values need configuration and hardware validation |
| Sony SIRC | 12 / 15 / 20-bit | Encoder included; configure bit length, address, and key data |
| Philips RC5 / RC5X | 14-bit Manchester | Encoder included; configure address and command data |

**Validation status matters:** AEHA timing and frame data were derived from captures of the original remote, and an earlier single-protocol AEHA version was tested on hardware. The multi-protocol firmware was syntax-checked and simulated on a computer, but the project notes do not confirm a complete ATtiny44A hardware test of that version. NEC, Samsung, Sony SIRC, and RC5 are not documented as tested with a real target device. Treat them as starting points that need verification.

## Repository layout

```text
IRremoteClone/
├── IRremoteClone.ino
├── Config.h
├── IRProtocols.h
├── LICENSE
├── README.md
└── docs/
    ├── IRremote_Sch.pdf
    ├── IRremote_Sch.jpg
    ├── IRremote.jpg
    └── IR_Remote_Clone_Guide.md
```

- `IRremoteClone.ino` — keypad scan, sleep/wake, key handling, and repeat logic.
- `Config.h` — pin mapping, protocol selection, timing options, and key tables.
- `IRProtocols.h` — Timer0 carrier engine and protocol encoders.
- `docs/IRremote_Sch.xxx` — hardware schematic.
- `docs/IRremote.jpg` — image of the test remote control.
- `docs/IR_Remote_Clone_Guide.md` — build, configuration, and test guide.

## Build and flash

1. Install the Arduino IDE.
2. Install [ATTinyCore](https://github.com/SpenceKonde/ATTinyCore).
3. Open `IRremoteClone.ino` with the supporting headers in the same sketch directory.
4. Select **ATtiny24/44/84 (No bootloader)**, chip **ATtiny44(a)**, and **4 MHz internal clock**. Disable BOD for the low-power configuration if appropriate.
5. In `Config.h`, select `PROTO` and configure the address and key table for your target remote.
6. Connect an ISP programmer and use **Upload Using Programmer**, or export the compiled HEX and program it with your preferred ISP tool.

The firmware relies on the compile-time `F_CPU` value supplied by the IDE. Select the intended 4 MHz clock before compiling. The design notes document fuse values `LFUSE = 0x62`, `HFUSE = 0xDF`, `EFUSE = 0xFF`; read the detailed guide before changing fuses. **Do not enable CKOUT** because it shares PB2 with the IR output, and do not disable RESET if you need normal ISP recovery.

### Default AEHA configuration

```cpp
#define PROTO          PROTO_AEHA
#define AEHA_CUSTOMER   0x2002
#define AEHA_DATA0      0x8
#define AEHA_BYTE3      0x00
#define AEHA_T_US       432
#define AEHA_REPEAT_FRAME 0
```

The Power-key code `0xBD3D` produces the captured frame `02 20 80 00 3D BD`. The key table is indexed by `row * 3 + column`, so its order must match the physical keypad wiring. The other protocol key tables contain placeholder values and must be customized.

## Hardware and power notes

- Use a transistor driver for the IR LED and select the series resistor according to supply voltage, LED forward voltage, desired range, pulse current, and datasheet limits.
- The guide describes 27 Ω for the two-AAA design and 68 Ω for a short-range CR2032 option; do not assume either value is suitable for a different LED or circuit.
- Add 100 nF decoupling close to the MCU and 100–470 µF bulk capacitance near the LED supply path.
- Rows PA4–PA6 are also ISP SCK/MISO/MOSI. Release all buttons while programming.
- A diode-less matrix may ghost during simultaneous key presses; add one signal diode per key if multi-key operation is required.

## Test and troubleshooting

For the original Panasonic-family remote, capture the clone using an ESP32 IR receiver and compare the raw timings. The Power-key frame should decode to `02 20 80 00 3D BD`, with `T ≈ 432 µs` and a full-frame repeat period near 130 ms. Measure carrier frequency with appropriate instrumentation; a demodulating receiver does not reveal the carrier waveform.

See the [English technical guide](docs/IR_Remote_Clone_Guide_EN.md) for the complete bring-up checklist, power measurements, known limitations, programming instructions, and troubleshooting table.

## Contributing

Bug reports, verified key maps, timing corrections, hardware test results, and additional protocol implementations are welcome. Please include MCU/clock settings, selected protocol, target device, and raw capture data when reporting an issue.

## License

Released under the [MIT License](LICENSE).
