// analogWrite() self-test with a jumper from IO3 to A0: step IO3's PWM through several
// duty cycles and estimate each by sampling A0 (each reading catches the 490 Hz PWM at a
// random point, so the share of high readings approaches the duty cycle). Then take IO3
// back with pinMode() and check digitalWrite() on it.
void setup() { Serial.begin(115200); }

int high_percent() {
    const int samples = 400;
    int high = 0;
    for (int i = 0; i < samples; i++) {
        if (analogRead(A0) > 512) high++;
        delayMicroseconds(137);  // not a multiple of the PWM period
    }
    return high * 100 / samples;
}

void loop() {
    const int values[] = {0, 32, 64, 128, 192, 255};
    for (int v : values) {
        analogWrite(3, v);
        delay(50);
        Serial.print("analogWrite ");
        Serial.print(v);
        Serial.print(" (expect ");
        Serial.print(v * 100 / 255);
        Serial.print("%): measured ");
        Serial.print(high_percent());
        Serial.println("%");
    }
    pinMode(3, OUTPUT);
    digitalWrite(3, HIGH);
    delay(20);
    Serial.print("digitalWrite HIGH: A0=");
    Serial.print(analogRead(A0));
    digitalWrite(3, LOW);
    delay(20);
    Serial.print(", LOW: A0=");
    Serial.println(analogRead(A0));
    Serial.println();
    delay(3000);
}
