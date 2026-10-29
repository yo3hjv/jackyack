#ifndef YACK_H
#define YACK_H

#include "setup.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// TYPE DEFINITIONS (Arduino / C compatibility)
// ============================================================================
#ifndef byte
typedef uint8_t  byte;
#endif

#ifndef word
typedef uint16_t word;
#endif

// ============================================================================
// PERMANENT STATUS BITS (yackflags - Stored in EEPROM)
// ============================================================================
#define NOTUSED1        0b00000001  // Reserved
#define CONFLOCK        0b00000010  // Configuration locked down
#define MODE            0b00001100  // 2 bits to define keyer mode
#define SIDETONE        0b00010000  // Sidetone generator enabled
#define TXKEY           0b00100000  // Transmitter keying output enabled
#define TXINV           0b01000000  // TX key line inverted (Active LOW)
#define PDLSWAP         0b10000000  // DIT and DAH paddles swapped

#define IAMBICA         0b00000000  // IAMBIC A mode
#define IAMBICB         0b00000100  // IAMBIC B mode
#define ULTIMATIC       0b00001000  // Ultimatic mode
#define DAHPRIO         0b00001100  // DAH priority mode

#if positive_key_down
#define DEFAULT_TXINV_FLAG      0
#else
#define DEFAULT_TXINV_FLAG      TXINV
#endif

#define DEFAULT_SIDETONE_FLAG   (DEFAULT_SIDETONE_ON ? SIDETONE : 0)
#define DEFAULT_TXKEY_FLAG      (DEFAULT_TX_KEY_ON ? TXKEY : 0)
#define DEFAULT_SWAP_FLAG       (DEFAULT_PADDLE_SWAP ? PDLSWAP : 0)
#define DEFAULT_LOCK_FLAG       (DEFAULT_CONFLOCK ? CONFLOCK : 0)

#define FLAGDEFAULT             (DEFAULT_KEYER_MODE | DEFAULT_TXKEY_FLAG | DEFAULT_SIDETONE_FLAG | DEFAULT_TXINV_FLAG | DEFAULT_SWAP_FLAG | DEFAULT_LOCK_FLAG)

// ============================================================================
// VOLATILE STATUS BITS (volflags - Not stored in EEPROM)
// ============================================================================
#define DITLATCH        0b00000001  // DIT paddle was closed
#define DAHLATCH        0b00000010  // DAH paddle was closed
#define SQUEEZED        0b00000011  // Both DIT and DAH closed simultaneously
#define DIRTYFLAG       0b00000100  // Configuration changed and needs saving to EEPROM
#define CKLATCH         0b00001000  // Command button was pressed
#define VSCOPY          0b00110000  // Temporary copies of SIDETONE and TXKEY flags

// ============================================================================
// TIMING AND HEARTBEAT MACROS
// ============================================================================
#define YACKSECS(n)     ((word)((uint32_t)(n) * (1000 / YACKBEAT))) // Heartbeats in n seconds
#define YACKMS(n)       ((word)((n) / YACKBEAT))                    // Heartbeats in n milliseconds
#define WPMCALC(n)      ((1200 / YACKBEAT) / (n))                   // Number of heartbeats in a dot

#define FARNSWORTH      1
#define WPMSPEED        0
#define MAXFARN         255

// Standard Morse element duration ratios
#define DITLEN          1   // Dot length (1 unit)
#define DAHLEN          3   // Dash length (3 units)
#define IEGLEN          1   // Inter-element gap
#define ICGLEN          3   // Inter-character gap
#define IWGLEN          7   // Inter-word gap

// ============================================================================
// TIMER 0 PRESCALER CALCULATION FOR SIDETONE (PB1 / OC0B)
// ============================================================================
// For high clock frequencies (e.g. 16 / 16.5 MHz), prescaler 256 keeps OCR0A
// within 8-bit limits (below 255) across the full 400 Hz .. 1500 Hz range.
#if F_CPU >= 8000000UL
  #define PRESCALE        256UL
  #define TCCR0B_PRESCALE (1 << CS02)
#else
  #define PRESCALE        8UL
  #define TCCR0B_PRESCALE (1 << CS01)
#endif

#define CTCVAL(n)       ((uint8_t)((F_CPU / (2UL * PRESCALE * (uint32_t)(n))) - 1))

#define DEFCTC          CTCVAL(DEFFREQ)
#define MAXCTC          CTCVAL(MAXFREQ) // Minimum CTC value = Maximum frequency
#define MINCTC          CTCVAL(MINFREQ) // Maximum CTC value = Minimum frequency

// ============================================================================
// TIMER 1 PRESCALER CALCULATION FOR HEARTBEAT (5 ms)
// ============================================================================
#if F_CPU >= 12000000UL
  #define T1_PRESCALE_VAL 512UL
  #define T1_TCCR1_BITS   ((1 << CTC1) | (1 << CS13) | (1 << CS11)) // Prescaler 512
#elif F_CPU >= 4000000UL
  #define T1_PRESCALE_VAL 512UL
  #define T1_TCCR1_BITS   ((1 << CTC1) | (1 << CS13) | (1 << CS11))
#else
  #define T1_PRESCALE_VAL 64UL
  #define T1_TCCR1_BITS   ((1 << CTC1) | 0b00000111)                // Prescaler 64
#endif

#define T1_OCR1C_VAL      ((uint8_t)(((F_CPU / 1000UL) * YACKBEAT) / T1_PRESCALE_VAL - 1))

// ============================================================================
// GENERAL DEFINITIONS & CONSTANTS
// ============================================================================
#define MAGPAT          0xA5    // EEPROM magic pattern for valid memory

#define DIT             1
#define DAH             2

#define UP              1
#define DOWN            2

#define ON              1
#define OFF             0

#define RECORD          1
#define PLAY            2

#define READ            1
#define WRITE           2

#ifndef TRUE
#define TRUE            1
#endif

#ifndef FALSE
#define FALSE           0
#endif

#define SETBIT(ADDRESS, BIT)    ((ADDRESS) |= (1 << (BIT)))
#define CLEARBIT(ADDRESS, BIT)  ((ADDRESS) &= ~(1 << (BIT)))

// ============================================================================
// PUBLIC LIBRARY FUNCTION PROTOTYPES
// ============================================================================
void        yackinit(void);
void        yackchar(char c);
void        yackstring(const char *p);
char        yackiambic(byte ctrl);
void        yackpitch(uint8_t dir);
void        yacktune(void);
void        yackmode(uint8_t mode);
void        yackinhibit(uint8_t mode);
void        yackerror(void);
void        yacktoggle(byte flag);
byte        yackflag(byte flag);
void        yackbeat(void);
void        yackmessage(byte function, byte msgnr);
void        yacksave(void);
byte        yackctrlkey(byte mode);
void        yackreset(void);
word        yackuser(byte func, byte nr, word content);
void        yacknumber(word n);
word        yackwpm(void);
void        yackplay(byte i);
void        yackdelay(byte n);
void        yackfarns(void);
void        yackspeed(byte dir, byte mode);

#ifdef POWERSAVE
void        yackpower(byte n);
#endif

#ifdef __cplusplus
}
#endif

#endif // YACK_H
