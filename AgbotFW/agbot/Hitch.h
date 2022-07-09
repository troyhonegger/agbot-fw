/*
 * Hitch.h
 * Encapsulates the data and logic necessary to raise and lower the multivator's three-point hitch in the Hitch class.
 * 
 * The Hitch class also controls the clutch for the roto-tillers. When the hitch is lowered, the clutch will disengage and allow
 * the tillers to rotate. When the hitch is raised, the clutch will automatically disengage, and the tillers will stop.
 * 
 * May consider moving the clutch to its own API module, and allowing finer-grained control in the API. (may be useful for diagnostics)
 * 
 * The hitch class breaks from C++ RAII pattern, to allow for its instantiation as a global variable before main() runs.
 * Accordingly, you must call hitch.begin() before using it.
 *
 * hitch.update() must be called every loop iteration. Most operations are scheduled, not immediate,
 * so failing to call update() will result in the physical I/O points not being activated.
 * 
 * At time of writing, there is concern that the tillers may need to be stopped while the hitch is moving,
 * to limit the current draw on the alternator. In case this should be the case, the Hitch class contains
 * a needsUpdate() method. It may be checked before calling update() so that, if it returns true, the controller can
 * stop the tillers before moving the hitch.
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
#include "Config.h"

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
		static const uint8_t CLUTCH_ON_VOLTAGE = LOW; // TODO: toggle this if clutch is active high
		static const uint8_t CLUTCH_OFF_VOLTAGE = HIGH;
		static const uint8_t CLUTCH_PIN = 25;

		Config const* config;
		uint8_t targetHeight;
		volatile mutable uint8_t actualHeight;
		int8_t dh;

		// Computes the difference between target and actual heights, and returns the direction the hitch
		// would be moving if update() were called. There is some I/O overhead associated with this, so a
		// calling function should cache the value where possible rather than calling it twice.
		int8_t getNewDH() const;

		// Updates the state of the clutch. The clutch is engaged if the hitch is not lowered OR if it is moving.
		// It is disengaged if and only if the hitch is lowered AND neither raising nor lowering.
		void updateClutch();
	public:
		// Creates a new hitch object. Until begin() is called, any other member functions are still undefined.
		Hitch() : config(nullptr), targetHeight(STOP), actualHeight(MAX_HEIGHT), dh(0) {  }

		// Configures the GPIO and initializes the hitch. Call this before any other class methods.
		void begin(Config const*);

		// Returns the target height of the hitch (0-100 or a Hitch::STOP)
		inline uint8_t getTargetHeight() const { return targetHeight; }
		// Reads and stores the height from the hitch's height sensor. This is the actual, physical height of the trailer on a scale from 0-100,
		// and as such it may be different from the target height.
		uint8_t getActualHeight() const;
		// Returns the current direction of the tiller (1 means raising, 0 means stopped, -1 means lowering).
		inline int8_t getDH() const { return dh; }

		// Tells the hitch to try and reach the given height. Acceptable values are 0-100, or Hitch::STOP.
		inline void setTargetHeight(uint8_t targetHeight) { this->targetHeight = targetHeight; }
		// Tells the hitch to stop if it hasn't already. Equivalent to setTargetHeight(Hitch::STOP);
		inline void stop() { setTargetHeight(STOP); }
		// Raises the hitch. Equivalent to calling setTargetHeight(config->get(Setting::HitchRaisedHeight)); }
		inline void raise() { setTargetHeight(config->get(Setting::HitchRaisedHeight)); }
		// Lowers the hitch. Equivalent to calling setTargetHeight(config->get(Setting::HitchLoweredHeight)); }
		inline void lower() { setTargetHeight(config->get(Setting::HitchLoweredHeight)); }

		// Returns true if update() would perform any I/O. This function is included so the controller has time
		// to stop the tillers and sprayers if necessary before raising or lowering the hitch.
		bool needsUpdate() const;
		// Checks for and performs any scheduled operations. This should be called every iteration of the main controller loop.
		void update();

		// Writes the information pertaining to the hitch to the given string. Writes at most n characters, and returns the number of
		// characters that written (or that would have been written, if the size exceeds n)
		size_t serialize(char* str, size_t n) const;
};
