// Servo self-test with a jumper from IO3 to A0: a servo on IO3 at 0, 90 and 180 degrees.
// A0 counts the pulses (attachInterrupt; expect 50/s) and estimates their width from the
// share of high samples (expect 544, 1472 and 2400 us).
#include <Servo.h>

Servo servo;
volatile unsigned long pulses = 0;

void count() { pulses++; }

void setup() {
    Serial.begin(115200);
    pinMode(A0, INPUT);
    Serial.print("attach: ");
    Serial.println(servo.attach(3));
}

void loop() {
    const int angles[] = {0, 90, 180};
    for (int angle : angles) {
        servo.write(angle);
        delay(100);
        attachInterrupt(digitalPinToInterrupt(A0), count, RISING);
        noInterrupts();
        pulses = 0;
        interrupts();
        delay(1000);
        noInterrupts();
        unsigned long n = pulses;
        interrupts();
        detachInterrupt(digitalPinToInterrupt(A0));
        const int samples = 2000;
        int high = 0;
        for (int i = 0; i < samples; i++) {
            if (digitalRead(A0) == HIGH) high++;
            delayMicroseconds(97);
        }
        Serial.print(angle);
        Serial.print(" deg (");
        Serial.print(servo.readMicroseconds());
        Serial.print(" us): ");
        Serial.print(n);
        Serial.print(" pulses/s, measured ~");
        Serial.print(20000L * high / samples);
        Serial.println(" us");
    }
    Serial.println();
}
