/*
 * Estop.cpp
 *
 * Created: 3/6/2019 3:14:52 PM
 *  Author: troy.honegger
 */ 

#include "Common.hpp"
#include "Estop.hpp"

namespace agbot{
	Estop::Estop() : whenEngaged(0), engaged(false) { }

	void Estop::begin() {
		pinMode(Estop::HW_PIN, INPUT_PULLUP);
	}

	bool Estop::isEngaged() const {
		if (engaged) { return true; }
		else { return digitalRead(Estop::HW_PIN) == LOW; }
	}

	void Estop::engage() {
		whenEngaged = millis();
		engaged = true;
		digitalWrite(Estop::HW_PIN, LOW);
		pinMode(Estop::HW_PIN, OUTPUT);
	}

	void Estop::update() {
		if (engaged && isElapsed(whenEngaged + Estop::PULSE_LEN)) {
			engaged = false;
			pinMode(Estop::HW_PIN, INPUT_PULLUP);
		}
	}
}