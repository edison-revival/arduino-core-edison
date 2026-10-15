# arduino-core-edison

An Arduino core for the Intel Edison on the Arduino expansion board, for boards running
[edison-image](https://github.com/edison-revival/edison-image) (Debian 13). Part of
[edison-revival](https://github.com/edison-revival).

**Status: early.** Works: `pinMode`/`digitalWrite`/`digitalRead` on the header pins,
`Serial` (USB) and `Serial1` (IO0/IO1), `millis`/`micros`/delays, upload over USB or the
network. Not yet: `analogRead`, `analogWrite` (PWM), `Wire`, `SPI`, interrupts.

## Installing

**Arduino IDE 2.x:** add this URL under File → Preferences → Additional boards manager URLs,
then install "Intel Edison (edison-revival)" in the Boards Manager:

```
https://github.com/edison-revival/arduino-core-edison/releases/latest/download/package_edison-revival_index.json
```

**PlatformIO:** use [platform-edison](https://github.com/edison-revival/platform-edison),
which installs the same release archives.

The board needs [edison-image](https://github.com/edison-revival/edison-image) v0.2.0 or
later (it runs the sketch).

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
later uploads don't need it; the IDE still asks once per session, and the field can then
be left empty. On the network, the board's host key is remembered on first use; if it
changes (another board at that address), `edison-upload -forget ADDRESS` clears it.

## Building from source

You need [arduino-cli](https://arduino.github.io/arduino-cli/) (or the Arduino IDE),
[Zig 0.17.0](https://ziglang.org/download/) and Docker (to build the libgpiod sysroot and
the uploader).

1. Build the sysroot and the tools (`edison-upload`, and `edison-cc`, a small wrapper the
   build uses): `tools/make-sysroot.sh` and `tools/build-tools.sh` (they write `dist/`).
2. Put this repository (or a link to it) at `<sketchbook>/hardware/edison-revival/edison`.
3. Create `platform.local.txt` next to `platform.txt`, pointing at the tools (forward
   slashes, and a trailing slash on `compiler.path`; pick your computer's tools folder):

   ```
   compiler.path=/path/to/zig-x86_64-linux-0.17.0/
   compiler.sysroot=/path/to/arduino-core-edison/dist/edison-sysroot
   edison.tools.path=/path/to/arduino-core-edison/dist/edison-tools/linux-amd64
   ```

4. `arduino-cli compile -b edison-revival:edison:edison tests/sketches/Blink`, then
   `arduino-cli upload -b edison-revival:edison:edison -p PORT tests/sketches/Blink`.

**Release archives:** after step 1, `tools/package.sh VERSION [PREVIOUS_INDEX]` writes the
Boards Manager and PlatformIO archives and the index to `dist/release/`, as CI does
(`.github/workflows/build.yml`). It downloads Zig from ziglang.org and checks its
checksums; put the archives in `downloads/` to build without that site. Pushing a `v*` tag
whose version matches `platform.txt` publishes a release (`release.yml`).

## License

LGPL-2.1-or-later, like the [ArduinoCore-API](https://github.com/arduino/ArduinoCore-API)
it builds on (vendored in `cores/edison/api`, from commit `0f4e57e`). See
[LICENSE](LICENSE).
