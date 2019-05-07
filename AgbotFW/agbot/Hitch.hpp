/*
 * Hitch.hpp
 * Encapsulates the data and logic necessary to raise and lower the multivator's three-point hitch in the agbot::Hitch class.
 * Unlike the apparently similar Tiller class, Hitch has no distinct states, and no distinction between diag and processing mode.
 * This is because the hitch has no need for scheduling - it can be told to raise NOW, or to lower NOW, or to stop NOW, and it
 * will do so.
 * 
 * The hitch class breaks from C++ RAII pattern, since it needs to be initialized as a global variable, and the initialization
 * logic for a hitch depends on the completion of certain hardware setup that occurs in the Arduino library's main() method.
 * Accordingly, you must call begin() on the hitch (typically in the setup() function) before using it.
 * 
 * As of the initial implementation, it is possible that the tillers will need to be stopped while the hitch is moving,
 * to prevent drawing too much current from the alternator. In case this should be the case, the Hitch class contains
 * a needsUpdate() method. It may be checked before calling update() so that, if it returns true, the controller can
 * stop the tillers before moving the hitch.
 * 
 * It is critically important to realize that the hitch class only directly controls the physical hitch through the update()
 * function; all other methods simply instruct the update() function how to do its work.
 * 
 * Usage:
 *	Hitch hitch;
 *	hitch.begin(config);
 *	hitch.lower();
 *	if (hitch.needsUpdate()) {
 *		// Remember to stop the sprayers and tillers here!
 *		...
 * 
 *		hitch.update();
 *	}
 *	else {
 *		// Update sprayers and tillers as usual
 *		...
 *	}
 * 
 * Created: 3/6/2019 7:59:30 PM
 *  Author: troy.honegger
 */ 

#pragma once

#include <Arduino.h>
#include "Config.hpp"

namespace agbot {
	class Hitch {
		public:
			static const uint8_t MAX_HEIGHT = 100;
			static const uint8_t STOP = 255;
		private:
			static const uint8_t ON_VOLTAGE = LOW; // TODO: toggle this if hitch is active high
			static const uint8_t OFF_VOLTAGE = !ON_VOLTAGE;
			static const uint8_t RAISE_PIN = 26;
			static const uint8_t LOWER_PIN = 27;
			static const uint8_t HEIGHT_SENSOR_PIN = PIN_A15;

			Config const* config;
			uint8_t targetHeight;
			volatile mutable uint8_t actualHeight;
			int8_t dh;

			// Computes the difference between target and actual heights, and returns the direction the hitch
			// would be moving if update() were called. There is some I/O overhead associated with this, so a
			// calling function should cache the value where possible rather than calling it twice.
			int8_t getNewDH() const;
		public:
			// Creates a new hitch object. Until begin() is called, any other member functions are still undefined.
			Hitch() : config(nullptr), targetHeight(STOP), actualHeight(MAX_HEIGHT), dh(0) {  }

			// Configures the GPIO and initializes the hitch to begin raising and lowering. This should be
			// called before any other function on the hitch class.
			void begin(Config const*);

			// Returns the target height of the hitch. If the height is Hitch::STOP, the hitch wants to stop regardless
			// of its position. If the height is between 0 and 100, the hitch wants to move to the target height.
			inline uint8_t getTargetHeight() const { return targetHeight; }
			// Reads and returns the actual, physical height of the hitch off the GPIO pins. There is some performance
			// overhead associated with this operation (roughly 100us estimated), so calling it thousands of times a
			// second can slow the system down.
			uint8_t getActualHeight() const;
			// Returns the current direction of the tiller (1 means raising, 0 means stopped, -1 means lowering).
			inline int8_t getDH() const { return dh; }

			// Tells the hitch to try and reach the given height. Acceptable values are 0-100, or Hitch::STOP.
			inline void setTargetHeight(uint8_t targetHeight) { this->targetHeight = targetHeight; }
			// Tells the hitch to stop if it hasn't already. Equivalent to setTargetHeight(Hitch::STOP);
			inline void stop() { setTargetHeight(STOP); }
			// Tells the hitch to raise to a height given by the configuration.
			// Equivalent to calling setTargetHeight(config->get(Setting::HitchRaisedHeight)); }
			inline void raise() { setTargetHeight(config->get(Setting::HitchRaisedHeight)); }
			// Tells the hitch to lower to a height given by the configuration.
			// Equivalent to calling setTargetHeight(config->get(Setting::HitchLoweredHeight)); }
			inline void lower() { setTargetHeight(config->get(Setting::HitchLoweredHeight)); }

			// Calculates if the hitch needs to update - if the difference between the target and actual heights is great enough.
			// This is included so the controller has time to stop the tillers and sprayers if necessary before raising or lowering
			// the hitch.
			bool needsUpdate() const;
			// Raises, lowers, or stops the hitch to try and reach the target height. Note that this is the ONLY function that
			// actually moves the hitch - other functions simply instruct the update() function on what to do
			void update();

			// Writes the information pertaining to the hitch to the given string. Writes at most n characters, and returns the number of
			// characters that written (or that would have been written, if the size exceeds n)
			size_t serialize(char* str, size_t n) const;
	};
}