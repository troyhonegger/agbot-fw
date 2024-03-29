/*
 * Estop.cpp
 * Implements the Estop class defined in Estop.h for triggering the machine's e-stop.
 * See Estop.h for more info.
 * Created: 3/6/2019 3:14:52 PM
 *  Author: troy.honegger
 */

#include "Common.h"
#include "Estop.h"

Estop::Estop() : whenEngaged(0), engaged(false) { }

void Estop::begin() {
	pinMode(Estop::HW_PIN, OUTPUT);
	digitalWrite(Estop::HW_PIN, HIGH);
}

void Estop::engage() {
	whenEngaged = millis();
	engaged = true;
	digitalWrite(Estop::HW_PIN, LOW);
}

void Estop::update() {
	if (engaged && isElapsed(whenEngaged + Estop::PULSE_LEN)) {
		engaged = false;
		digitalWrite(Estop::HW_PIN, HIGH);
	}
}
