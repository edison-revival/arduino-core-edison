/*
  gpio.h - GPIO line handling shared by the core's pin functions.
  Copyright (c) 2026 The edison-revival contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#pragma once

// Release every GPIO line the sketch holds (on exit, so the next sketch can take them).
void edison_gpio_release_all();
