# arduino-core-edison

An Arduino core for the Intel Edison on the Arduino expansion board, for boards running
[edison-image](https://github.com/edison-revival/edison-image) (Debian 13). Part of
[edison-revival](https://github.com/edison-revival).

**Status: early.** Works: `pinMode`/`digitalWrite`/`digitalRead` on the header pins,
`Serial` (USB) and `Serial1` (IO0/IO1), `millis`/`micros`/delays. Not yet: upload from the
IDE, `analogRead`, `analogWrite` (PWM), `Wire`, `SPI`, interrupts, the Boards Manager
package.

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
  while nothing on the computer has the port open, as on other USB Arduinos.
- When the sketch stops, its pins go back to inputs.

## Building from source

You need [arduino-cli](https://arduino.github.io/arduino-cli/), [Zig
0.17.0](https://ziglang.org/download/) and Docker (to build the libgpiod sysroot).

1. Build the sysroot: `tools/make-sysroot.sh` (writes `dist/edison-sysroot/`).
2. Put this repository (or a link to it) at `<sketchbook>/hardware/edison-revival/edison`.
3. Create `platform.local.txt` next to `platform.txt`, pointing at the tools (forward
   slashes, and a trailing slash on `compiler.path`):

   ```
   compiler.path=/path/to/zig-x86_64-linux-0.17.0/
   compiler.sysroot=/path/to/arduino-core-edison/dist/edison-sysroot
   ```

4. `arduino-cli compile -b edison-revival:edison:edison tests/sketches/Blink`

Until the uploader exists, copy the `.elf` to the board yourself:

```sh
scp Blink.ino.elf edison@edison.local:/var/lib/edison-sketch/.sketch.elf.tmp
ssh edison@edison.local 'cd /var/lib/edison-sketch && chmod 755 .sketch.elf.tmp && mv .sketch.elf.tmp sketch.elf'
```

## License

LGPL-2.1-or-later, like the [ArduinoCore-API](https://github.com/arduino/ArduinoCore-API)
it builds on (vendored in `cores/edison/api`, from commit `0f4e57e`). See
[LICENSE](LICENSE).
