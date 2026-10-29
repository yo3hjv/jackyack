/*!
 * @file      commands.cpp
 * @brief     External functions for interactive modes and CW commands
 * @author    Jan Lategahn DK3LJ / Jack Welch AI4SV - adapted for Digispark 16.5MHz
 */

#include "setup.h"
#include "yack.h"
#include "commands.h"

// Messages in Flash memory (PROGMEM)
static const char txok[] PROGMEM = MSG_OK;
static const char vers[] PROGMEM = MSG_VERSION;
static const char prgx[] PROGMEM = MSG_SIGNOFF;

void pitch(void)
{
    word timer = PITCHREPEAT;

    while (timer)
    {
        timer--;
        yackchar('E');

        if (!(KEYINP & (1 << DITPIN))) // DIT -> pitch up
        {
            yackpitch(DOWN);
            timer = PITCHREPEAT;
        }

        if (!(KEYINP & (1 << DAHPIN))) // DAH -> pitch down
        {
            yackpitch(UP);
            timer = PITCHREPEAT;
        }
    }
}

void setfarns(void)
{
    byte timer = 0;

    while (timer++ != FARNSREPEAT)
    {
        if (!(KEYINP & (1 << DITPIN)))
        {
            yackspeed(DOWN, FARNSWORTH);
            timer = 0;
        }
        else if (!(KEYINP & (1 << DAHPIN)))
        {
            yackspeed(UP, FARNSWORTH);
            timer = 0;
        }
        else
        {
            yackdelay(IWGLEN);
        }
    }
}

word lfsr(byte n)
{
    static word lfsr_val = 0xACE1;
    byte random;

    lfsr_val = (lfsr_val >> 1) ^ (-(lfsr_val & 1u) & 0xB400u);
    random = lfsr_val >> 8;

    while (random >= n) random -= n;

    return random;
}

void rndcall(char* call)
{
    byte i;

    // Callsign format: 2 letters + 1 digit + 2 letters
    for (i = 0; i < 5; i++)
    {
        if (i == 2)
            call[i] = lfsr(10) + '0';
        else
            call[i] = lfsr(26) + 'A';
    }
}

void cstrain(void)
{
    char call[5];
    char c;
    byte i;
    byte n;
    word timer;

    while (1)
    {
        rndcall(call);
        i = 0;

        while (i < 5)
        {
            if (!i)
            {
                yackdelay(2 * IWGLEN);
                for (n = 0; n < 5; n++)
                {
                    yackchar(call[n]);
                    yackfarns();
                    if (yackctrlkey(TRUE))
                        return;
                }
            }

            timer = YACKSECS(TRAINTIMEOUT);

            do
            {
                c = yackiambic(OFF);
                yackbeat();
                timer--;
            } while ((!c) && timer && !(yackctrlkey(FALSE)));

            if (timer == 0 || yackctrlkey(TRUE))
                return;

            if (call[i] == c)
            {
                i++;
            }
            else
            {
                yackerror();
                i = 0;
            }
        }

        yackchar('R');
    }
}

void beacon(byte mode)
{
    static word interval = 65000;
    static word timer;
    char c;

    if (interval == 65000)
        interval = yackuser(READ, 1, 0);

    if (mode == RECORD)
    {
        interval = 0;
        timer = YACKSECS(DEFTIMEOUT);

        yackchar('N');

        while (--timer)
        {
            c = yackiambic(FALSE);
            yackbeat();

            if (c >= '0' && c <= '9')
            {
                interval *= 10;
                interval += (c - '0');
                timer = YACKSECS(DEFTIMEOUT);
            }
        }

        if (interval <= 9999)
        {
            yackuser(WRITE, 1, interval);
            yacknumber(interval);
        }
        else
        {
            yackerror();
        }
    }

    if ((mode == PLAY) && interval)
    {
#ifdef POWERSAVE
        yackpower(FALSE);
#endif
        if (timer)
        {
            timer--;
        }
        else
        {
            timer = YACKSECS(1);

            if ((--interval) == 0)
            {
                interval = yackuser(READ, 1, 0);
                yackmessage(PLAY, 2);
            }
        }
    }
}

void commandmode(void)
{
    char c;
    word timer;

    yackinhibit(ON);
    yackchar('?');

    timer = YACKSECS(DEFTIMEOUT);

    while ((yackctrlkey(TRUE) == 0) && (timer-- > 0))
    {
        c = yackiambic(OFF);
        if (c) timer = YACKSECS(DEFTIMEOUT);

        yackbeat();
        lfsr(255);

        if (!yackflag(CONFLOCK))
        {
            switch (c)
            {
                case 'R': // Reset to default values
                    yackreset();
                    c = TRUE;
                    break;

                case 'A': // IAMBIC A
                    yackmode(IAMBICA);
                    c = TRUE;
                    break;

                case 'B': // IAMBIC B
                    yackmode(IAMBICB);
                    c = TRUE;
                    break;

                case 'L': // Ultimatic
                    yackmode(ULTIMATIC);
                    c = TRUE;
                    break;

                case 'D': // DAH priority
                    yackmode(DAHPRIO);
                    c = TRUE;
                    break;

                case 'X': // Paddle swapping (DIT <-> DAH)
                    yacktoggle(PDLSWAP);
                    c = TRUE;
                    break;

                case 'S': // Toggle sidetone ON/OFF
                    yacktoggle(SIDETONE);
                    c = TRUE;
                    break;

                case 'K': // Toggle TX transmitter keying ON/OFF
                    yacktoggle(TXKEY);
                    c = TRUE;
                    break;

                case 'Z': // Farnsworth pause adjust
                    setfarns();
                    c = TRUE;
                    break;

                case 'I': // Toggle TX logic level (Active HIGH <-> Active LOW)
                    yacktoggle(TXINV);
                    c = TRUE;
                    break;

                case '1': // Record Macro 1
                    yackchar('1');
                    yackmessage(RECORD, 1);
                    c = TRUE;
                    break;

                case '2': // Record Macro 2
                    yackchar('2');
                    yackmessage(RECORD, 2);
                    c = TRUE;
                    break;

                case 'N': // Automatic Beacon interval
                    beacon(RECORD);
                    c = TRUE;
                    break;
            }
        }

        switch (c)
        {
            case 'V': // Version query
                yackstring(vers);
                c = TRUE;
                break;

            case 'P': // Pitch adjust
                pitch();
                c = TRUE;
                break;

            case 'U': // Continuous keydown tune (20s)
                yackinhibit(OFF);
                yacktune();
                yackinhibit(ON);
                c = TRUE;
                break;

            case 'C': // Callsign trainer
                cstrain();
                c = TRUE;
                break;

            case '0': // Lock / unlock configuration
                yacktoggle(CONFLOCK);
                c = TRUE;
                break;

            case 'E': // Play Macro 1
                yackinhibit(OFF);
                yackmessage(PLAY, 1);
                yackinhibit(ON);
                timer = YACKSECS(MACTIMEOUT);
                c = FALSE;
                break;

            case 'T': // Play Macro 2
                yackinhibit(OFF);
                yackmessage(PLAY, 2);
                yackinhibit(ON);
                timer = YACKSECS(MACTIMEOUT);
                c = FALSE;
                break;

            case 'W': // Query current speed in WPM
                yacknumber(yackwpm());
                c = TRUE;
                break;
        }

        if (c == TRUE)
            yackstring(txok);
        else if (c)
            yackerror();
    }

    yackstring(prgx); // Sign-off prosign "SK"
    yackinhibit(OFF);
}
