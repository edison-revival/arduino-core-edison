# arduino-core-edison

An Arduino core for the Intel Edison on the Arduino expansion board, for boards running
[edison-image](https://github.com/open-edison/edison-image) (Debian 13). Part of
[open-edison](https://github.com/open-edison).

**Status: early.** Works: `pinMode`/`digitalWrite`/`digitalRead` on the header pins,
`Serial` (USB) and `Serial1` (IO0/IO1), `millis`/`micros`/delays, upload over USB or the
network, `analogRead` on A0-A3, `Wire` (I2C). Not yet: `analogWrite` (PWM), `SPI`,
interrupts.

## Installing

**Arduino IDE 2.x:** add this URL under File → Preferences → Additional boards manager URLs,
then install "Intel Edison" by open-edison in the Boards Manager:

```
https://github.com/open-edison/arduino-core-edison/releases/latest/download/package_open-edison_index.json
```

**PlatformIO:** use [platform-edison](https://github.com/open-edison/platform-edison),
which installs the same release archives.

The board needs [edison-image](https://github.com/open-edison/edison-image) v0.2.0 or
later (it runs the sketch). Its
[hardware guide](https://github.com/open-edison/edison-image/blob/main/docs/hardware.md)
covers the board's jumpers (IOREF, AREF, PWM pins), buttons and quirks.

## How it works

A sketch is compiled on your computer into a Linux program for the Edison (by Zig's clang,
so the same toolchain works on Windows, macOS and Linux) and uploaded to
`/var/lib/edison-sketch/sketch.elf`. edison-image runs it as the `edison-sketch` service,
as the `edison` user, and restarts it when a new one arrives.

- **Pins** are driven through the Linux GPIO character devices (libgpiod). For each pin the
  core also sets the kit's level shifter direction, pull resistor and muxes, which are
  described in [variants/edison/variant.cpp](variants/edison/variant.cpp).
- Pins owned by a peripheral are refused, with a message in the sketch's log
  (`journalctl -u edison-sketch`): IO0/IO1 (UART1, `Serial1`) and A4/A5 (I2C-6, `Wire`).
- **Serial** is the USB serial port on J16 (`/dev/ttyGS0` on the board). Output is dropped
  while nothing on the computer reads the port, as on other USB Arduinos; the last few
  KiB from before can still arrive when the port is opened.
- **analogRead** uses the kit's 12-bit ADC (TI ADS7951) on A0-A3, returning 0-1023 by
  default (`analogReadResolution(12)` for 0-4095). Full scale is IOREF, the 5 V or 3.3 V
  set by jumper J9 (with J8 on IOREF); on USB power the 5 V rail, and so IOREF, is nearer
  4.5 V. `analogReference()` does nothing, as the reference is set by the jumpers. A4/A5
  are the I2C pins, so they can't be analog inputs. Needs edison-image v0.2.4 or later:
  on earlier images the ADC is left in its half range, and readings saturate above about
  half of IOREF.
- **Wire** is I2C-6 on SDA/SCL (A4/A5), through Linux's i2c-dev: controller mode only.
  `endTransmission(false)` followed by `requestFrom()` (a register read) is sent as one
  transfer with a repeated start. The bus speed is set by the kernel; `setClock()` does
  nothing.
- When the sketch stops, its pins go back to inputs.

## Uploading

Pick either port in the IDE (or `arduino-cli upload -p`):

- **The board's USB serial port** (J16): the upload goes over the USB network link on the
  same cable (to `192.168.2.15`), and the Serial Monitor works on the same port. Only one
  Edison can be connected by USB at a time, as they all use that address.
- **Its network port** (found by mDNS, over WiFi or USB): as with other boards' network
  uploads, the Serial Monitor isn't available on it.

The upload is done over SSH by `edison-upload` ([tools/edison-upload](tools/edison-upload)). The first upload from a computer asks for the `edison` user's password, and installs
a key for that computer (kept in `edison-upload` under your user config directory), so
later uploads don't need it. The IDE still asks once for each sketch you open and won't
accept an empty field, so type anything there: with the key installed, it's ignored. On the network, the board's host key is remembered on first use; if it
changes (another board at that address), `edison-upload -forget ADDRESS` clears it.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for building from source, tests, releasing and
conventions.

## License

LGPL-2.1-or-later, like the [ArduinoCore-API](https://github.com/arduino/ArduinoCore-API)
it builds on (vendored in `cores/edison/api`, from commit `0f4e57e`). See
[LICENSE](LICENSE).
