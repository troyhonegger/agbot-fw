/*
 * Config.cpp
 * Implements the agbot::Config class declared in Config.hpp for storing persistent configuration settings.
 * See Config.hpp for more info.
 * Created: 11/21/2018 8:24:59 PM
 *  Author: troy.honegger
 */

#include <Arduino.h>
#include <EEPROM.h>

#include "Config.hpp"

namespace agbot {
	void Config::begin() {
		for (uint8_t i = 0; i < NUM_SETTINGS; i++) {
			EEPROM.get(i * SETTING_SIZE, settings[i]);
		}
	}
	
	void Config::set(Setting setting, uint16_t value) {
		uint8_t index = static_cast<uint8_t>(setting);
		if (settings[index] != value) {
			settings[index] = value;
			EEPROM.put(index * SETTING_SIZE, value);
		}
	}
}