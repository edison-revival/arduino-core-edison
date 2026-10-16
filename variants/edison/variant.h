/*
  variant.h - how each Arduino header pin reaches an Edison SoC GPIO.
  Copyright (c) 2026 The open-edison contributors.

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
    int adc = -1;                  // the kit ADC's channel for this pin, or -1
    EdisonMuxSetting adc_mux{};    // the line to set to route the pin to the ADC
    int pwm = -1;                  // the SoC PWM channel (pwmchip0) for this pin, or -1
};

// SPI on the header (the SPI library): which pins carry SPI5, which way, and the mux line
// that routes each to SPI5 (1) rather than its GPIO (0). SS (IO10) stays a GPIO, which
// libraries drive themselves.
struct EdisonSpiPin {
    unsigned pin;
    bool output;     // MOSI and SCK: driven by SPI5
    const char *sel;
};
extern const EdisonSpiPin edison_spi_pins[3];
extern const char *const edison_spi_device;  // the spidev on SPI5 for the header

// Lines that route the header's SPI pins (IO10-IO13) to their GPIOs rather than SPI5.
// Driven to 0 for analogRead() too: the kit's ADC is on SPI5, and with these left
// floating its bus is connected to the header and it reads nothing.
extern const EdisonMuxSetting edison_spi_isolate[4];

extern const EdisonPinDesc edison_pins[];
extern const size_t edison_num_pins;
