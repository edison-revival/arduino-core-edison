/*
  wiring_interrupts.cpp - attachInterrupt() on GPIO edges (libgpiod edge events).
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.

  The kernel timestamps and queues each edge; a thread waits for them and calls the
  sketch's handler. Handlers therefore run alongside loop() rather than pausing it:
  noInterrupts()/interrupts() hold them off, so the sketch can read what they change.
  LOW and HIGH (fire while the pin is at that level) are checked every millisecond.
*/
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <mutex>
#include <thread>
#include <vector>

#include "Arduino.h"
#include "gpio.h"

namespace {

struct Handler {
    voidFuncPtrParam fn = nullptr;
    void *param = nullptr;
    PinStatus mode = CHANGE;
    int fd = -1;
};

Handler handlers[NUM_DIGITAL_PINS];
std::mutex table_lock;           // handlers[]
std::recursive_mutex irq_lock;   // held while a handler runs, and by noInterrupts()
thread_local bool irq_disabled;  // this thread holds irq_lock through noInterrupts()
int wake_pipe[2] = {-1, -1};     // tells the thread the table changed

void call(unsigned pin) {
    Handler h;
    {
        std::lock_guard<std::mutex> l(table_lock);
        h = handlers[pin];
    }
    if (!h.fn) return;
    std::lock_guard<std::recursive_mutex> g(irq_lock);
    h.fn(h.param);
}

void on_edge(bool rising, void *arg) {
    unsigned pin = unsigned(reinterpret_cast<uintptr_t>(arg));
    PinStatus mode;
    {
        std::lock_guard<std::mutex> l(table_lock);
        mode = handlers[pin].mode;
    }
    // RISING/FALLING only detect their own edge; CHANGE both; LOW/HIGH fire on reaching
    // the level here, then again each millisecond while it lasts (below).
    if (mode == CHANGE || mode == RISING || mode == FALLING || (mode == HIGH) == rising) {
        call(pin);
    }
}

void run() {
    for (;;) {
        std::vector<pollfd> fds = {{wake_pipe[0], POLLIN, 0}};
        std::vector<unsigned> pins;
        std::vector<unsigned> level_pins;
        {
            std::lock_guard<std::mutex> l(table_lock);
            for (unsigned pin = 0; pin < NUM_DIGITAL_PINS; pin++) {
                const Handler &h = handlers[pin];
                if (!h.fn || h.fd < 0) continue;
                fds.push_back({h.fd, POLLIN, 0});
                pins.push_back(pin);
                if (h.mode == LOW || h.mode == HIGH) level_pins.push_back(pin);
            }
        }
        if (poll(fds.data(), fds.size(), level_pins.empty() ? -1 : 1) < 0) continue;
        if (fds[0].revents) {
            char buf[16];
            while (read(wake_pipe[0], buf, sizeof(buf)) > 0) {
            }
        }
        for (size_t i = 0; i < pins.size(); i++) {
            if (fds[i + 1].revents & POLLIN) {
                edison_gpio_events(pins[i], on_edge, reinterpret_cast<void *>(uintptr_t(pins[i])));
            }
        }
        for (unsigned pin : level_pins) {
            PinStatus mode;
            {
                std::lock_guard<std::mutex> l(table_lock);
                mode = handlers[pin].mode;
            }
            if ((mode == LOW || mode == HIGH) && digitalRead(pin_size_t(pin)) == mode) call(pin);
        }
    }
}

void wake() {
    static std::once_flag started;
    std::call_once(started, [] {
        if (pipe2(wake_pipe, O_CLOEXEC | O_NONBLOCK) == 0) std::thread(run).detach();
    });
    if (wake_pipe[1] >= 0) {
        char c = 0;
        (void)!write(wake_pipe[1], &c, 1);
    }
}

void call_plain(void *fn) { reinterpret_cast<voidFuncPtr>(fn)(); }

}  // namespace

void attachInterruptParam(pin_size_t pin, voidFuncPtrParam fn, PinStatus mode, void *param) {
    if (pin >= NUM_DIGITAL_PINS || !fn) return;
    int edges = mode == RISING ? RISING : mode == FALLING ? FALLING : CHANGE;
    int fd = edison_gpio_edges(pin, edges);
    if (fd < 0) return;
    {
        std::lock_guard<std::mutex> l(table_lock);
        handlers[pin] = {fn, param, mode, fd};
    }
    wake();
}

void attachInterrupt(pin_size_t pin, voidFuncPtr fn, PinStatus mode) {
    attachInterruptParam(pin, call_plain, mode, reinterpret_cast<void *>(fn));
}

void detachInterrupt(pin_size_t pin) {
    if (pin >= NUM_DIGITAL_PINS) return;
    bool attached;
    {
        std::lock_guard<std::mutex> l(table_lock);
        attached = handlers[pin].fn != nullptr;
        handlers[pin] = Handler();
    }
    if (!attached) return;
    edison_gpio_edges(pin, 0);
    wake();
}

void noInterrupts(void) {
    if (irq_disabled) return;
    irq_lock.lock();
    irq_disabled = true;
}

void interrupts(void) {
    if (!irq_disabled) return;
    irq_disabled = false;
    irq_lock.unlock();
}
