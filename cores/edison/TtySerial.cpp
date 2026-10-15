/*
  TtySerial.cpp - HardwareSerial on a Linux tty.
  Copyright (c) 2026 The edison-revival contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#include "TtySerial.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

TtySerial Serial("/dev/ttyGS0", true);
TtySerial Serial1("/dev/ttyS1", false);

namespace {

speed_t to_speed(unsigned long baud) {
    switch (baud) {
        case 300: return B300;
        case 1200: return B1200;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 500000: return B500000;
        case 921600: return B921600;
        case 1000000: return B1000000;
        case 1500000: return B1500000;
        case 2000000: return B2000000;
        case 3000000: return B3000000;
        case 3500000: return B3500000;
        default: return B115200;
    }
}

}  // namespace

void TtySerial::begin(unsigned long baud, uint16_t config) {
    if (fd_ >= 0) end();
    fd_ = open(path_, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) {
        fprintf(stderr, "Serial: can't open %s: %s\n", path_, strerror(errno));
        return;
    }
    termios t;
    if (tcgetattr(fd_, &t) == 0) {
        cfmakeraw(&t);
        cfsetspeed(&t, to_speed(baud));
        t.c_cflag |= CLOCAL | CREAD;
        t.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB);
        switch (config & SERIAL_DATA_MASK) {
            case SERIAL_DATA_5: t.c_cflag |= CS5; break;
            case SERIAL_DATA_6: t.c_cflag |= CS6; break;
            case SERIAL_DATA_7: t.c_cflag |= CS7; break;
            default: t.c_cflag |= CS8; break;
        }
        switch (config & SERIAL_PARITY_MASK) {
            case SERIAL_PARITY_EVEN: t.c_cflag |= PARENB; break;
            case SERIAL_PARITY_ODD: t.c_cflag |= PARENB | PARODD; break;
            default: break;
        }
        if ((config & SERIAL_STOP_BIT_MASK) == SERIAL_STOP_BIT_2) t.c_cflag |= CSTOPB;
        tcsetattr(fd_, TCSANOW, &t);
    }
    rx_head_ = rx_tail_ = 0;
}

void TtySerial::end() {
    if (fd_ >= 0) close(fd_);
    fd_ = -1;
}

bool TtySerial::fill() {
    if (fd_ < 0) return false;
    if (rx_head_ == rx_tail_) rx_head_ = rx_tail_ = 0;
    if (rx_tail_ == sizeof(rx_)) return rx_head_ != rx_tail_;
    ssize_t n = ::read(fd_, rx_ + rx_tail_, sizeof(rx_) - rx_tail_);
    if (n > 0) rx_tail_ += size_t(n);
    return rx_head_ != rx_tail_;
}

int TtySerial::available() {
    fill();
    return int(rx_tail_ - rx_head_);
}

int TtySerial::peek() { return fill() ? rx_[rx_head_] : -1; }

int TtySerial::read() { return fill() ? rx_[rx_head_++] : -1; }

void TtySerial::flush() {
    if (fd_ >= 0) tcdrain(fd_);
}

size_t TtySerial::write(const uint8_t *buf, size_t size) {
    if (fd_ < 0) return 0;
    size_t done = 0;
    while (done < size) {
        ssize_t n = ::write(fd_, buf + done, size - done);
        if (n > 0) {
            done += size_t(n);
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && errno == EAGAIN) {
            // A USB port with nobody reading would block forever: wait briefly, then drop.
            pollfd p = {fd_, POLLOUT, 0};
            if (poll(&p, 1, usb_ ? 20 : 1000) > 0) continue;
        }
        break;
    }
    return done;
}
