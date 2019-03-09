/*
 * Hitch.cpp
 * Implements the agbot::Hitch class defined in Hitch.hpp for interacting with the hitch on the machine.
 * See Hitch.hpp for more info.
 * Created: 3/9/2019 2:45:06 PM
 *  Author: troy.honegger
 */ 

#include <Arduino.h>
#include "Common.hpp"
#include "Config.hpp"
#include "Hitch.hpp"

namespace agbot {
	inline uint8_t Hitch::getActualHeight() const {
		actualHeight = map(analogRead(HEIGHT_SENSOR_PIN), 0, 1023, 0, MAX_HEIGHT);
		return actualHeight;
	}

	void Hitch::begin(Config const* config) {
		Hitch::config = config;
		digitalWrite(RAISE_PIN, OFF_VOLTAGE);
		pinMode(RAISE_PIN, OUTPUT);
		digitalWrite(LOWER_PIN, OFF_VOLTAGE);
		pinMode(LOWER_PIN, OUTPUT);
		pinMode(HEIGHT_SENSOR_PIN, INPUT);
		targetHeight = STOP;
		dh = 0;
	}

	bool Hitch::needsUpdate() const {
		int8_t computedDh = 0;
		if (targetHeight != STOP) {
			int8_t diff = getActualHeight() - targetHeight;
			if (diff > config->get(Setting::HitchAccuracy)) { computedDh = 1; }
			else if (diff < config->get(Setting::HitchAccuracy)) { computedDh = -1; }
		}

		return computedDh == dh;
	}

	void Hitch::update() {
		int8_t newDh = 0;
		if (targetHeight != STOP) {
			int8_t diff = getActualHeight() - targetHeight;
			if (diff > config->get(Setting::HitchAccuracy)) { newDh = 1; }
			else if (diff < config->get(Setting::HitchAccuracy)) { newDh = -1; }
		}

		dh = newDh;

		switch (newDh) {
			case -1:
				digitalWrite(RAISE_PIN, OFF_VOLTAGE);
				digitalWrite(LOWER_PIN, ON_VOLTAGE);
				break;
			case 0:
				digitalWrite(RAISE_PIN, OFF_VOLTAGE);
				digitalWrite(LOWER_PIN, ON_VOLTAGE);
				break;
			case 1:
				digitalWrite(LOWER_PIN, OFF_VOLTAGE);
				digitalWrite(RAISE_PIN, ON_VOLTAGE);
				break;
		}
	}
}