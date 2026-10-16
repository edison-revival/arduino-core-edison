/*
  SPI.cpp - SPI through Linux's spidev (SPI5).
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#include "SPI.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <vector>

#include "gpio.h"
#include "variant.h"

namespace {

uint8_t reverse(uint8_t b) {
    b = uint8_t((b & 0xF0) >> 4 | (b & 0x0F) << 4);
    b = uint8_t((b & 0xCC) >> 2 | (b & 0x33) << 2);
    return uint8_t((b & 0xAA) >> 1 | (b & 0x55) << 1);
}

}  // namespace

EdisonSPI::EdisonSPI(const char *device) : device_(device) {}

void EdisonSPI::begin() {
    if (fd_ >= 0) return;
    if (!edison_spi_pins_enable(true)) return;
    fd_ = open(device_, O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
        if (!warned_) {
            warned_ = true;
            fprintf(stderr, "SPI: can't open %s: %s (needs edison-image v0.2.5 or later)\n",
                    device_, strerror(errno));
        }
        edison_spi_pins_enable(false);
        return;
    }
    // The device's own settings (the chip select polarity, in particular).
    if (ioctl(fd_, SPI_IOC_RD_MODE32, &base_mode_) < 0) {
        uint8_t m8 = 0;
        ioctl(fd_, SPI_IOC_RD_MODE, &m8);
        base_mode_ = m8;
    }
    // The device already has a clock mode; don't set it again unless it changes.
    applied_mode_ = uint8_t(base_mode_ & SPI_MODE_3);
}

void EdisonSPI::end() {
    if (fd_ < 0) return;
    close(fd_);
    fd_ = -1;
    edison_spi_pins_enable(false);
}

void EdisonSPI::beginTransaction(arduino::SPISettings settings) {
    clock_ = settings.getClockFreq();
    mode_ = uint8_t(settings.getDataMode());
    lsb_first_ = settings.getBitOrder() == LSBFIRST;
}

void EdisonSPI::setBitOrder(BitOrder order) { lsb_first_ = order == LSBFIRST; }

void EdisonSPI::setDataMode(uint8_t mode) { mode_ = mode & 3; }

void EdisonSPI::setClockDivider(uint8_t divider) {
    if (divider) clock_ = 16000000 / divider;
}

// The SPI mode is a property of the device file; change it only when it changes.
bool EdisonSPI::apply() {
    if (fd_ < 0) begin();
    if (fd_ < 0) return false;
    if (mode_ != applied_mode_) {
        // Keep the device's chip select polarity (SPI_CS_HIGH, from edison-image's overlay):
        // only the clock mode is the sketch's to set.
        uint32_t mode = (base_mode_ & ~uint32_t(SPI_MODE_3)) | mode_;
        if (ioctl(fd_, SPI_IOC_WR_MODE32, &mode) < 0) {
            uint8_t m8 = uint8_t(mode);
            if (ioctl(fd_, SPI_IOC_WR_MODE, &m8) < 0) return false;
        }
        applied_mode_ = mode_;
    }
    return true;
}

void EdisonSPI::transfer(const void *tx, void *rx, size_t count) {
    if (!count || !apply()) {
        if (rx) memset(rx, 0, count);
        return;
    }
    // Bit order is done here: the controller sends most significant bit first.
    std::vector<uint8_t> out(count, 0), in(count, 0);
    if (tx) memcpy(out.data(), tx, count);
    if (lsb_first_) {
        for (uint8_t &b : out) b = reverse(b);
    }
    // The SPI controller's driver uses DMA for transfers of 8 bytes or more, and that hangs
    // the board (the transfer times out, then a CPU stalls). Below 8 bytes it doesn't: send
    // 7-byte segments, many to one message (one system call), with the chip select held
    // between them, so the device sees one transfer. A message holds at most 511 segments
    // (the ioctl's size field), within spidev's 4096-byte buffer.
    const size_t segment = 7, per_message = 511 * segment;
    std::vector<spi_ioc_transfer> segs;
    for (size_t msg = 0; msg < count; msg += per_message) {
        size_t end = count - msg < per_message ? count : msg + per_message;
        segs.clear();
        for (size_t off = msg; off < end; off += segment) {
            spi_ioc_transfer t = {};
            t.tx_buf = reinterpret_cast<uintptr_t>(out.data() + off);
            t.rx_buf = reinterpret_cast<uintptr_t>(in.data() + off);
            t.len = uint32_t(end - off < segment ? end - off : segment);
            t.speed_hz = clock_;
            t.bits_per_word = 8;
            segs.push_back(t);
        }
        unsigned long request = _IOC(_IOC_WRITE, SPI_IOC_MAGIC, 0, SPI_MSGSIZE(segs.size()));
        if (ioctl(fd_, request, segs.data()) < 0) {
            if (!warned_) {
                warned_ = true;
                fprintf(stderr, "SPI: transfer failed: %s\n", strerror(errno));
            }
            break;
        }
    }
    if (rx) {
        if (lsb_first_) {
            for (uint8_t &b : in) b = reverse(b);
        }
        memcpy(rx, in.data(), count);
    }
}

void EdisonSPI::transfer(void *buf, size_t count) { transfer(buf, buf, count); }

uint8_t EdisonSPI::transfer(uint8_t data) {
    uint8_t in = 0;
    transfer(&data, &in, 1);
    return in;
}

uint16_t EdisonSPI::transfer16(uint16_t data) {
    // As on AVR: most significant byte first, unless the bit order is LSB first.
    uint8_t out[2], in[2];
    if (lsb_first_) {
        out[0] = uint8_t(data);
        out[1] = uint8_t(data >> 8);
    } else {
        out[0] = uint8_t(data >> 8);
        out[1] = uint8_t(data);
    }
    transfer(out, in, 2);
    return lsb_first_ ? uint16_t(in[0] | in[1] << 8) : uint16_t(in[0] << 8 | in[1]);
}

EdisonSPI SPI(edison_spi_device);
