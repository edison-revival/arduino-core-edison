// Print A0-A3 once a second on the USB serial port: 10-bit (0-1023, the default) and
// 12-bit (the ADC's full resolution). Full scale is IOREF (5 V or 3.3 V, set by J9).
void setup() { Serial.begin(115200); }

void loop() {
    analogReadResolution(10);
    for (int pin = A0; pin <= A3; pin++) {
        Serial.print("A");
        Serial.print(pin - A0);
        Serial.print("=");
        Serial.print(analogRead(pin));
        Serial.print(" ");
    }
    analogReadResolution(12);
    Serial.print(" 12-bit:");
    for (int pin = A0; pin <= A3; pin++) {
        Serial.print(" ");
        Serial.print(analogRead(pin));
    }
    Serial.println();
    delay(1000);
}
