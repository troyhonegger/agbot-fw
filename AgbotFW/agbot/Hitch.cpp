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

static const char HITCH_FMT_STR[] PROGMEM = "{\"height\":%hhu,\"dh\":%hhd,\"target\":%hhu}";
static const char HITCH_STOPPED_FMT_STR[] PROGMEM = "{\"height\":%hhu,\"dh\":%hhd,\"target\":\"STOP\"}";

namespace agbot {
	uint8_t Hitch::getActualHeight() const {
		actualHeight = map(analogRead(HEIGHT_SENSOR_PIN), 1023, 204, 0, MAX_HEIGHT);
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
		pinMode(CLUTCH_PIN, OUTPUT);
		digitalWrite(CLUTCH_PIN, CLUTCH_OFF_VOLTAGE);
		updateClutch();
	}

	int8_t Hitch::getNewDH() const {
		getActualHeight(); // update actual height (not currently used to determine newDH, but updating it at least keeps it between 0 and 100)
		if (targetHeight == STOP) { return 0; }
		else if (targetHeight < MAX_HEIGHT / 2) { return -1; }
		else { return 1; }
	}

	bool Hitch::needsUpdate() const { return getNewDH() != dh; }

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
					digitalWrite(LOWER_PIN, OFF_VOLTAGE);
					break;
				case 1:
					digitalWrite(LOWER_PIN, OFF_VOLTAGE);
					digitalWrite(RAISE_PIN, ON_VOLTAGE);
					break;
			}
		}
		updateClutch();
	}

	void Hitch::updateClutch() {
		if (dh) { digitalWrite(CLUTCH_PIN, CLUTCH_ON_VOLTAGE); }
		else {
			if (targetHeight == STOP) {
				// We're not moving, but we don't know if the hitch is up or not. We'll leave the clutch on to be safe.
				digitalWrite(CLUTCH_PIN, CLUTCH_ON_VOLTAGE);
			}
			else if (targetHeight < MAX_HEIGHT / 2) {
				digitalWrite(CLUTCH_PIN, CLUTCH_OFF_VOLTAGE);
			}
			else { digitalWrite(CLUTCH_PIN, CLUTCH_ON_VOLTAGE); }
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