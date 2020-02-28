/*
 * Sprayer.hpp
 * Encapsulates the data and logic necessary to interact with a sprayer on the multivator in the agbot::Sprayer class.
 * Handles scheduling of on/off commands sent to a single sprayer.
 * 
 * The sprayer class breaks from C++ RAII pattern, to allow for its instantiation as a global variable before main() runs.
 * Accordingly, you must call begin() on every sprayer before using it.
 * 
 * sprayer.update() must be called every loop iteration. Most operations are scheduled, not immediate,
 * so failing to call update() will result in the physical I/O points not being activated
 * 
 * Usage example:
 *	agbot::Sprayer sprayer;
 *	sprayer.begin(0, &config); // requires pre-initialized configuration - see Config.hpp
 *	sprayer.killWeed();
 *	sprayer.update(); // call this repeatedly so sprayer can turn on when ready
 * 
 * Created: 3/4/2019 1:00:05 PM
 *  Author: troy.honegger
 */


#pragma once

#include <Arduino.h>
#include "Common.hpp"
#include "Config.hpp"

namespace agbot {

	class Sprayer {
		public:
			static const uint8_t COUNT = 8; // number of sprayers on the machine.
			static const bool ON = true;
			static const bool OFF = false;
		private:
			static const uint8_t ON_VOLTAGE = LOW; // TODO: toggle this if sprayers are active high
			static const uint8_t OFF_VOLTAGE = !ON_VOLTAGE;
			static const uint8_t COMMAND_LIST_SIZE = 4; // cannot exceed 8 as each command occupies a bit in a uint8_t

			Timer timers[COMMAND_LIST_SIZE];

			Config const* config;

			// To save space, encodes ID in bits 0-3 and status in bit 7 (where bit 0 is LSB)
			uint8_t state;
			uint8_t commandList;

			// Physically turns the sprayer on or off.
			void setActualStatus(bool status);

			inline uint8_t getPin() const { return getId() + 38; }

			// disallow copy constructor since Sprayer interacts with hardware, so duplicate instances are a bad idea
			void operator=(Sprayer const&) {}
			Sprayer(Sprayer const& other) { }
		public:
			// Creates a new sprayer object. Until begin() is called, any other member functions are still undefined.
			Sprayer() {}
			
			// Initializes the GPIO pins, sets the initial states, etc. so the sprayer can begin running.
			// This should be called from inside setup().
			void begin(uint8_t id, Config const* config);

			// Releases any resources held by the sprayer - currently, this just means resetting the GPIO pins.
			// This is implemented for completeness' sake, but it should really not be used in practice, as
			// I can't think of a logical reason for dynamically creating and then destroying a sprayer object
			~Sprayer();

			// Retrieves the ID of the sprayer, which should be between 0 and Sprayer::COUNT - 1.
			inline uint8_t getId() const { return state & 0x0F; }
			// Retrieves the status (ON or OFF) of the sprayer
			inline bool getStatus() const { return state & 0x80 ? ON : OFF; }

			// Tells the sprayer to turn ON or OFF after a delay. Cancels all operations already scheduled to occur after that delay.
			// returns: true if there is room in the command list to schedule the operation; false if the command list is full.
			bool setStatus(bool status, uint32_t delay = 0);

			// Signals to the sprayer that a weed has been sighted up ahead and the sprayer should turn on at some point in the future.
			// The exact time is computed from the configuration settings. This command should be issued for every weed that is sighted,
			// even if the sprayer is already on. If enough time passes after sending this command, the sprayer will turn back off.
			void killWeed(void);

			// Checks for and performs any scheduled spray operations. This should be called every iteration of the main controller loop.
			void update();

			// Writes the information pertaining to this sprayer to the given string. Writes at most n characters, and returns the number of
			// characters that written (or that would have been written, if the size exceeds n)
			size_t serialize(char* str, size_t n) const;
	};
}
