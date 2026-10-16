/*
  Servo.cpp - hobby servos through the SoC's hardware PWM (edison_pwm.h).
  Copyright (c) 2026 The open-edison contributors.

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.
*/
#include "Servo.h"

#include <stdio.h>

#include "edison_pwm.h"

Servo::Servo() {}

uint8_t Servo::attach(int pin) { return attach(pin, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH); }

uint8_t Servo::attach(int pin, int min, int max) {
    if (!digitalPinHasPWM(pin)) {
        fprintf(stderr, "Servo: pin %d has no PWM; use 3, 5, 6 or 9\n", pin);
        return INVALID_SERVO;
    }
    if (attached() && pin_ != pin) detach();
    min_ = min;
    max_ = max > min ? max : min + 1;
    if (us_ < min_) us_ = min_;
    if (us_ > max_) us_ = max_;
    pin_ = pin;
    if (!update()) {
        pin_ = -1;
        return INVALID_SERVO;
    }
    // PWM pins 3, 5, 6, 9 are channels 0-3.
    return pin == 3 ? 0 : pin == 5 ? 1 : pin == 6 ? 2 : 3;
}

void Servo::detach() {
    if (!attached()) return;
    edisonPwmStop(uint8_t(pin_));
    pin_ = -1;
}

bool Servo::update() {
    return edisonPwmWrite(uint8_t(pin_), uint32_t(refresh_us_) * 1000, uint32_t(us_) * 1000);
}

void Servo::write(int value) {
    if (value < MIN_PULSE_WIDTH) {
        // An angle.
        if (value < 0) value = 0;
        if (value > 180) value = 180;
        value = map(value, 0, 180, min_, max_);
    }
    writeMicroseconds(value);
}

void Servo::writeMicroseconds(int value) {
    if (value < min_) value = min_;
    if (value > max_) value = max_;
    us_ = value;
    if (attached()) update();
}

int Servo::read() { return map(us_, min_, max_, 0, 180); }

int Servo::readMicroseconds() { return us_; }

bool Servo::attached() { return pin_ >= 0; }

void Servo::setRefreshInterval(int us) {
    if (us < 2500) us = 2500;
    if (us > 50000) us = 50000;
    if (us <= max_) us = max_ + 500;  // room for the widest pulse and a gap
    refresh_us_ = us;
    if (attached()) update();
}
