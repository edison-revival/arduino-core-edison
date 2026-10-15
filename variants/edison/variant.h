/*
  variant.h - how each Arduino header pin reaches an Edison SoC GPIO.
  Copyright (c) 2026 The edison-revival contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#pragma once

#include <stddef.h>
#include <stdint.h>

// An I/O expander line (named by the arduino ACPI overlay) and the level it must have.
struct EdisonMuxSetting {
    const char *line;
    uint8_t value;
};

// The kit routes each header pin through a level shifter whose direction is set by an
// expander line, with a switchable pull resistor and, on some pins, muxes that choose
// between the GPIO and a peripheral (SPI, analog input).
struct EdisonPinDesc {
    int soc;                       // gpiochip0 line, or -1
    const char *dir;               // level shifter direction: HIGH = output
    const char *pull;              // HIGH = pull-up, LOW = pull-down, input = none
    EdisonMuxSetting gpio_mux[2];  // lines to set to route the pin to the GPIO
    const char *busy;              // the peripheral that owns the pin, if any
};

extern const EdisonPinDesc edison_pins[];
extern const size_t edison_num_pins;
