/*
 * Config.cpp
 * Implements the Config class declared in Config.h for storing persistent configuration settings.
 * See Config.h for more info.
 * Created: 11/21/2018 8:24:59 PM
 *  Author: troy.honegger
 */

#include <Arduino.h>
#include <EEPROM.h>

#include "Config.h"

#define MAKE_SETTING_STRING(setting)	static_assert((uint8_t) Setting::setting >= 0, ""); \
										static const char setting##_STR [] PROGMEM = #setting
#define SETTING_METADATA(setting, minVal, maxVal) { setting##_STR, minVal, maxVal }

MAKE_SETTING_STRING(Precision);
MAKE_SETTING_STRING(KeepAliveTimeout);
MAKE_SETTING_STRING(ResponseDelay);
MAKE_SETTING_STRING(TillerRaiseTime);
MAKE_SETTING_STRING(TillerLowerTime);
MAKE_SETTING_STRING(TillerAccuracy);
MAKE_SETTING_STRING(TillerLoweredHeight);
MAKE_SETTING_STRING(TillerRaisedHeight);
MAKE_SETTING_STRING(HitchAccuracy);
MAKE_SETTING_STRING(HitchLoweredHeight);
MAKE_SETTING_STRING(HitchRaisedHeight);

// Ordering of these values MUST align with order of settings declared
// in Setting enum.
static struct {
	const char* name;
	uint16_t minValue;
	uint16_t maxValue;
} settingData[] = {
	SETTING_METADATA(Precision, 0, 0xFFFF),
	SETTING_METADATA(KeepAliveTimeout, 0, 0xFFFF),
	SETTING_METADATA(ResponseDelay, 0, 0xFFFF),
	SETTING_METADATA(TillerRaiseTime, 0, 0xFFFF),
	SETTING_METADATA(TillerLowerTime, 0, 0xFFFF),
	SETTING_METADATA(TillerAccuracy, 0, 100),
	SETTING_METADATA(TillerLoweredHeight, 0, 100),
	SETTING_METADATA(TillerRaisedHeight, 0, 100),
	SETTING_METADATA(HitchAccuracy, 0, 100),
	SETTING_METADATA(HitchLoweredHeight, 0, 100),
	SETTING_METADATA(HitchRaisedHeight, 0, 100),
};

static_assert(Config::NUM_SETTINGS == sizeof(settingData) / sizeof(settingData[0]),
		"settingData must align with Setting enum definitions in Config.h");

void Config::begin() {
	for (uint8_t i = 0; i < NUM_SETTINGS; i++) {
		EEPROM.get(i * SETTING_SIZE, settings[i]);
	}
}

bool Config::set(Setting setting, uint16_t value) {
	uint8_t index = static_cast<uint8_t>(setting);
	if (index >= NUM_SETTINGS
			|| settingData[index].minValue > value
			|| settingData[index].maxValue < value) {
		return false;
	}
	if (settings[index] != value) {
		settings[index] = value;
		EEPROM.put(index * SETTING_SIZE, value);
	}
	return true;
}

const char* settingToString(Setting setting) {
	uint8_t index = static_cast<uint8_t>(setting);
	if (index >= Config::NUM_SETTINGS) {
		return nullptr;
	}
	else {
		return settingData[index].name;
	}
}

bool stringToSetting(const char* string, Setting& setting) {
	if (string) {
		for (uint8_t i = 0; i < Config::NUM_SETTINGS; i++) {
			if (!strcmp_P(string, settingData[i].name)) {
				setting = static_cast<Setting>(i);
				return true;
			}
		}
	}
	return false;
}

uint16_t minSettingValue(Setting setting) {
	if (static_cast<uint8_t>(setting) >= Config::NUM_SETTINGS) {
		return 0;
	}
	return settingData[static_cast<uint8_t>(setting)].minValue;
}

uint16_t maxSettingValue(Setting setting) {
	if (static_cast<uint8_t>(setting) >= Config::NUM_SETTINGS) {
		return 0xFFFF;
	}
	return settingData[static_cast<uint8_t>(setting)].maxValue;
}
