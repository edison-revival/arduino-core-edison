/*
  Wire.h - I2C (Wire) on the Arduino header's SDA/SCL, through Linux's i2c-dev (I2C-6).
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.

  Controller (master) mode only. Linux doesn't hold the bus between calls, so a repeated
  start works for the usual pattern, endTransmission(false) then requestFrom(), which is
  sent as one combined transfer. The bus speed is set by the kernel, not setClock().
*/
#pragma once

#include <Arduino.h>

#include <vector>

#include "api/HardwareI2C.h"

// The largest transfer, for libraries that size their transfers by it. Linux takes larger
// ones; this is the largest buffer here.
#define BUFFER_LENGTH 256

class TwoWire : public arduino::HardwareI2C {
  public:
    explicit TwoWire(const char *device);

    void begin() override;
    void begin(uint8_t address) override;  // target (slave) mode: not supported
    void end() override;

    void setClock(uint32_t freq) override;  // the kernel sets the bus speed

    void beginTransmission(uint8_t address) override;
    uint8_t endTransmission(bool stopBit) override;
    uint8_t endTransmission(void) override;

    size_t requestFrom(uint8_t address, size_t len, bool stopBit) override;
    size_t requestFrom(uint8_t address, size_t len) override;

    void onReceive(void (*)(int)) override;  // target mode: not supported
    void onRequest(void (*)(void)) override;

    size_t write(uint8_t data) override;
    size_t write(const uint8_t *data, size_t len) override;
    // As on AVR: Wire.write(0) and other integer literals pick these.
    size_t write(unsigned long n) { return write(uint8_t(n)); }
    size_t write(long n) { return write(uint8_t(n)); }
    size_t write(unsigned int n) { return write(uint8_t(n)); }
    size_t write(int n) { return write(uint8_t(n)); }
    using Print::write;

    int available() override;
    int read() override;
    int peek() override;
    void flush() override {}

  private:
    uint8_t transfer(uint8_t address, const std::vector<uint8_t> &out, size_t in_len);
    uint8_t flush_pending();
    void warn_once(bool &done, const char *what);

    const char *device_;
    int fd_ = -1;
    bool transmitting_ = false;
    uint8_t tx_address_ = 0;
    std::vector<uint8_t> tx_;
    // A write ended without a stop: held to send with the next requestFrom().
    bool pending_ = false;
    uint8_t pending_address_ = 0;
    std::vector<uint8_t> pending_tx_;
    std::vector<uint8_t> rx_;
    size_t rx_pos_ = 0;
    bool warned_open_ = false, warned_target_ = false;
};

extern TwoWire Wire;
