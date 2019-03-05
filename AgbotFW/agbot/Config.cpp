/*
 * Config.cpp
 * There are a handful of settings that need to be stored in EEPROM so that they can be configured at runtime and persisted
 * across application instances. This module implements functions for reading and writing those values. The "getter" functions
 * can be called anytime - the settings are buffered into memory so there is no data read overhead. The "setter" functions,
 * however, do have overhead and, in general, should not be called except upon request over the serial port.
 * Created: 11/21/2018 8:24:59 PM
 *  Author: troy.honegger
 */

#include <Arduino.h>
#include <EEPROM.h>

#include "Config.hpp"

namespace agbot {
	Config::Config() {
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