/*
 * Throttle.cpp
 *
 * Created: 5/7/2019 5:14:16 PM
 *  Author: troy.honegger
 */ 

#include <Arduino.h>
#include "Common.hpp"
#include "Throttle.hpp"

void Throttle::begin() {
	pinMode(XTD_PIN, OUTPUT);
	digitalWrite(XTD_PIN, OFF_VOLTAGE);
	pinMode(RET_PIN, OUTPUT);
	digitalWrite(RET_PIN, OFF_VOLTAGE);
	pinMode(SENSOR_PIN, INPUT);
}

void Throttle::updateActuatorLength() const {
	// TODO: if we put a resistor on here to limit the min voltage to 1V, remember to change this
	actuatorLength = map(analogRead(SENSOR_PIN), 0, 1024, 0, 100);
}

void Throttle::up() { throttledUp = true; }
void Throttle::down() { throttledUp = false; }

// returns 0 for stop, 1 for extend, -1 for retract
int8_t Throttle::getDesiredDL() const {
	updateActuatorLength();
	uint8_t target = throttledUp ? RET_LEN : XTD_LEN;
	// The following applies a sort of software hysteresis to the control logic.
	// If |target - initial| <= accuracy / 2, the actuator must be stoppped;
	// if |target - initial| > accuracy, the actuator must be started;
	// if accuracy / 2 <= |target - initial| < accuracy, the actuator will maintain the
	// previous state, unless doing so would push it further away from the target

	if (target > actuatorLength) {
		if (target - actuatorLength > ACCURACY) { return 1; }
		else if (target - actuatorLength <= (ACCURACY / 2)) { return 0; }
		else { return dl == 1 ? 1 : 0; }
	}
	else if (target < actuatorLength) {
		if (actuatorLength - target > ACCURACY) { return -1; }
		else if (actuatorLength - target <= (ACCURACY / 2)) { return 0; }
		else { return dl == -1 ? -1 : 0; }
	}
	else { // target == actuatorLength
		return 0;
	}
}

void Throttle::update() {
	int8_t newDL = getDesiredDL();
	if (dl != newDL) {
		dl = newDL;
		switch (newDL) {
			case 1:
				digitalWrite(RET_PIN, OFF_VOLTAGE);
				digitalWrite(XTD_PIN, ON_VOLTAGE);
				break;
			case -1:
				digitalWrite(XTD_PIN, OFF_VOLTAGE);
				digitalWrite(RET_PIN, ON_VOLTAGE);
				break;
			default:
				digitalWrite(XTD_PIN, OFF_VOLTAGE);
				digitalWrite(RET_PIN, OFF_VOLTAGE);
				break;
		}
	}
}
