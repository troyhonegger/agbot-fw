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

static const char HITCH_FMT_STR[] PROGMEM = "{\"height\":%hhud,\"dh\":%hhd,\"target\":%hhud";
static const char HITCH_STOPPED_FMT_STR[] PROGMEM = "{\"height\":%hhud,\"dh\":%hhd,\"target\":\"STOP\"}";

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

	int8_t Hitch::getNewDH() const {
		// The following applies a sort of software hysteresis to the control logic.
		// If |target - initial| <= accuracy / 2, the hitch must be stoppped;
		// if |target - initial| > accuracy, the hitch must be started;
		// if accuracy / 2 <= |target - initial| < accuracy, the hitch will maintain the
		// previous state, unless doing so would push it further away from the target height
		if (targetHeight == STOP) { return 0; }
		
		getActualHeight(); // update actual height
		uint16_t accuracy = config->get(Setting::HitchAccuracy);

		if (targetHeight > actualHeight) {
			if (targetHeight - actualHeight > accuracy) { return 1; }
			else if (targetHeight - actualHeight <= (accuracy / 2)) { return 0; }
			else { return getDH() == 1 ? 1 : 0; }
		}
		else if (targetHeight < actualHeight) {
			if (actualHeight - targetHeight > accuracy) { return -1; }
			else if (actualHeight - targetHeight <= (accuracy / 2)) { return 0; }
			else { return getDH() == -1 ? -1 : 0; }
		}
	}

	bool Hitch::needsUpdate() const { return getNewDH() == dh; }

	void Hitch::update() {
		int8_t newDh = getNewDH();

		if (dh != newDh) {
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

	size_t Hitch::serialize(char* str, size_t n) const {
		if (targetHeight == STOP) {
			return snprintf_P(str, n, HITCH_STOPPED_FMT_STR, actualHeight, getDH());
		}
		else {
			return snprintf_P(str, n, HITCH_FMT_STR, actualHeight, getDH(), targetHeight);
		}
	}
}