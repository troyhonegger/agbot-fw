/*
 * Config.hpp
 * Encapsulates the data and logic necessary to store persistent configuration settings in the agbot::Config class.
 * This class allows you to store and retrieve configuration values from the EEPROM memory.
 * 
 * Usage example:
 *	agbot::Config config; // (most likely as a global variable)
 *	config.begin();
 *	uint16_t value = config.get(agbot::Setting::Precision);
 *	config.set(agbot::Setting::Precision, value);
 *
 * Created: 3/2/2019 3:26:42 PM
 *  Author: troy.honegger
 */ 
#pragma once

namespace agbot {
	const size_t SETTING_SIZE = sizeof(uint16_t);
	const uint8_t NUM_SETTINGS = 10;

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
		private:
			// Note: if memory becomes an issue we can save 6 bytes here (at some maintainability cost) by packing the 1-byte settings
			// together instead of allocating 2 bytes for everything.
			uint16_t settings[12];

			// disallow copy constructor
			void operator=(Config const&) {}
			Config(Config const&) {}
		public:
			// Creates a new config object. Until begin() is called, any other member functions are still undefined.
			Config() {}

			// Loads the configuration settings from EEPROM memory.
			void begin();

			// Returns the specified setting from the cache.
			inline uint16_t get(Setting setting) const { return settings[static_cast<uint8_t>(setting)]; }

			// Writes the specified setting value to EEPROM memory and updates the cache. There is some overhead
			// associated with this and, in general, it should only be done upon request over the API.
			void set(Setting setting, uint16_t value);
	};
}