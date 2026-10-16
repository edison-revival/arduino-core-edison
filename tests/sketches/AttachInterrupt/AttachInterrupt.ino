// attachInterrupt() self-test with a jumper from IO3 to A0: IO3 makes a 490 Hz square wave
// (analogWrite), and A0, as a digital input, counts its edges for a second in each mode.
// Expect about 490/s for RISING and FALLING, 980/s for CHANGE, and 0 once detached.
volatile unsigned long edges = 0;

void count() { edges++; }

unsigned long count_for_a_second() {
    noInterrupts();
    edges = 0;
    interrupts();
    delay(1000);
    noInterrupts();
    unsigned long n = edges;
    interrupts();
    return n;
}

void setup() {
    Serial.begin(115200);
    analogWrite(3, 128);
    pinMode(A0, INPUT);
}

void loop() {
    const struct {
        PinStatus mode;
        const char *name;
    } modes[] = {{RISING, "RISING"}, {FALLING, "FALLING"}, {CHANGE, "CHANGE"}};
    for (auto m : modes) {
        attachInterrupt(digitalPinToInterrupt(A0), count, m.mode);
        Serial.print(m.name);
        Serial.print(": ");
        Serial.print(count_for_a_second());
        Serial.println("/s");
    }
    detachInterrupt(digitalPinToInterrupt(A0));
    Serial.print("detached: ");
    Serial.print(count_for_a_second());
    Serial.println("/s");
    Serial.println();
}
