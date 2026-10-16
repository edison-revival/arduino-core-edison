/*
  Servo.h - hobby servos on the Intel Edison Kit for Arduino's PWM pins.
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.

  The same interface as Arduino's Servo library. The pulses come from the SoC's hardware
  PWM at 50 Hz, so they're steady, but only on the four PWM pins: ~3, ~5, ~6 and ~9 (one
  servo each). analogWrite() on the other PWM pins carries on at its own 490 Hz.

  Resolution: the hardware sets the pulse width in 255ths of the refresh interval, so at
  50 Hz it moves in steps of 78 us: about 7.5 degrees over the default 544-2400 us range.
  Digital servos usually accept faster refresh rates, and the steps shrink with the
  interval: setRefreshInterval(10000) (100 Hz) gives 39 us, 5000 (200 Hz) 20 us. Analog
  servos may buzz or run hot faster than 50 Hz.
*/
#pragma once

#include <Arduino.h>

#define MIN_PULSE_WIDTH 544      // the shortest pulse sent to a servo, in microseconds
#define MAX_PULSE_WIDTH 2400     // the longest
#define DEFAULT_PULSE_WIDTH 1500 // sent when attached
#define REFRESH_INTERVAL 20000   // microseconds between pulses (50 Hz)
#define MAX_SERVOS 4
#define INVALID_SERVO 255

class Servo {
  public:
    Servo();
    // Start pulses on pin (a PWM pin: 3, 5, 6 or 9), centred. Returns the servo's index,
    // or INVALID_SERVO.
    uint8_t attach(int pin);
    // As attach(pin), with the pulse widths (microseconds) for 0 and 180 degrees.
    uint8_t attach(int pin, int min, int max);
    void detach();
    // An angle (0-180), or, from MIN_PULSE_WIDTH up, a pulse width in microseconds.
    void write(int value);
    void writeMicroseconds(int value);
    int read();              // the angle last written
    int readMicroseconds();  // the pulse width
    bool attached();
    // Edison only: microseconds between pulses (default REFRESH_INTERVAL, 20000: 50 Hz).
    // Shorter gives finer steps (see above); 2500-50000, and longer than the widest pulse.
    void setRefreshInterval(int us);

  private:
    bool update();

    int pin_ = -1;
    int min_ = MIN_PULSE_WIDTH;
    int max_ = MAX_PULSE_WIDTH;
    int us_ = DEFAULT_PULSE_WIDTH;
    int refresh_us_ = REFRESH_INTERVAL;
};
