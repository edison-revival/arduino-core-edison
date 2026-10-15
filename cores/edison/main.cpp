/*
  main.cpp - entry point for sketches running as a Linux process (edison-sketch.service).
  Copyright (c) 2026 The edison-revival contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "Arduino.h"
#include "gpio.h"

namespace {

volatile sig_atomic_t stop_requested = 0;

void on_signal(int) { stop_requested = 1; }

}  // namespace

void init() {
    // systemd stops the sketch with SIGTERM (e.g. before running a new upload): leave the
    // loop and exit normally, so GPIO lines are released.
    struct sigaction sa = {};
    sa.sa_handler = on_signal;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    // A host closing the serial port mustn't kill the sketch.
    signal(SIGPIPE, SIG_IGN);
}

void initVariant() __attribute__((weak));
void initVariant() {}

int main() {
    init();
    initVariant();
    setup();
    while (!stop_requested) {
        loop();
        if (arduino::serialEventRun) arduino::serialEventRun();
    }
    Serial.end();
    Serial1.end();
    edison_gpio_release_all();
    return 0;
}
