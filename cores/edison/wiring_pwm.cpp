/*
  wiring_pwm.cpp - analogWrite() through the SoC's PWM controller (Linux sysfs PWM).
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.

  IO3/5/6/9 have hardware PWM, at 490 Hz as on an Uno's pin 3. Each needs its SoC pin
  switched to its PWM function, which needs root: edison-image's edison-pinmux service does
  it on request. Other pins are written HIGH or LOW, as on AVR boards.
*/
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <mutex>
#include <string>

#include "Arduino.h"
#include "gpio.h"

namespace {

const long PERIOD_NS = 2040816;  // 490 Hz
const int CHANNELS = 4;
const char *const PINMUX_SOCKET = "/run/edison-pinmux.sock";

int resolution = 8;
std::mutex lock;

struct Channel {
    bool running = false;
    long duty = -1;
};
Channel channels[CHANNELS];

// The SoC's PWM controller (the only PWM chip, with four channels).
std::string chip_dir() {
    static const std::string dir = [] {
        const char *base = "/sys/class/pwm";
        std::string found;
        if (DIR *d = opendir(base)) {
            while (dirent *e = readdir(d)) {
                if (strncmp(e->d_name, "pwmchip", 7) == 0) {
                    found = std::string(base) + "/" + e->d_name;
                    break;
                }
            }
            closedir(d);
        }
        if (found.empty()) fprintf(stderr, "analogWrite: no PWM controller found\n");
        return found;
    }();
    return dir;
}

void sleep_ms(long ms) {
    timespec t = {ms / 1000, (ms % 1000) * 1000000};
    nanosleep(&t, nullptr);
}

// Write a value to a sysfs file. A newly exported channel's files belong to root until
// udev gives them to the gpio group, a moment later: retry for a while.
bool write_file(const std::string &path, long value) {
    std::string s = std::to_string(value);
    for (int attempt = 0; attempt < 100; attempt++) {
        int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            ssize_t n = write(fd, s.c_str(), s.size());
            int err = errno;
            close(fd);
            if (n == ssize_t(s.size())) return true;
            errno = err;
            return false;
        }
        if (errno != EACCES && errno != ENOENT) return false;
        sleep_ms(10);
    }
    return false;
}

void report_once(int channel, const char *what) {
    static bool reported[CHANNELS];
    if (reported[channel]) return;
    reported[channel] = true;
    fprintf(stderr, "analogWrite: PWM channel %d: %s: %s\n", channel, what, strerror(errno));
}

// Export the channel and run it at 490 Hz with nothing on.
bool start(int c) {
    std::string chip = chip_dir();
    if (chip.empty()) return false;
    std::string ch = chip + "/pwm" + std::to_string(c);
    // EBUSY: already exported (left by a sketch that didn't stop cleanly); reuse it.
    if (!write_file(chip + "/export", c) && errno != EBUSY) {
        report_once(c, "export");
        return false;
    }
    // Duty first: the period can't be set below it.
    if (!write_file(ch + "/duty_cycle", 0) || !write_file(ch + "/period", PERIOD_NS) ||
        !write_file(ch + "/enable", 1)) {
        report_once(c, "setup");
        return false;
    }
    channels[c].running = true;
    channels[c].duty = 0;
    return true;
}

}  // namespace

bool edison_pwm_pinmux(int channel) {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;
    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, PINMUX_SOCKET, sizeof(addr.sun_path) - 1);
    bool ok = false;
    if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
        std::string request = "pwm" + std::to_string(channel) + "\n";
        char reply[64] = {};
        if (write(fd, request.c_str(), request.size()) == ssize_t(request.size())) {
            ssize_t n = read(fd, reply, sizeof(reply) - 1);
            ok = n >= 2 && strncmp(reply, "ok", 2) == 0;
        }
    }
    close(fd);
    return ok;
}

void edison_pwm_stop(int c) {
    if (c < 0 || c >= CHANNELS) return;
    std::lock_guard<std::mutex> l(lock);
    if (!channels[c].running) return;
    std::string chip = chip_dir();
    write_file(chip + "/pwm" + std::to_string(c) + "/enable", 0);
    write_file(chip + "/unexport", c);
    channels[c] = Channel();
}

void analogWrite(pin_size_t pin, int value) {
    long max = (1L << resolution) - 1;
    if (value < 0) value = 0;
    if (value > max) value = int(max);
    int c = edison_pwm_select(pin);
    if (c < 0) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, value * 2 > max ? HIGH : LOW);
        return;
    }
    std::lock_guard<std::mutex> l(lock);
    if (!channels[c].running && !start(c)) return;
    long duty = PERIOD_NS * value / max;
    if (duty == channels[c].duty) return;
    std::string path = chip_dir() + "/pwm" + std::to_string(c) + "/duty_cycle";
    if (write_file(path, duty)) {
        channels[c].duty = duty;
    } else {
        report_once(c, "duty_cycle");
    }
}

void analogWriteResolution(int bits) {
    if (bits >= 1 && bits <= 16) resolution = bits;
}
