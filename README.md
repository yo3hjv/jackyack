# jackyack
A cheap open source CW keyer (ATtiny*5)
This is a tiny, cheap, and open source CW keyer. It is 
described in detail on my blog:
http://blog.templaro.com/a-tiny-and-open-source-cw-keyer/
 
 ### About this fork:

 # TinyKeyer (JackYack v0.75 - Arduino IDE Refactoring)

A versatile, compact open-source CW (Morse Code) keyer for the **Atmel AVR ATtiny85** microcontroller (specifically configured for **Digispark** and **Adafruit Trinket** compatible boards at 16.5 MHz / 16 MHz).

Original library and application: **JackYack (v0.75)**  
Original Authors: **Jan Lategahn (DK3LJ)** and **Jack Welch (AI4SV)**  
Version: **0.75** (Refactored edition for Arduino IDE)

---

## 1. Overview of the Refactoring

This project is a complete refactoring and modernization of the original JackYack codebase to allow direct compilation and programming within the **Arduino IDE** using standard AVR ISP programmers such as the **USBasp**.

While maintaining 100% backward functional compatibility with the original v0.75 feature set, this refactoring introduces significant architectural, modular, and hardware-timing improvements.

---

## 2. Key Refactoring Adaptations

### A. Modular Architecture for Arduino IDE
* **Separation of Concerns**: The monolithic source files (`main.c` and `yack.c`) were separated into clear, distinct modules:
  * `TinyKeyer.ino`: Standard Arduino sketch containing `setup()` and `loop()` lifecycle handlers.
  * `setup.h`: Centralized configuration header dedicated exclusively to user-definable hardware pins, clock speed, operational defaults, and timeouts.
  * `yack.h` / `yack.cpp`: Core keyer library implementing the IAMBIC finite state machine (FSM), Morse translation table in Flash (`PROGMEM`), CTC audio tone generation, and EEPROM persistence.
  * `commands.h` / `commands.cpp`: High-level interactive command-mode functions, pitch adjust, Farnsworth pause, callsign trainer, and beacon functionality.
* **C++ Compilation Cleanliness**: Fixed array boundary constraints (`spechar[]`), added C/C++ linkage guards (`extern "C"`), and resolved type alias conflicts with Arduino core types (`byte` and `word`).

### B. Hardware Timing Adaptations for 16.5 MHz / 16 MHz Clock
The original JackYack codebase was designed for a 1 MHz internal clock. Running on an ATtiny85 with internal PLL (Digispark default at 16.5 MHz or Trinket at 16 MHz) required key timer adaptations:
1. **Timer 0 (Sidetone Audio Generator on PB1 / OC0B)**:
   * With the original prescaler of 8, calculating CTC compare values at 16.5 MHz exceeded the 8-bit capacity of `OCR0A` (`CTCVAL(700) = 1472 > 255`).
   * The prescaler was scaled to **256** (`TCCR0B = (1 << CS02)`). This brings the CTC register values comfortably into an 8-bit range (20 to 79) for clean, adjustable audio tones between 400 Hz and 1500 Hz.
2. **Timer 1 (System Heartbeat - 5 ms Cadence)**:
   * The system relies on a 5 ms heartbeat (`YACKBEAT = 5`) for FSM timing.
   * Timer 1 prescaler is configured to **512** (`TCCR1 = (1 << CTC1) | (1 << CS13) | (1 << CS11)`).
   * At 16.5 MHz, `OCR1C = 160` generates an exact 4.996 ms period (error < 0.08%).
   * At 16 MHz, `OCR1C = 155` generates a 4.992 ms period.
3. **Arduino Core Timer Isolation**:
   * Arduino's default core initializes Timer 0 interrupts for `millis()` and `micros()`.
   * In `yackinit()`, timer interrupts are disabled (`TIMSK = 0;`) to eliminate timer jitter, interrupt latency, and interference with the keyer's CTC audio and FSM timing.

### C. Configurable TX Keying Polarity (`positive_key_down`)
* Added explicit configuration in `setup.h`:
  * `positive_key_down = true`: **Active HIGH keying**. Pin PB0 goes to **+5V (HIGH)** during key-down (transmitting) and rests at **0V (GND)** during key-up (idle). Ideal for driving NPN transistors, N-channel MOSFETs, or optocouplers.
  * `positive_key_down = false`: **Active LOW keying**. Pin PB0 goes to **0V (GND)** during key-down and rests at **+5V (HIGH)** during key-up. Ideal for cathode/grid keying or active-low logic.
* **Glitch-Free Power-On**: Output pin PB0 is initialized immediately to its appropriate idle voltage at startup.

### D. Comprehensive Centralized Defaults in `setup.h`
All startup and reset parameters are now accessible and cleanly documented in `setup.h`:
* Default speed: `DEFWPM` (15 WPM)
* Default sidetone pitch: `DEFFREQ` (700 Hz)
* Default keyer mode: `DEFAULT_KEYER_MODE` (`IAMBICA`)
* Default feature flags:
  * `DEFAULT_SIDETONE_ON` (`true`)
  * `DEFAULT_TX_KEY_ON` (`true`)
  * `DEFAULT_PADDLE_SWAP` (`false`)
  * `DEFAULT_CONFLOCK` (`false`)
  * `DEFAULT_FARNSWORTH` (`0`)
  * `DEFAULT_BEACON_INTERVAL` (`0` = disabled at boot)
* Default macro strings in EEPROM:
  * `DEFAULT_MACRO_1` (`"CQ CQ CQ DE YO K"`)
  * `DEFAULT_MACRO_2` (`"BEACON DE YO/B"`)

### E. Standardized English Documentation
* All source code comments across all project files were standardized into clean, professional English.

---

## 3. Hardware Pin Mapping (ATtiny85 / Digispark)

| Physical Pin | AVR Port | Digispark Label | Function | Circuit Details |
| :---: | :---: | :---: | :--- | :--- |
| **Pin 1** | `PB5` | **P5** | `RESET` | Connected to USBasp ISP programmer `RESET` line |
| **Pin 2** | `PB3` | **P3** | `DIT Paddle` | Connected to GND when pressed; internal pull-up enabled |
| **Pin 3** | `PB4` | **P4** | `DAH Paddle` | Connected to GND when pressed; internal pull-up enabled |
| **Pin 4** | `GND` | **GND** | `Ground` | System Ground (0V) |
| **Pin 5** | `PB0` | **P0** | `TX Key Output` | Drives transmitter keying line (MOSI for ISP) |
| **Pin 6** | `PB1` | **P1** | `Sidetone Output` | Square wave audio output / on-board LED (MISO for ISP) |
| **Pin 7** | `PB2` | **P2** | `Command Button`| Connected to GND when pressed; internal pull-up enabled (SCK for ISP) |
| **Pin 8** | `VCC` | **5V / VCC** | `Supply Voltage`| +3.3V to +5V DC |

---

## 4. Keyer Operations & Commands

### Changing Speed On-the-Fly
* Press and **hold the Command Button** while operating the paddles:
  * Tap **DIT** to decrease speed.
  * Tap **DAH** to increase speed.
  * The keyer plays an alternating sequence of dots and dashes while adjusting speed, without keying the transmitter.

### Command Mode
* Press the **Command Button** once (without touching the paddles).
* The keyer sends `?` in Morse code to confirm Command Mode.
* Transmitter keying is disabled and sidetone is forced ON while in Command Mode.
* Key in any of the single-character Morse commands below:

| Command | Action |
| :---: | :--- |
| **`A`** | Set keyer to **IAMBIC A** mode. (Acknowledged with `R`). |
| **`B`** | Set keyer to **IAMBIC B** mode. (Acknowledged with `R`). |
| **`L`** | Set keyer to **ULTIMATIC** mode. (Acknowledged with `R`). |
| **`D`** | Set keyer to **DAH Priority** mode. (Acknowledged with `R`). |
| **`X`** | **Swap paddles** (DIT > DAH). (Acknowledged with `R`). |
| **`S`** | Toggle **Sidetone** ON/OFF for regular operation. |
| **`K`** | Toggle **TX Transmitter Keying** output ON/OFF. |
| **`I`** | Toggle **TX Output Inversion** (Active HIGH > Active LOW). |
| **`P`** | **Pitch adjust mode**: Keyer sends continuous `E`s. Tap DIT/DAH to raise/lower frequency. |
| **`Z`** | **Farnsworth pause adjust mode**. |
| **`W`** | **Query speed**: Keyer sends current speed in WPM. |
| **`V`** | **Query version**: Keyer sends software version (`V0.75`). |
| **`U`** | **Tune mode**: Continuous keydown for 20 seconds. Abort with any paddle or button. |
| **`1`** | **Record Macro 1**: Keyer sends `1`; key your message (up to 100 chars). Auto-saves to EEPROM after 5s pause. |
| **`2`** | **Record Macro 2**: Keyer sends `2`; key your message (up to 100 chars). Auto-saves to EEPROM after 5s pause. |
| **`E`** | **Play Macro 1**. |
| **`T`** | **Play Macro 2**. |
| **`N`** | **Automatic Beacon**: Keyer sends `N`; key a number between 1 and 9999 (interval in seconds). Repeats Macro 2. Set 0 to disable. |
| **`C`** | **Callsign Trainer**: Keyer plays random callsigns; user echoes back on paddles. |
| **`0`** | **Lock / Unlock configuration**: Prevents accidental changes to keying mode and settings. |
| **`R`** | **Factory Reset**: Restores all settings to defaults specified in `setup.h`. |

* Press the Command Button again to exit Command Mode. The keyer sends `SK` to confirm return to normal operation.

---

## 5. Compiling and Flashing in Arduino IDE

1. Open `TinyKeyer.ino` in **Arduino IDE**.
2. Select the target board:
   * **Tools** > **Board** > **Adafruit AVR Boards** > **Adafruit Trinket (ATtiny85 @ 16MHz)**  
     *(or using ATTinyCore: ATtiny25/45/85 > Chip: ATtiny85, Clock: 16.5MHz PLL / 16MHz PLL)*.
3. Select the programmer:
   * **Tools** > **Programmer** > **USBasp**.
4. Connect the USBasp programmer to the ATtiny85 ISP pins (MOSI=P0, MISO=P1, SCK=P2, RESET=P5, 5V, GND).
   * *Important*: Disconnect any external loads (speakers/buzzers) from P1 during ISP programming so the MISO line is not pulled down.
5. Upload the code:
   * Click **Sketch** > **Upload Using Programmer** (`Ctrl + Shift + U`).

---

## 6. License
This program is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License (GPLv3)** as published by the Free Software Foundation.

