/*!
 * @file      yack.cpp
 * @brief     CW Keyer engine (Ported for Arduino IDE on ATtiny85 / Digispark)
 * @author    Jan Lategahn DK3LJ / Jack Welch AI4SV - adapted for Digispark 16.5MHz
 */

#include "setup.h"
#include "yack.h"

// Forward declaration of private functions
static void key(byte mode);
static char morsechar(byte buffer);
static void keylatch(void);

// IAMBIC FSM States
enum FSMSTATE {
    IDLE,   //!< Idle, waiting for paddle contact
    KEYED,  //!< Keyed, waiting for duration of current element
    IEG     //!< In Inter-Element-Gap
};

// Module local variables
static byte yackflags;      // Permanent status flags (saved in EEPROM)
static byte volflags = 0;   // Volatile working flags (RAM only)
static word ctcvalue;       // CTC compare value for sidetone pitch
static word wpmcnt;         // Beats per dot for current keying speed
static byte wpm;            // Current speed in WPM
static byte farnsworth;     // Additional Farnsworth pause duration

// ============================================================================
// DATA STORED IN EEPROM
// ============================================================================
byte magic      EEMEM = MAGPAT;                 // 'A5' indicates initialized EEPROM
byte flagstor   EEMEM = FLAGDEFAULT;            // Default feature flags
word ctcstor    EEMEM = DEFCTC;                 // Default pitch (e.g. 700 Hz)
byte wpmstor    EEMEM = DEFWPM;                 // Default speed (e.g. 15 WPM)
byte fwstor     EEMEM = DEFAULT_FARNSWORTH;     // Farnsworth pause from setup.h
word user1      EEMEM = DEFAULT_BEACON_INTERVAL;// Beacon interval from setup.h
word user2      EEMEM = 0;                      // User storage 2

char eebuffer1[RBSIZE] EEMEM = DEFAULT_MACRO_1; // Macro 1 from setup.h
char eebuffer2[RBSIZE] EEMEM = DEFAULT_MACRO_2; // Macro 2 from setup.h

// ============================================================================
// MORSE CODE TABLE IN FLASH (PROGMEM)
// ============================================================================
//! Encoding: Each byte is read from left to right.
//! 0 = dot (DIT), 1 = dash (DAH).
//! Playback stops when the leftmost bit is '1' and all remaining bits are zeros.
const byte morse[] PROGMEM = {
    0b11111100, // 0
    0b01111100, // 1
    0b00111100, // 2
    0b00011100, // 3
    0b00001100, // 4
    0b00000100, // 5
    0b10000100, // 6
    0b11000100, // 7
    0b11100100, // 8
    0b11110100, // 9
    0b01100000, // A
    0b10001000, // B
    0b10101000, // C
    0b10010000, // D
    0b01000000, // E
    0b00101000, // F
    0b11010000, // G
    0b00001000, // H
    0b00100000, // I
    0b01111000, // J
    0b10110000, // K
    0b01001000, // L
    0b11100000, // M
    0b10100000, // N
    0b11110000, // O
    0b01101000, // P
    0b11011000, // Q
    0b01010000, // R
    0b00010000, // S
    0b11000000, // T
    0b00110000, // U
    0b00011000, // V
    0b01110000, // W
    0b10011000, // X
    0b10110000, // Y
    0b11001000, // Z
    0b00110010, // ?
    0b10001100, // =
    0b01010110, // .
    0b00010110, // SK
    0b01010100, // AR
    0b10010100  // /
};

const char spechar[6] PROGMEM = {'?', '=', '.', '#', '$', '/'};

// ============================================================================
// INITIALIZATION AND CONTROL FUNCTIONS
// ============================================================================

void yackreset(void)
{
    ctcvalue   = DEFCTC;
    wpm        = DEFWPM;
    wpmcnt     = (1200 / YACKBEAT) / DEFWPM;
    farnsworth = DEFAULT_FARNSWORTH;
    yackflags  = FLAGDEFAULT;

    volflags |= DIRTYFLAG;
    yacksave();
}

void yackinit(void)
{
    byte magval;

    // 1. Disable timer interrupts initiated by Arduino core (millis/micros)
    // to prevent jitter in CTC audio tone generation and FSM heartbeat.
    TIMSK = 0;

    // 2. Configure output pin directions
    SETBIT(OUTDDR, OUTPIN); // PB0: TX Key output
    SETBIT(STDDR, STPIN);   // PB1: Sidetone audio output

    // 3. Set idle state for TX line according to positive_key_down
#if positive_key_down
    CLEARBIT(OUTPORT, OUTPIN); // Idle = 0V (GND)
#else
    SETBIT(OUTPORT, OUTPIN);   // Idle = +5V (HIGH)
#endif

    CLEARBIT(STPORT, STPIN);   // Sidetone off initially (0V)

    // 4. Enable internal pull-ups for all input pins
    SETBIT(KEYPORT, DITPIN); // PB3
    SETBIT(KEYPORT, DAHPIN); // PB4
    SETBIT(BTNPORT, BTNPIN); // PB2

    // 5. Check and read EEPROM configuration
    magval = eeprom_read_byte(&magic);

    if (magval == MAGPAT)
    {
        ctcvalue   = eeprom_read_word(&ctcstor);
        wpm        = eeprom_read_byte(&wpmstor);
        wpmcnt     = (1200 / YACKBEAT) / wpm;
        farnsworth = eeprom_read_byte(&fwstor);
        yackflags  = eeprom_read_byte(&flagstor);
    }
    else
    {
        yackreset();
    }

    yackinhibit(OFF);

#ifdef POWERSAVE
    PCMSK |= PWRWAKE;    // Pins that wake CPU from sleep (PB3, PB4, PB2)
    GIMSK |= (1 << PCIE);// Enable Pin Change Interrupt
#endif

    // 6. Initialize Timer1 for system heartbeat (5 ms)
    OCR1C = T1_OCR1C_VAL;
    TCCR1 = T1_TCCR1_BITS;
    OCR1A = 1;
}

#ifdef POWERSAVE
ISR(PCINT0_vect)
{
    // Pin change interrupt service routine to wake CPU from sleep mode
}

void yackpower(byte n)
{
    static uint32_t shdntimer = 0;

    if (n)
    {
        if (shdntimer++ >= YACKSECS(PSTIME))
        {
            shdntimer = 0;

            set_sleep_mode(SLEEP_MODE_PWR_DOWN);
            sleep_bod_disable();
            sleep_enable();
            sei();
            sleep_cpu();
            cli();
            sleep_disable();
        }
    }
    else
    {
        shdntimer = 0;
    }
}
#endif

void yacksave(void)
{
    if (volflags & DIRTYFLAG)
    {
        eeprom_write_byte(&magic, MAGPAT);
        eeprom_write_word(&ctcstor, ctcvalue);
        eeprom_write_byte(&wpmstor, wpm);
        eeprom_write_byte(&flagstor, yackflags);
        eeprom_write_byte(&fwstor, farnsworth);

        volflags &= ~DIRTYFLAG;
    }
}

void yackinhibit(byte mode)
{
    if (mode)
    {
        volflags &= ~(TXKEY | SIDETONE);
        volflags |= SIDETONE;
    }
    else
    {
        volflags &= ~(TXKEY | SIDETONE);
        volflags |= (yackflags & (TXKEY | SIDETONE));
        key(UP);
    }
}

word yackuser(byte func, byte nr, word content)
{
    if (func == READ)
    {
        if (nr == 1) return (eeprom_read_word(&user1));
        else if (nr == 2) return (eeprom_read_word(&user2));
    }

    if (func == WRITE)
    {
        if (nr == 1) eeprom_write_word(&user1, content);
        else if (nr == 2) eeprom_write_word(&user2, content);
    }

    return (FALSE);
}

word yackwpm(void)
{
    return wpm;
}

void yackspeed(byte dir, byte mode)
{
    if (mode == FARNSWORTH)
    {
        if ((dir == UP) && (farnsworth > 0))
            farnsworth--;

        if ((dir == DOWN) && (farnsworth < MAXFARN))
            farnsworth++;
    }
    else // WPMSPEED
    {
        if ((dir == UP) && (wpm < MAXWPM))
            wpm++;

        if ((dir == DOWN) && (wpm > MINWPM))
            wpm--;

        wpmcnt = (1200 / YACKBEAT) / wpm;
    }

    volflags |= DIRTYFLAG;

    yackplay(DIT);
    yackdelay(IEGLEN);
    yackplay(DAH);
    yackdelay(ICGLEN);
    yackfarns();
}

void yackbeat(void)
{
    while ((TIFR & (1 << OCF1A)) == 0); // Wait for 5 ms timeout
    TIFR |= (1 << OCF1A);               // Clear output compare flag
}

void yackpitch(byte dir)
{
    if (dir == UP)
        ctcvalue--;
    if (dir == DOWN)
        ctcvalue++;

    if (ctcvalue < MAXCTC)
        ctcvalue = MAXCTC;

    if (ctcvalue > MINCTC)
        ctcvalue = MINCTC;

    volflags |= DIRTYFLAG;
}

void yacktune(void)
{
    word timer = YACKSECS(TUNEDURATION);

    key(DOWN);

    while (timer && (KEYINP & (1 << DITPIN)) && (KEYINP & (1 << DAHPIN)) && !yackctrlkey(TRUE))
    {
        timer--;
        yackbeat();
    }

    key(UP);
}

void yackmode(byte mode)
{
    yackflags &= ~MODE;
    yackflags |= mode;

    volflags |= DIRTYFLAG;
}

byte yackflag(byte flag)
{
    return (yackflags & flag);
}

void yacktoggle(byte flag)
{
    yackflags ^= flag;
    volflags |= DIRTYFLAG;
}

void yackerror(void)
{
    byte i;

    for (i = 0; i < 8; i++)
    {
        yackplay(DIT);
        yackdelay(DITLEN);
    }
    yackdelay(DAHLEN);
}

// ============================================================================
// CW PLAYBACK AND KEYING FUNCTIONS
// ============================================================================

static void key(byte mode)
{
    if (mode == DOWN)
    {
        if (volflags & SIDETONE)
        {
            OCR0A = ctcvalue;
            OCR0B = ctcvalue;

            // Timer0 CTC mode with toggle on pin OC0B (PB1)
            TCCR0A = (1 << COM0B0) | (1 << WGM01);
            TCCR0B = TCCR0B_PRESCALE;
        }

        if (volflags & TXKEY)
        {
            if (yackflags & TXINV)
                CLEARBIT(OUTPORT, OUTPIN); // Active LOW -> 0V on KEY-DOWN
            else
                SETBIT(OUTPORT, OUTPIN);   // Active HIGH -> +5V on KEY-DOWN
        }
    }

    if (mode == UP)
    {
        if (volflags & SIDETONE)
        {
            TCCR0A = 0;
            TCCR0B = 0;
            CLEARBIT(STPORT, STPIN);       // Ensure speaker/LED is kept OFF in idle
        }

        if (volflags & TXKEY)
        {
            if (yackflags & TXINV)
                SETBIT(OUTPORT, OUTPIN);   // Active LOW -> +5V on KEY-UP (idle)
            else
                CLEARBIT(OUTPORT, OUTPIN); // Active HIGH -> 0V on KEY-UP (idle)
        }
    }
}

void yackfarns(void)
{
    word i = farnsworth;

    while (i--)
    {
        yackdelay(1);
    }
}

void yackdelay(byte n)
{
    byte i = n;
    byte x;

    while (i--)
    {
        x = wpmcnt;
        while (x--)
            yackbeat();
    }
}

void yackplay(byte i)
{
    key(DOWN);

#ifdef POWERSAVE
    yackpower(FALSE);
#endif

    switch (i)
    {
        case DAH:
            yackdelay(DAHLEN);
            break;

        case DIT:
            yackdelay(DITLEN);
            break;
    }

    key(UP);
}

void yackchar(char c)
{
    byte code = 0x80;
    byte i;

    if (c >= '0' && c <= '9')
        code = pgm_read_byte(&morse[c - '0']);

    if (c >= 'a' && c <= 'z')
        code = pgm_read_byte(&morse[c - 'a' + 10]);

    if (c >= 'A' && c <= 'Z')
        code = pgm_read_byte(&morse[c - 'A' + 10]);

    for (i = 0; i < sizeof(spechar); i++)
    {
        if (c == (char)pgm_read_byte(&spechar[i]))
            code = pgm_read_byte(&morse[i + 36]);
    }

    if (c == ' ')
    {
        yackdelay(IWGLEN - ICGLEN);
    }
    else
    {
        while (code != 0x80)
        {
            if (yackctrlkey(FALSE))
                return;

            if (code & 0x80)
                yackplay(DAH);
            else
                yackplay(DIT);

            yackdelay(IEGLEN);
            code = code << 1;
        }

        yackdelay(ICGLEN - IEGLEN);
        yackfarns();
    }
}

void yackstring(const char *p)
{
    char c;

    while ((c = (char)pgm_read_byte(p++)) && !(yackctrlkey(FALSE)))
    {
        yackchar(c);
    }
}

void yacknumber(word n)
{
    char buffer[5];
    byte i = 0;

    if (n == 0)
    {
        yackchar('0');
        yackchar(' ');
        return;
    }

    while (n)
    {
        buffer[i++] = (n % 10) + '0';
        n /= 10;
    }

    while (i)
    {
        yackchar(buffer[--i]);
    }

    yackchar(' ');
}

// ============================================================================
// BUTTON SCANNING AND PADDLE SENSING FUNCTIONS
// ============================================================================

static void keylatch(void)
{
    byte swap = (yackflags & PDLSWAP);

    if (!(KEYINP & (1 << DITPIN)))
        volflags |= (swap ? DAHLATCH : DITLATCH);

    if (!(KEYINP & (1 << DAHPIN)))
        volflags |= (swap ? DITLATCH : DAHLATCH);
}

byte yackctrlkey(byte mode)
{
    byte volbfr = volflags;

    if (!(BTNINP & (1 << BTNPIN))) // Command button is pressed
    {
        volbfr |= CKLATCH;

        yackinhibit(ON);
        _delay_ms(50);

        while (!(BTNINP & (1 << BTNPIN))) // Wait for button release
        {
            if (!(KEYINP & (1 << DITPIN))) // DIT paddle pressed -> speed down
            {
                yackspeed(DOWN, WPMSPEED);
                volbfr &= ~(CKLATCH);
            }

            if (!(KEYINP & (1 << DAHPIN))) // DAH paddle pressed -> speed up
            {
                yackspeed(UP, WPMSPEED);
                volbfr &= ~(CKLATCH);
            }
        }

        _delay_ms(50); // Trailing edge debounce
    }

    volflags = volbfr;

    if (mode == TRUE)
    {
        volflags &= ~(CKLATCH);
    }

    yacksave();

    return ((volbfr & CKLATCH) != 0);
}

static char morsechar(byte buffer)
{
    byte i;

    for (i = 0; i < sizeof(morse); i++)
    {
        if (pgm_read_byte(&morse[i]) == buffer)
        {
            if (i < 10) return ('0' + i);
            if (i < 36) return ('A' + i - 10);
            return ((char)pgm_read_byte(&spechar[i - 36]));
        }
    }

    return '\0';
}

void yackmessage(byte function, byte msgnr)
{
    unsigned char rambuffer[RBSIZE];
    unsigned char c;
    word extimer = 0;
    byte i = 0;
    byte n;

    if (function == RECORD)
    {
        extimer = YACKSECS(DEFTIMEOUT);

        while (extimer--)
        {
            if (yackctrlkey(FALSE)) return;

            if ((c = (unsigned char)yackiambic(ON)))
            {
                rambuffer[i++] = c;
                extimer = YACKSECS(DEFTIMEOUT);
            }

            if (i >= RBSIZE)
            {
                yackerror();
                i = 0;
            }

            yackbeat();
        }

        if (i)
        {
            rambuffer[--i] = 0; // Terminate string over last space

            // Replay
            for (n = 0; n < i; n++)
                yackchar(rambuffer[n]);

            // Save to EEPROM
            if (msgnr == 1)
                eeprom_write_block(rambuffer, eebuffer1, RBSIZE);
            else
                eeprom_write_block(rambuffer, eebuffer2, RBSIZE);
        }
        else
        {
            yackerror();
        }
    }

    if (function == PLAY)
    {
        if (msgnr == 1)
            eeprom_read_block(rambuffer, eebuffer1, RBSIZE);
        else
            eeprom_read_block(rambuffer, eebuffer2, RBSIZE);

        for (n = 0; (c = rambuffer[n]); n++)
            yackchar(c);
    }
}

char yackiambic(byte ctrl)
{
    static enum FSMSTATE fsms = IDLE;
    static word timer;
    static byte lastsymbol;
    static byte buffer = 0;
    static byte bcntr = 0;
    static byte iwgflag = 0;
    static byte ultimem = 0;
    char retchar;

    if (timer) timer--;

    if (ctrl == OFF) iwgflag = 0;

    switch (fsms)
    {
        case IDLE:
            keylatch();

#ifdef POWERSAVE
            yackpower(TRUE);
#endif

            switch (yackflags & MODE)
            {
                case IAMBICA:
                case IAMBICB:
                    volflags &= ~lastsymbol;
                    lastsymbol = 0;
                    break;

                case ULTIMATIC:
                    if ((volflags & SQUEEZED) == SQUEEZED)
                    {
                        if (ultimem)
                            volflags &= ~ultimem;
                        else
                            volflags &= ~DITLATCH;
                    }
                    else
                    {
                        ultimem = volflags & SQUEEZED;
                    }
                    break;

                case DAHPRIO:
                    if ((volflags & SQUEEZED) == SQUEEZED)
                    {
                        volflags &= ~DITLATCH;
                    }
                    break;
            }

            if (timer == 0 && bcntr != 0)
            {
                buffer = buffer << 1;
                buffer |= 1;
                buffer = buffer << (7 - bcntr);
                retchar = morsechar(buffer);
                buffer = bcntr = 0;
                timer = (IWGLEN - ICGLEN) * wpmcnt;
                iwgflag = 1;
                return (retchar);
            }

            if (timer == 0 && iwgflag)
            {
                iwgflag = 0;
                return (' ');
            }

            if (volflags & (DITLATCH | DAHLATCH))
            {
                iwgflag = 0;
                bcntr++;
                buffer = buffer << 1;

                if (volflags & DITLATCH)
                {
                    timer = DITLEN * wpmcnt;
                    lastsymbol = DITLATCH;
                }
                else
                {
                    timer = DAHLEN * wpmcnt;
                    lastsymbol = DAHLATCH;
                    buffer |= 1;
                }

                key(DOWN);
                volflags &= ~(DITLATCH | DAHLATCH);
                fsms = KEYED;
            }
            break;

        case KEYED:
#ifdef POWERSAVE
            yackpower(FALSE);
#endif
            if ((yackflags & MODE) == IAMBICB)
                keylatch();

            if (timer == 0)
            {
                key(UP);
                timer = IEGLEN * wpmcnt;
                fsms = IEG;
            }
            break;

        case IEG:
            keylatch();

            if (timer == 0)
            {
                fsms = IDLE;
                timer = (ICGLEN - IEGLEN - 1) * wpmcnt;
            }
            break;
    }

    return '\0';
}
