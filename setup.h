#ifndef SETUP_H
#define SETUP_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * CPU CLOCK FREQUENCY (DIGISPARK DEFAULT = 16.5 MHz / TRINKET = 16 MHz)
 * ============================================================================
 * If defined by the Arduino environment (via board selection), it is used
 * automatically. Otherwise, defaults to 16.5 MHz for Digispark.
 */
#ifndef F_CPU
#define F_CPU 16500000UL
#endif

/* ============================================================================
 * HARDWARE PIN CONFIGURATION (ATtiny85 on Digispark / Trinket board)
 * ============================================================================
 * Physical pin assignments on ATtiny85 (DIP-8 / SOIC-8):
 *  - Pin 1 : PB5 / P5 -> RESET (used by USBasp programmer)
 *  - Pin 2 : PB3 / P3 -> DIT paddle (active low / GND, internal pull-up enabled)
 *  - Pin 3 : PB4 / P4 -> DAH paddle (active low / GND, internal pull-up enabled)
 *  - Pin 4 : GND
 *  - Pin 5 : PB0 / P0 -> TX Key output line
 *  - Pin 6 : PB1 / P1 -> Sidetone audio output (and on-board LED)
 *  - Pin 7 : PB2 / P2 -> Command / Speed Button (active low / GND, internal pull-up)
 *  - Pin 8 : VCC (+5V)
 */
#define KEYDDR                  DDRB
#define KEYPORT                 PORTB
#define KEYINP                  PINB
#define DITPIN                  3       // PB3 (P3)
#define DAHPIN                  4       // PB4 (P4)

#define OUTDDR                  DDRB
#define OUTPORT                 PORTB
#define OUTPIN                  0       // PB0 (P0) - TX Key output

#define STDDR                   DDRB
#define STPORT                  PORTB
#define STPIN                   1       // PB1 (P1) - Sidetone audio / LED output

#define BTNDDR                  DDRB
#define BTNPORT                 PORTB
#define BTNINP                  PINB
#define BTNPIN                  2       // PB2 (P2) - Command button

/* ============================================================================
 * TX KEY OUTPUT POLARITY (Pin PB0 / P0)
 * ============================================================================
 * true  : Positive keying (Active HIGH):
 *         - Key-Down (active) = +VCC (HIGH)
 *         - Key-Up (idle)     = GND (LOW)
 *         (Recommended for NPN transistors, N-channel MOSFETs, optocouplers)
 * false : Negative keying (Active LOW):
 *         - Key-Down (active) = GND (LOW)
 *         - Key-Up (idle)     = +VCC (HIGH)
 *         (Recommended for direct cathode/grid keying or inverted logic)
 */
#define positive_key_down       true

/* ============================================================================
 * DEFAULT STARTUP / RESET VALUES
 * ============================================================================
 */

// 1. CW Speed parameters (WPM - Words Per Minute):
#define DEFWPM                  15      // Default speed at startup / reset (WPM)
#define MINWPM                  5       // Minimum selectable speed (WPM)
#define MAXWPM                  50      // Maximum selectable speed (WPM)

// 2. Sidetone audio pitch parameters (Hz):
#define DEFFREQ                 700     // Default sidetone pitch (700 Hz)
#define MINFREQ                 400     // Minimum selectable pitch (400 Hz)
#define MAXFREQ                 1500    // Maximum selectable pitch (1500 Hz)

// 3. Default keyer mode:
// Options: IAMBICA, IAMBICB, ULTIMATIC, DAHPRIO
#define DEFAULT_KEYER_MODE      IAMBICA

// 4. Feature default states:
#define DEFAULT_SIDETONE_ON     true    // true = Sidetone audio enabled at startup
#define DEFAULT_TX_KEY_ON       true    // true = TX transmitter keying enabled at startup
#define DEFAULT_PADDLE_SWAP     false   // true = Swapped paddles (DIT on P4, DAH on P3); false = normal (DIT P3, DAH P4)
#define DEFAULT_CONFLOCK        false   // true = Configuration commands locked at startup (toggle with '0')
#define DEFAULT_FARNSWORTH      0       // 0 = Farnsworth pause disabled by default

// 5. Default automatic beacon interval:
#define DEFAULT_BEACON_INTERVAL 0       // Repetition interval in seconds (0 = beacon disabled)

// 6. Default messages stored in EEPROM (Macro 1 and Macro 2):
#define DEFAULT_MACRO_1         "CQ CQ CQ DE YO K"
#define DEFAULT_MACRO_2         "BEACON DE YO/B"

/* ============================================================================
 * SYSTEM TIMING (FSM HEARTBEAT)
 * ============================================================================
 */
#define YACKBEAT                5       // Heartbeat interval in milliseconds (5 ms)

/* ============================================================================
 * POWER SAVE MODE (POWERSAVE)
 * ============================================================================
 * Chip enters Power-Down sleep after inactivity timeout and wakes up
 * immediately upon touching paddles or the command button.
 */
#define POWERSAVE                       // Comment out to disable auto-sleep
#define PSTIME                  30      // Seconds of inactivity before sleep
#define PWRWAKE                 ((1<<PCINT3) | (1<<PCINT4) | (1<<PCINT2)) // PB3, PB4, PB2

/* ============================================================================
 * VARIOUS TIMEOUTS (Seconds)
 * ============================================================================
 */
#define TUNEDURATION            20      // Continuous tuning keydown timeout (seconds)
#define DEFTIMEOUT              5       // Command entry & message recording timeout (seconds)
#define MACTIMEOUT              15      // Macro playback timeout (seconds)
#define TRAINTIMEOUT            10      // Callsign trainer timeout (seconds)

#define PITCHREPEAT             10      // Number of dits played for pitch adjust
#define FARNSREPEAT             10      // Number of dits played for Farnsworth adjust

/* ============================================================================
 * EEPROM BUFFER SIZES
 * ============================================================================
 */
#define RBSIZE                  100     // Maximum length of each message buffer (characters)

/* ============================================================================
 * PREDEFINED MORSE TEXTS & STRINGS
 * ============================================================================
 */
#define MSG_GREETING            "73"    // Greeting played at power-up
#define MSG_VERSION             "V0.75" // Response to 'V' command
#define MSG_SIGNOFF             "SK"    // Sign-off when exiting command mode
#define MSG_OK                  "R"     // Acknowledge character

#endif // SETUP_H
