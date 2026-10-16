/*
  Arduino.h - Intel Edison (Kit for Arduino) core, open-edison.
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#pragma once

#include "api/ArduinoAPI.h"

#define digitalPinToInterrupt(p) (p)

#ifdef __cplusplus
extern "C" {
#endif
// analogRead()'s resolution in bits: 10 (0-1023) by default, as on AVR boards; the kit's
// ADC has 12. As on ARM Arduino boards.
void analogReadResolution(int bits);
// analogWrite()'s resolution in bits: 8 (0-255) by default, as on AVR boards.
void analogWriteResolution(int bits);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
using namespace arduino;

#include "TtySerial.h"

// Serial: the USB serial port on J16 (the host's Serial Monitor).
// Serial1: UART1 on IO0 (RX) / IO1 (TX).
extern TtySerial Serial;
extern TtySerial Serial1;
#endif

#include "pins_arduino.h"
