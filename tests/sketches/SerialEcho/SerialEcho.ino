// Report uptime and pin 2 (pulled up) once a second on the USB serial port, and echo back
// whatever is received.
const unsigned long INTERVAL = 1000;
unsigned long last = 0;

void setup() {
    Serial.begin(115200);
    pinMode(2, INPUT_PULLUP);
}

void loop() {
    if (millis() - last >= INTERVAL) {
        last += INTERVAL;
        Serial.print("millis=");
        Serial.print(millis());
        Serial.print(" pin2=");
        Serial.println(digitalRead(2));
    }
    while (Serial.available()) Serial.write(Serial.read());
    delay(1);
}
