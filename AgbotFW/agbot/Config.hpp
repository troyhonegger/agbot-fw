/*
 * Config.hpp
 * Encapsulates the data and logic necessary to store persistent configuration settings in the Config class.
 * This class allows you to store and retrieve configuration values from the EEPROM memory.
 * 
 * Usage example:
 *	Config config; // (most likely as a global variable)
 *	config.begin();
 *	uint16_t value = config.get(Setting::Precision);
 *	config.set(Setting::Precision, value);
 *
 * Created: 3/2/2019 3:26:42 PM
 *  Author: troy.honegger
 */ 
#pragma once

enum class Setting : uint8_t {
	// The length of time, in milliseconds, the tiller will be lowered or the sprayer will be on to eliminate a single weed.
	// Lowering the value means more efficiency but tighter timing.
	Precision = 0,
	// The length of time, in milliseconds, the controller will wait without receiving a KeepAlive before engaging the e-stop.
	// Should be set to 2000 or a similar small default.
	KeepAliveTimeout = 1,
	// The length of time, in milliseconds, between when the controller learns of the weed and when the weed actually passes
	// beneath the tillers/sprayers. The exact value will be a function of vehicle speed.
	ResponseDelay = 2,
	// The amount of time, in milliseconds, for a tiller to raise from 0 to 100.
	TillerRaiseTime = 3,
	// The amount of time, in milliseconds, for a tiller to lower from 100 to 0.
	TillerLowerTime = 4,
	// The amount of variation built into the state machine that maintains the tiller height. For example, if set to 5, and the
	// tiller's target height is 30, the controller will allow a height anywhere between 25 and 35. This should be between 0 and 100.
	TillerAccuracy = 5,
	// The height of the tiller when it is considered "lowered" in the processing state. This should be between 0 and 100.
	TillerLoweredHeight = 6,
	// The height of the tiller when it is considered "raised" in the processing state. This should be between 0 and 100.
	TillerRaisedHeight = 7,
	// The amount of variation allowed by the state machine that maintains the hitch height. For example, if set to 5, and the hitch
	// has a target height of 30, the controller will allow a height anywhere between 25 and 35. This should be set between 0 and 100.
	HitchAccuracy = 8,
	// The height of the 3-point hitch when it is lowered for processing. This should be between 0 and 100.
	HitchLoweredHeight = 9,
	// The height of the 3-point hitch when it is raised for transport or at the end of a row. This should be between 0 and 100.
	HitchRaisedHeight = 10
};

class Config {
	public:
		static const size_t SETTING_SIZE = sizeof(uint16_t);
		static const uint8_t NUM_SETTINGS = 11;
	private:
		// in-RAM buffer for all settings
		uint16_t settings[NUM_SETTINGS];

		// disallow copy constructor
		void operator=(Config const&) {}
		Config(Config const&) {}
	public:
		// Creates a new config object. Until begin() is called, any other member functions are still undefined.
		Config() {}

		// Loads the configuration settings from EEPROM memory.
		void begin();

		// Returns the specified setting from the cache.
		inline uint16_t get(Setting setting) const {
			if (static_cast<uint8_t>(setting) >= NUM_SETTINGS) {
				return 0;
			}
			return settings[static_cast<uint8_t>(setting)];
		}

		// Writes the specified setting value to EEPROM memory and updates the cache. There is some overhead
		// associated with this and, in general, it should only be done upon request over the API.
		// Returns true upon success, false if setting is invalid or value is out of bounds
		bool set(Setting setting, uint16_t value);
};

const char* settingToString(Setting); // Returns a PROGMEM string containing the string representation of the setting.
bool stringToSetting(const char*, Setting&); // Accepts a non-PROGMEM string. returns true if setting is valid, false otherwise
uint16_t maxSettingValue(Setting);
uint16_t minSettingValue(Setting);
