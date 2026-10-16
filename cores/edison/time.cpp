/*
  time.cpp - millis(), micros() and delays for Linux.
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#include <errno.h>
#include <sched.h>
#include <time.h>

#include "Arduino.h"

namespace {

uint64_t now_us() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000000u + ts.tv_nsec / 1000;
}

// The epoch is set on first use, so millis()/micros() are safe in global constructors
// (Intel's core divided by zero there, because its time base was only set in main()).
uint64_t epoch_us() {
    static const uint64_t epoch = now_us();
    return epoch;
}

void sleep_us(uint64_t us) {
    timespec ts = {time_t(us / 1000000u), long(us % 1000000u) * 1000};
    while (clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, &ts) == EINTR) {
    }
}

}  // namespace

unsigned long micros() {
    // Read the epoch first: on the very first call it's set now, and must not be later than
    // the time it's subtracted from.
    uint64_t epoch = epoch_us();
    return now_us() - epoch;
}

unsigned long millis() { return micros() / 1000u; }

void delay(unsigned long ms) {
    // Let other work run during long delays, as on other cores.
    sleep_us(uint64_t(ms) * 1000u);
}

void delayMicroseconds(unsigned int us) {
    // Sleeping has ~50-100 us of scheduling latency; busy-wait for short delays.
    if (us >= 200) {
        sleep_us(us);
        return;
    }
    uint64_t end = now_us() + us;
    while (now_us() < end) {
    }
}

void yield() { sched_yield(); }
