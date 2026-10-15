/*
  main.cpp - entry point for sketches running as a Linux process (edison-sketch.service).
  Copyright (c) 2026 The edison-revival contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "Arduino.h"
#include "gpio.h"

namespace {

sigset_t stop_signals() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);  // systemd stopping the sketch (a new upload, shutdown)
    sigaddset(&set, SIGINT);   // Ctrl+C when run by hand
    return set;
}

// Stop at once when asked, wherever the sketch is (like a reset on other boards), leaving
// its pins as inputs. Runs in its own thread, so it can safely do more than a signal
// handler could.
void *stopper(void *) {
    sigset_t set = stop_signals();
    int sig;
    while (sigwait(&set, &sig) != 0) {
    }
    edison_gpio_release_all();  // takes the GPIO lock for good: the sketch can't race it
    _exit(0);
}

}  // namespace

void init() {
    // Block the stop signals here, so they go to the stopper thread (threads the sketch
    // starts later inherit the mask). If the thread can't start, keep the default action,
    // which ends the process without resetting the pins.
    sigset_t set = stop_signals();
    pthread_sigmask(SIG_BLOCK, &set, nullptr);
    pthread_t thread;
    if (pthread_create(&thread, nullptr, stopper, nullptr) == 0) {
        pthread_detach(thread);
    } else {
        perror("can't start the stop thread");
        pthread_sigmask(SIG_UNBLOCK, &set, nullptr);
    }
    // A host closing the serial port mustn't kill the sketch.
    signal(SIGPIPE, SIG_IGN);
}

void initVariant() __attribute__((weak));
void initVariant() {}

int main() {
    init();
    initVariant();
    setup();
    for (;;) {
        loop();
        if (arduino::serialEventRun) arduino::serialEventRun();
    }
}
