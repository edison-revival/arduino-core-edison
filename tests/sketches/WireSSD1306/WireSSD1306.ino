// Drive a 128x64 SSD1306 OLED (I2C, address 0x3C) over Wire with no libraries: initialise
// it, then draw diagonal stripes that move, sending the whole 1 KiB screen each frame in
// chunks of up to 255 bytes. Prints the frame rate on Serial.
#include <Wire.h>

const uint8_t OLED = 0x3C;
const int WIDTH = 128, PAGES = 8;  // 64 rows = 8 pages of 8

uint8_t command(uint8_t c) {
    Wire.beginTransmission(OLED);
    Wire.write(0x00);  // control byte: command
    Wire.write(c);
    return Wire.endTransmission();
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    const uint8_t init[] = {
        0xAE,        // display off
        0xD5, 0x80,  // clock
        0xA8, 0x3F,  // multiplex: 64 rows
        0xD3, 0x00,  // display offset
        0x40,        // start line 0
        0x8D, 0x14,  // charge pump on
        0x20, 0x00,  // horizontal addressing
        0xA1, 0xC8,  // segment remap, COM scan direction
        0xDA, 0x12,  // COM pins
        0x81, 0xCF,  // contrast
        0xD9, 0xF1,  // precharge
        0xDB, 0x40,  // VCOM detect
        0xA4, 0xA6,  // resume from RAM, normal (not inverted)
        0xAF,        // display on
    };
    uint8_t errors = 0;
    for (uint8_t c : init) errors |= command(c);
    Serial.print("init: ");
    Serial.println(errors == 0 ? "ok" : "failed (no display at 0x3C?)");
}

void loop() {
    static uint8_t shift = 0;
    static unsigned long last = 0;
    static int frames = 0;
    // Whole screen: columns 0-127, pages 0-7.
    const uint8_t window[] = {0x21, 0, WIDTH - 1, 0x22, 0, PAGES - 1};
    for (uint8_t c : window) command(c);
    uint8_t screen[WIDTH * PAGES];
    for (int i = 0; i < WIDTH * PAGES; i++) {
        int x = i % WIDTH, page = i / WIDTH;
        uint8_t col = 0;
        for (int bit = 0; bit < 8; bit++) {
            if (((x + page * 8 + bit + shift) / 8) % 2) col |= 1 << bit;
        }
        screen[i] = col;
    }
    for (int i = 0; i < WIDTH * PAGES; i += 255) {
        Wire.beginTransmission(OLED);
        Wire.write(0x40);  // control byte: data
        int n = min(255, WIDTH * PAGES - i);
        Wire.write(screen + i, n);
        if (Wire.endTransmission() != 0) {
            Serial.println("data write failed");
            delay(1000);
            return;
        }
    }
    shift++;
    frames++;
    if (millis() - last >= 2000) {
        Serial.print("frames/s: ");
        Serial.println(frames * 1000.0 / (millis() - last), 1);
        frames = 0;
        last = millis();
    }
}
