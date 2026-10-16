/*
  edison_pwm.h - PWM with a chosen period, for libraries (Servo) beyond analogWrite().
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.

  Each of the four PWM pins (~3, ~5, ~6, ~9) has a channel of its own, so each can run at
  its own frequency. The high time is set in 255ths of the period (to the nearest step):
  at 50 Hz that's 78 us, at 490 Hz (analogWrite) 8 us.
*/
#pragma once

#include <stdint.h>

// Run hardware PWM on pin with this period and high time, in nanoseconds. Returns false
// if the pin has no PWM or can't be switched to it (reported once on the sketch's log).
bool edisonPwmWrite(uint8_t pin, uint32_t period_ns, uint32_t high_ns);

// Stop the pin's PWM (it stays switched to PWM until pinMode() takes it back).
void edisonPwmStop(uint8_t pin);
