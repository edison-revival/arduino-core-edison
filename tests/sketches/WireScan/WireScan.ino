// Scan the I2C bus on SDA/SCL every 5 seconds and print the addresses that answer.
#include <Wire.h>

void setup() {
    Serial.begin(115200);
    Wire.begin();
}

void loop() {
    int found = 0;
    Serial.print("I2C devices:");
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t result = Wire.endTransmission();
        if (result == 0) {
            Serial.print(" 0x");
            Serial.print(address, HEX);
            found++;
        } else if (result != 2) {
            Serial.print(" (0x");
            Serial.print(address, HEX);
            Serial.print(": error ");
            Serial.print(result);
            Serial.print(")");
        }
    }
    Serial.println(found ? "" : " none");
    delay(5000);
}
