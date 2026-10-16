/*
  TtySerial.h - HardwareSerial on a Linux tty (/dev/ttyGS0 for Serial, /dev/ttyS1 for
  Serial1).
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#pragma once

#include "api/HardwareSerial.h"

class TtySerial : public arduino::HardwareSerial {
  public:
    // usb: a USB gadget port, where the baud rate doesn't matter and output is dropped
    // (not blocked) while no host has the port open, like on other USB Arduinos.
    TtySerial(const char *path, bool usb) : path_(path), usb_(usb) {}

    void begin(unsigned long baud) override { begin(baud, SERIAL_8N1); }
    void begin(unsigned long baud, uint16_t config) override;
    void end() override;
    int available() override;
    int peek() override;
    int read() override;
    void flush() override;
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t *buf, size_t size) override;
    using Print::write;
    operator bool() override { return fd_ >= 0; }

  private:
    bool fill();  // read whatever is waiting into rx_
    const char *path_;
    bool usb_;
    int fd_ = -1;
    bool stalled_ = false;  // a write timed out: nothing is reading the USB port
    uint8_t rx_[256];
    size_t rx_head_ = 0, rx_tail_ = 0;
};
