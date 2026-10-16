// Sweep a servo on pin 9 back and forth between 0 and 180 degrees.
//
// Wiring: the servo's signal to ~9, its ground to GND, its power to 5V. A servo can draw
// more than USB power allows (the board may reset as it moves): power the board from its
// DC jack, or the servo from its own 5 V supply with its ground joined to the board's.
#include <Servo.h>

Servo servo;

void setup() {
    servo.attach(9);
    // Most hobby servos move between about 1000 and 2000 us; for the full range of yours,
    // give its limits: servo.attach(9, 1000, 2000);
}

void loop() {
    for (int angle = 0; angle <= 180; angle++) {
        servo.write(angle);
        delay(15);
    }
    for (int angle = 180; angle >= 0; angle--) {
        servo.write(angle);
        delay(15);
    }
}
