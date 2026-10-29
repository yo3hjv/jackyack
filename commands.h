/*!
 * @file      commands.h
 * @brief     Header for external interactive modes and CW commands
 * @author    Jan Lategahn DK3LJ / Jack Welch AI4SV - adapted for Digispark 16.5MHz
 */

#ifndef COMMANDS_H
#define COMMANDS_H

#include "setup.h"
#include "yack.h"

#ifdef __cplusplus
extern "C" {
#endif

void commandmode(void);
void pitch(void);
void setfarns(void);
word lfsr(byte n);
void rndcall(char* call);
void cstrain(void);
void beacon(byte mode);

#ifdef __cplusplus
}
#endif

#endif // COMMANDS_H
