#ifndef SERIAL_CLI_H
#define SERIAL_CLI_H

#include <Arduino.h>

// ==========================================================
// SERIAL CLI
// ==========================================================
//
// Non-blocking command parser for UART at 115200 baud.
//
// Commands:
//
// set ab <val>           Set adaptive baseline TDS
// set week <idx> <val>   Override week average
// r p                    Reset points to 0
// s p <0-5>             Set points manually
// show ab                Display adaptive baseline
// show p                 Display current points
// show w                 Display week data
//

void initSerialCLI();
void updateSerialCLI();

#endif