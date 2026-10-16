/*
  gpio.h - GPIO line handling shared by the core's pin functions.
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#pragma once

// Return the sketch's pins to inputs and release every line it holds, so the next sketch
// can take them. Called once, when the sketch is stopped; the process must exit after it,
// as pin functions called from then on block.
void edison_gpio_release_all();
