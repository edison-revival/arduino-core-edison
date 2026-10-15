/*
  pins_arduino.h - Intel Edison Kit for Arduino pin numbers.
  Copyright (c) 2026 The edison-revival contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#pragma once

#define NUM_DIGITAL_PINS 20
#define NUM_ANALOG_INPUTS 6

#define LED_BUILTIN 13

#define PIN_A0 (14)
#define PIN_A1 (15)
#define PIN_A2 (16)
#define PIN_A3 (17)
#define PIN_A4 (18)
#define PIN_A5 (19)

#ifdef __cplusplus
static const uint8_t A0 = PIN_A0;
static const uint8_t A1 = PIN_A1;
static const uint8_t A2 = PIN_A2;
static const uint8_t A3 = PIN_A3;
static const uint8_t A4 = PIN_A4;
static const uint8_t A5 = PIN_A5;

// I2C-6 (Wire) shares A4/A5 with the SDA/SCL header pins.
static const uint8_t SDA = PIN_A4;
static const uint8_t SCL = PIN_A5;

// SPI5, when enabled in edison-image's ACPI tables (off by default).
static const uint8_t SS = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 12;
static const uint8_t SCK = 13;
#endif

#define digitalPinIsValid(p) ((p) < NUM_DIGITAL_PINS)
#define analogInputToDigitalPin(p) (((p) < NUM_ANALOG_INPUTS) ? (p) + PIN_A0 : -1)
#define digitalPinHasPWM(p) ((p) == 3 || (p) == 5 || (p) == 6 || (p) == 9)
