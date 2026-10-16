// SPI self-test with a jumper from MOSI (11) to MISO (12): everything sent comes back.
// Checks 256-byte transfers in all four modes at several clock rates, then single bytes,
// transfer16() and LSB-first order.
#include <SPI.h>

void setup() {
    Serial.begin(115200);
    SPI.begin();
}

bool check(uint32_t clock, uint8_t mode) {
    uint8_t buf[256], expect[256];
    for (int i = 0; i < 256; i++) expect[i] = buf[i] = uint8_t(i * 7 + mode);
    SPI.beginTransaction(SPISettings(clock, MSBFIRST, mode));
    SPI.transfer(buf, sizeof(buf));
    SPI.endTransaction();
    return memcmp(buf, expect, sizeof(buf)) == 0;
}

void loop() {
    const uint32_t clocks[] = {100000, 1000000, 4000000, 10000000, 25000000};
    int failures = 0;
    for (uint32_t clock : clocks) {
        Serial.print(clock / 1000);
        Serial.print(" kHz:");
        for (uint8_t mode = 0; mode < 4; mode++) {
            bool ok = check(clock, mode);
            if (!ok) failures++;
            Serial.print(ok ? " ok" : " FAIL");
        }
        Serial.println();
    }
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    uint8_t b = SPI.transfer(0xA5);
    uint16_t w = SPI.transfer16(0x1234);
    SPI.endTransaction();
    SPI.beginTransaction(SPISettings(1000000, LSBFIRST, SPI_MODE0));
    uint8_t lsb = SPI.transfer(0x81);
    SPI.endTransaction();
    bool small_ok = b == 0xA5 && w == 0x1234 && lsb == 0x81;
    if (!small_ok) failures++;
    Serial.print("byte 0x");
    Serial.print(b, HEX);
    Serial.print(", word 0x");
    Serial.print(w, HEX);
    Serial.print(", LSB-first 0x");
    Serial.print(lsb, HEX);
    Serial.println(small_ok ? " ok" : " FAIL");
    Serial.println(failures ? "SPI loopback: FAILED" : "SPI loopback: all ok");
    Serial.println();
    delay(3000);
}
