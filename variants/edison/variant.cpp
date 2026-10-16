/*
  variant.cpp - Intel Edison Kit for Arduino pin table.
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.

  Derived from the schematic and Intel's corelibs-edison variant (MuxDesc tables). The
  expander line names come from meta-acpi's arduino overlay, which edison-image loads.
  TRI_STATE_ALL is held high by edison-image (edison-acpi-tables.service), and the SoC's
  pin functions switch to GPIO when a line is requested, so neither appears here.
*/
#include "variant.h"

#include "pins_arduino.h"

const EdisonPinDesc edison_pins[] = {
    /* 0 */ {130, "MUX33_DIR", "DIG0_PU_PD", {}, "UART1 (Serial1)"},
    /* 1 */ {131, "MUX31_DIR", "DIG1_PU_PD", {}, "UART1 (Serial1)"},
    /* 2 */ {128, "MUX29_DIR", "DIG2_PU_PD", {}, nullptr},
    /* 3 */ {12, "MUX27_DIR", "DIG3_PU_PD", {}, nullptr},
    /* 4 */ {129, "MUX24_DIR", "DIG4_PU_PD", {}, nullptr},
    /* 5 */ {13, "MUX21_DIR", "DIG5_PU_PD", {}, nullptr},
    /* 6 */ {182, "MUX19_DIR", "DIG6_PU_PD", {}, nullptr},
    /* 7 */ {48, "MUX32_DIR", "DIG7_PU_PD", {}, nullptr},
    /* 8 */ {49, "MUX30_DIR", "DIG8_PU_PD", {}, nullptr},
    /* 9 */ {183, "MUX28_DIR", "DIG9_PU_PD", {}, nullptr},
    /* 10 */ {41, "MUX26_DIR", "DIG10_PU_PD", {{"SPI_FS_SEL", 0}, {"MUX25_SEL", 1}}, nullptr},
    /* 11 */ {43, "MUX23_DIR", "DIG11_PU_PD", {{"SPI_TXD_SEL", 0}, {"MUX22_SEL", 1}}, nullptr},
    /* 12 */ {42, "MUX20_DIR", "DIG12_PU_PD", {{"SPI_RXD_SEL", 0}}, nullptr},
    /* 13 */ {40, "MUX18_DIR", "DIG13_PU_PD", {{"SPI_CLK_SEL", 0}}, nullptr},
    /* A0 */ {44, "MUX14_DIR", "A0_PU_PD", {{"MUX15_SEL", 0}}, nullptr, 0, {"MUX15_SEL", 1}},
    /* A1 */ {45, "MUX12_DIR", "A1_PU_PD", {{"MUX13_SEL", 0}}, nullptr, 1, {"MUX13_SEL", 1}},
    /* A2 */ {46, "MUX10_DIR", "A2_PU_PD", {{"MUX11_SEL", 0}}, nullptr, 2, {"MUX11_SEL", 1}},
    /* A3 */ {47, "MUX8_DIR", "A3_PU_PD", {{"MUX9_SEL", 0}}, nullptr, 3, {"MUX9_SEL", 1}},
    // A4/A5 are I2C-6 in edison-image, so they can't be analog inputs either.
    /* A4 */ {14, "MUX6_DIR", "A4_PU_PD", {}, "I2C-6 (Wire)"},
    /* A5 */ {165, "MUX4_DIR", "A5_PU_PD", {}, "I2C-6 (Wire)"},
};

const size_t edison_num_pins = sizeof(edison_pins) / sizeof(edison_pins[0]);

const EdisonMuxSetting edison_spi_isolate[4] = {
    {"SPI_CLK_SEL", 0}, {"SPI_TXD_SEL", 0}, {"SPI_RXD_SEL", 0}, {"SPI_FS_SEL", 0}};

static_assert(sizeof(edison_pins) / sizeof(edison_pins[0]) == NUM_DIGITAL_PINS,
              "one entry per digital pin");
