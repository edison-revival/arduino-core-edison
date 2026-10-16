// Read an MPU6050 (e.g. a GY-521 board, address 0x68) over Wire with plain register
// accesses, no libraries: WHO_AM_I, then the accelerometer and temperature twice a second.
// Exercises write-then-read with a repeated start, as sensor libraries do.
#include <Wire.h>

const uint8_t MPU = 0x68;

uint8_t read_reg(uint8_t reg) {
    Wire.beginTransmission(MPU);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU, size_t(1));
    return Wire.available() ? Wire.read() : 0xFF;
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    delay(100);
    // Wake it from sleep (PWR_MGMT_1 = 0).
    Wire.beginTransmission(MPU);
    Wire.write(0x6B);
    Wire.write(0);
    Serial.print("wake: ");
    Serial.println(Wire.endTransmission());
}

void loop() {
    Serial.print("WHO_AM_I=0x");
    Serial.print(read_reg(0x75), HEX);
    // ACCEL_XOUT_H .. TEMP_OUT_L: 8 bytes from 0x3B in one read.
    Wire.beginTransmission(MPU);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    if (Wire.requestFrom(MPU, size_t(8)) == 8) {
        int16_t v[4];
        for (int i = 0; i < 4; i++) v[i] = int16_t((Wire.read() << 8) | Wire.read());
        // +-2 g full scale: 16384 per g. Temperature: raw / 340 + 36.53 C.
        Serial.print(" accel g: ");
        Serial.print(v[0] / 16384.0, 2);
        Serial.print(" ");
        Serial.print(v[1] / 16384.0, 2);
        Serial.print(" ");
        Serial.print(v[2] / 16384.0, 2);
        Serial.print(" temp C: ");
        Serial.print(v[3] / 340.0 + 36.53, 1);
    } else {
        Serial.print(" read failed");
    }
    Serial.println();
    delay(500);
}
