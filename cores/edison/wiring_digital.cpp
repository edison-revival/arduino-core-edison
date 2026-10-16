/*
  wiring_digital.cpp - pinMode(), digitalWrite() and digitalRead() through libgpiod.
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#include <errno.h>
#include <gpiod.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <mutex>
#include <utility>
#include <vector>

#include "Arduino.h"
#include "gpio.h"
#include "variant.h"

namespace {

const char *const CONSUMER = "sketch";

struct Line {
    int chip = -1;
    unsigned offset = 0;
    explicit operator bool() const { return chip >= 0; }
};

std::vector<gpiod_chip *> &chips() {
    static std::vector<gpiod_chip *> list = [] {
        std::vector<gpiod_chip *> v;
        for (int i = 0;; i++) {
            char path[32];
            snprintf(path, sizeof(path), "/dev/gpiochip%d", i);
            gpiod_chip *c = gpiod_chip_open(path);
            if (!c) {
                if (i == 0) fprintf(stderr, "gpio: can't open %s: %s\n", path, strerror(errno));
                break;
            }
            v.push_back(c);
        }
        return v;
    }();
    return list;
}

Line soc_line(int offset) { return chips().empty() ? Line{} : Line{0, unsigned(offset)}; }

Line named_line(const char *name) {
    static std::map<const char *, Line> cache;  // names are string literals from the table
    auto it = cache.find(name);
    if (it != cache.end()) return it->second;
    Line l;
    auto &cs = chips();
    for (size_t i = 0; i < cs.size(); i++) {
        int off = gpiod_chip_get_line_offset_from_name(cs[i], name);
        if (off >= 0) {
            l = Line{int(i), unsigned(off)};
            break;
        }
    }
    if (!l) fprintf(stderr, "gpio: no line named %s (is the arduino ACPI overlay loaded?)\n", name);
    cache[name] = l;
    return l;
}

std::map<std::pair<int, unsigned>, gpiod_line_request *> held;

enum class Dir { In, Out };

// Request the line (or change it, if the sketch already holds it). Returns false, with errno
// set, if the line can't be had, e.g. because another process holds it (EBUSY).
bool set_line(Line l, Dir dir, int value = 0) {
    if (!l) return false;
    gpiod_line_settings *s = gpiod_line_settings_new();
    gpiod_line_config *lc = gpiod_line_config_new();
    if (!s || !lc) {
        gpiod_line_settings_free(s);
        gpiod_line_config_free(lc);
        return false;
    }
    if (dir == Dir::Out) {
        gpiod_line_settings_set_direction(s, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(
            s, value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
    } else {
        gpiod_line_settings_set_direction(s, GPIOD_LINE_DIRECTION_INPUT);
    }
    gpiod_line_config_add_line_settings(lc, &l.offset, 1, s);

    bool ok;
    auto key = std::make_pair(l.chip, l.offset);
    auto it = held.find(key);
    if (it != held.end()) {
        ok = gpiod_line_request_reconfigure_lines(it->second, lc) == 0;
    } else {
        gpiod_request_config *rc = gpiod_request_config_new();
        if (rc) gpiod_request_config_set_consumer(rc, CONSUMER);
        gpiod_line_request *req = gpiod_chip_request_lines(chips()[l.chip], rc, lc);
        gpiod_request_config_free(rc);
        ok = req != nullptr;
        if (ok) held[key] = req;
    }
    gpiod_line_config_free(lc);
    gpiod_line_settings_free(s);
    return ok;
}

gpiod_line_request *request_of(Line l) {
    auto it = held.find(std::make_pair(l.chip, l.offset));
    return it == held.end() ? nullptr : it->second;
}

enum class State { Unset, Input, Output, Analog, Failed };
State state[NUM_DIGITAL_PINS];

// Report each pin's problem once: sketches call these functions in loops.
bool reported[NUM_DIGITAL_PINS];
void report(pin_size_t pin, const char *what, const char *why) {
    if (reported[pin]) return;
    reported[pin] = true;
    fprintf(stderr, "pin %u: %s: %s\n", unsigned(pin), what, why);
}

void set_pull(const EdisonPinDesc &p, PinMode mode) {
    Line l = named_line(p.pull);
    if (mode == INPUT_PULLUP) {
        set_line(l, Dir::Out, 1);
    } else if (mode == INPUT_PULLDOWN) {
        set_line(l, Dir::Out, 0);
    } else {
        set_line(l, Dir::In);
    }
}

bool configure(pin_size_t pin, PinMode mode) {
    const EdisonPinDesc &p = edison_pins[pin];
    if (p.busy) {
        errno = EBUSY;
        report(pin, "in use by", p.busy);
        return false;
    }
    if (mode == OUTPUT_OPENDRAIN) {
        report(pin, "pinMode", "OUTPUT_OPENDRAIN isn't supported (the level shifter drives)");
        return false;
    }
    for (const EdisonMuxSetting &m : p.gpio_mux) {
        if (m.line && !set_line(named_line(m.line), Dir::Out, m.value)) {
            report(pin, m.line, strerror(errno));
            return false;
        }
    }
    Line soc = soc_line(p.soc), dir = named_line(p.dir);
    // Turn the level shifter round only while the SoC side isn't driving into it.
    bool ok;
    if (mode == OUTPUT) {
        set_pull(p, INPUT);
        ok = set_line(dir, Dir::Out, 1) && set_line(soc, Dir::Out, 0);
    } else {
        ok = set_line(soc, Dir::In) && set_line(dir, Dir::Out, 0);
        if (ok) set_pull(p, mode);
    }
    if (!ok) {
        report(pin, "can't claim its GPIO lines", strerror(errno));
        return false;
    }
    return true;
}

void set_mode(pin_size_t pin, PinMode mode) {
    if (configure(pin, mode)) {
        state[pin] = mode == OUTPUT ? State::Output : State::Input;
    } else {
        state[pin] = State::Failed;
    }
}

// The pin as an analog input: the level shifter and pull resistor out of the way, and the
// mux switched from the GPIO to the ADC.
bool configure_analog(pin_size_t pin) {
    const EdisonPinDesc &p = edison_pins[pin];
    if (p.busy) {
        errno = EBUSY;
        report(pin, "in use by", p.busy);
        return false;
    }
    if (p.adc < 0) {
        report(pin, "analogRead", "not an analog input (use A0-A3)");
        return false;
    }
    // Isolate the ADC's SPI bus from the header (see edison_spi_isolate). Busy when SPI is
    // enabled on the header, which isolates it properly; nothing to do then.
    static bool isolated = false;
    if (!isolated) {
        for (const EdisonMuxSetting &m : edison_spi_isolate) {
            set_line(named_line(m.line), Dir::Out, m.value);
        }
        isolated = true;
    }
    Line soc = soc_line(p.soc), dir = named_line(p.dir);
    bool ok = set_line(soc, Dir::In) && set_line(dir, Dir::Out, 0) &&
              set_line(named_line(p.adc_mux.line), Dir::Out, p.adc_mux.value);
    if (ok) set_pull(p, INPUT);
    if (!ok) {
        report(pin, "can't claim its analog lines", strerror(errno));
        return false;
    }
    return true;
}

// Held by every pin function, and taken for good by edison_gpio_release_all() when the
// sketch is stopped (from another thread), so the two never touch the lines at once.
std::mutex gpio_lock;

}  // namespace

int edison_analog_select(unsigned pin) {
    if (pin >= NUM_DIGITAL_PINS) return -1;
    std::lock_guard<std::mutex> lock(gpio_lock);
    if (state[pin] != State::Analog) {
        state[pin] = configure_analog(pin_size_t(pin)) ? State::Analog : State::Failed;
    }
    return state[pin] == State::Analog ? edison_pins[pin].adc : -1;
}

void pinMode(pin_size_t pin, PinMode mode) {
    if (pin >= NUM_DIGITAL_PINS) return;
    std::lock_guard<std::mutex> lock(gpio_lock);
    set_mode(pin, mode);
}

void digitalWrite(pin_size_t pin, PinStatus value) {
    if (pin >= NUM_DIGITAL_PINS) return;
    std::lock_guard<std::mutex> lock(gpio_lock);
    if (state[pin] == State::Input) {
        // As on AVR: writing to an input switches its pull-up on or off.
        set_pull(edison_pins[pin], value == HIGH ? INPUT_PULLUP : INPUT);
        return;
    }
    if (state[pin] != State::Output) return;
    gpiod_line_request *r = request_of(soc_line(edison_pins[pin].soc));
    if (r) {
        gpiod_line_request_set_value(
            r, edison_pins[pin].soc,
            value == LOW ? GPIOD_LINE_VALUE_INACTIVE : GPIOD_LINE_VALUE_ACTIVE);
    }
}

PinStatus digitalRead(pin_size_t pin) {
    if (pin >= NUM_DIGITAL_PINS) return LOW;
    std::lock_guard<std::mutex> lock(gpio_lock);
    // Pins start as inputs on other boards; do the same for a pin read before pinMode().
    if (state[pin] == State::Unset) set_mode(pin, INPUT);
    if (state[pin] == State::Failed) return LOW;
    gpiod_line_request *r = request_of(soc_line(edison_pins[pin].soc));
    if (!r) return LOW;
    return gpiod_line_request_get_value(r, edison_pins[pin].soc) == GPIOD_LINE_VALUE_ACTIVE
               ? HIGH
               : LOW;
}

void edison_gpio_release_all() {
    // Never unlocked: the process exits next, and any pin call the sketch makes meanwhile
    // waits rather than undoing this.
    gpio_lock.lock();
    // Leave the pins as inputs, as a reset does on other boards: the lines keep their last
    // levels once released, so an output would otherwise go on driving.
    for (pin_size_t pin = 0; pin < NUM_DIGITAL_PINS; pin++) {
        if (state[pin] == State::Output || state[pin] == State::Input ||
            state[pin] == State::Analog) {
            configure(pin, INPUT);
        }
    }
    for (auto &h : held) gpiod_line_request_release(h.second);
    held.clear();
}
