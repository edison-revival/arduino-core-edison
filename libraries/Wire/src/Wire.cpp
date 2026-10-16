/*
  Wire.cpp - I2C (Wire) through Linux's i2c-dev.
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#include "Wire.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

// endTransmission() results, as on AVR.
static const uint8_t OK = 0, TOO_LONG = 1, NACK_ADDRESS = 2, OTHER = 4, TIMEOUT = 5;

TwoWire::TwoWire(const char *device) : device_(device) {}

void TwoWire::warn_once(bool &done, const char *what) {
    if (done) return;
    done = true;
    fprintf(stderr, "Wire: %s\n", what);
}

void TwoWire::begin() {
    if (fd_ >= 0) return;
    fd_ = open(device_, O_RDWR | O_CLOEXEC);
    if (fd_ < 0 && !warned_open_) {
        warned_open_ = true;
        fprintf(stderr, "Wire: can't open %s: %s\n", device_, strerror(errno));
    }
}

void TwoWire::begin(uint8_t) {
    warn_once(warned_target_, "target (slave) mode isn't supported");
    begin();
}

void TwoWire::end() {
    if (fd_ >= 0) close(fd_);
    fd_ = -1;
    pending_ = false;
}

void TwoWire::setClock(uint32_t) {}

void TwoWire::onReceive(void (*)(int)) {
    warn_once(warned_target_, "target (slave) mode isn't supported");
}

void TwoWire::onRequest(void (*)(void)) {
    warn_once(warned_target_, "target (slave) mode isn't supported");
}

// One combined transfer: write out (if any), then read in_len bytes (if any) into rx_,
// with a repeated start between them and a stop at the end.
uint8_t TwoWire::transfer(uint8_t address, const std::vector<uint8_t> &out, size_t in_len) {
    if (fd_ < 0) begin();
    if (fd_ < 0) return OTHER;
    if (out.size() > BUFFER_LENGTH || in_len > BUFFER_LENGTH) return TOO_LONG;

    std::vector<uint8_t> wbuf(out);
    rx_.assign(in_len, 0);
    rx_pos_ = 0;
    i2c_msg msgs[2];
    int n = 0;
    if (!wbuf.empty()) msgs[n++] = {address, 0, uint16_t(wbuf.size()), wbuf.data()};
    if (in_len) msgs[n++] = {address, I2C_M_RD, uint16_t(in_len), rx_.data()};
    if (n == 0) {
        // An empty write, as bus scanners send to see whether an address answers. The
        // Edison's I2C controller can't send zero-length transfers: probe with a one-byte
        // read instead, as i2cdetect -r does.
        static uint8_t probe;
        msgs[n++] = {address, I2C_M_RD, 1, &probe};
    }
    i2c_rdwr_ioctl_data data = {msgs, uint32_t(n)};
    if (ioctl(fd_, I2C_RDWR, &data) >= 0) return OK;
    int err = errno;
    rx_.clear();
    switch (err) {
        case ENXIO:      // no ACK (designware)
        case EREMOTEIO:  // no ACK
            return NACK_ADDRESS;
        case ETIMEDOUT:
            return TIMEOUT;
        default:
            return OTHER;
    }
}

// Send a write held for a repeated start that didn't come.
uint8_t TwoWire::flush_pending() {
    if (!pending_) return OK;
    pending_ = false;
    return transfer(pending_address_, pending_tx_, 0);
}

void TwoWire::beginTransmission(uint8_t address) {
    flush_pending();
    transmitting_ = true;
    tx_address_ = address;
    tx_.clear();
}

uint8_t TwoWire::endTransmission(bool stopBit) {
    if (!transmitting_) return OTHER;
    transmitting_ = false;
    if (tx_.size() > BUFFER_LENGTH) return TOO_LONG;
    if (!stopBit) {
        // Held for the requestFrom() that usually follows (a register read); sent then, or
        // on its own by the next transmission.
        pending_ = true;
        pending_address_ = tx_address_;
        pending_tx_ = tx_;
        return OK;
    }
    return transfer(tx_address_, tx_, 0);
}

uint8_t TwoWire::endTransmission(void) { return endTransmission(true); }

size_t TwoWire::requestFrom(uint8_t address, size_t len, bool) {
    if (len == 0) return 0;
    if (len > BUFFER_LENGTH) len = BUFFER_LENGTH;
    std::vector<uint8_t> out;
    if (pending_ && pending_address_ == address) {
        out.swap(pending_tx_);
        pending_ = false;
    } else {
        flush_pending();
    }
    return transfer(address, out, len) == OK ? len : 0;
}

size_t TwoWire::requestFrom(uint8_t address, size_t len) { return requestFrom(address, len, true); }

size_t TwoWire::write(uint8_t data) {
    if (!transmitting_ || tx_.size() >= BUFFER_LENGTH) {
        setWriteError();
        return 0;
    }
    tx_.push_back(data);
    return 1;
}

size_t TwoWire::write(const uint8_t *data, size_t len) {
    size_t n = 0;
    while (n < len && write(data[n])) n++;
    return n;
}

int TwoWire::available() { return int(rx_.size() - rx_pos_); }

int TwoWire::read() { return rx_pos_ < rx_.size() ? rx_[rx_pos_++] : -1; }

int TwoWire::peek() { return rx_pos_ < rx_.size() ? rx_[rx_pos_] : -1; }

// I2C-6: the header's SDA/SCL (A4/A5), as routed by edison-image's ACPI tables.
TwoWire Wire("/dev/i2c-6");
