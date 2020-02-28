/*
 * Tiller.hpp
 * Encapsulates the data and logic necessary to interact with a tiller on the multivator in the agbot::Tiller class.
 * Handles scheduling of commands (raise, lower, stop, etc) sent to a single tiller.
 * 
 * The tiller class breaks from C++ RAII pattern, to allow for its instantiation as a global variable before main() runs.
 * Accordingly, you must call begin() on every tiller before using it.
 * 
 * tiller.update() must be called every loop iteration. Most operations are scheduled, not immediate,
 * so failing to call update() will result in the physical I/O points not being activated
 * 
 * Usage example:
 *	agbot::Tiller tiller;
 *	tiller.begin(0, &config); // requires pre-initialized configuration - see Config.hpp
 *	tiller.killWeed();
 *	tiller.update(); // call this repeatedly so tiller can raise when ready
 * 
 * Created: 3/1/2019 11:12:49 PM
 *  Author: troy.honegger
 */ 

#pragma once

#include "Common.hpp"
#include "Config.hpp"

namespace agbot {
	// Commands that can be given to the tiller in setHeight() in place of a height 0-100.
	enum TillerCommand : uint8_t {
		RAISED = 251, // Tiller should be raised slightly above the ground, ready to lower into position if a weed is spotted. The exact height will depend on the distance to the ground.
		LOWERED = 252, // Tiller should be lowered into the soil. The exact height will depend on the distance to the ground.
		UP = 253, // Tiller should always be raising, until it hits the hardware limit switch.
		DOWN = 254, // Tiller should always be lowering, until it hits the hardware limit switch.
		STOP = 255 // Tiller should stop where it is.
	};

	class Tiller {
		public:
			static const uint8_t COUNT = 3; // number of tillers on the machine
			static const uint8_t MAX_HEIGHT = 100;
		private:
			static const uint8_t COMMAND_LIST_SIZE = 4;
			Timer timers[COMMAND_LIST_SIZE];
			Config const* config;
			uint8_t commandList[COMMAND_LIST_SIZE];
			uint8_t state; // To save space, id is stored in bits 4-5, and dh in bits 6-7 (where the least significant bit is bit 0)
			uint8_t targetHeight; // is either a height 0-Tiller::MAX_HEIGHT or a TillerCommand
			mutable uint8_t actualHeight;

			inline uint8_t getOnVoltage(void) const { return getId() == 2 ? LOW : HIGH; } // TODO: change mapping if need be
			inline uint8_t getOffVoltage(void) const { return !getOnVoltage(); }
			inline void setDH(int8_t dh) { state = (state & 0x3F) | ((dh & 3) << 6); }
			
			inline uint8_t getRaisePin(void) const { return getId() * 2 + 30; }
			inline uint8_t getLowerPin(void) const { return getRaisePin() + 1; }
			inline uint8_t getHeightSensorPin(void) const { return PIN_A9 + getId(); }

			// disallow copy constructor since Tiller interacts with hardware, which makes duplicate instances a bad idea
			void operator =(Tiller const&) {}
			Tiller(Tiller const& other) { }
		public:
			// Creates a new tiller object. Until begin() is called, any other member functions are still undefined.
			Tiller() {}

			// Initializes the tiller, sets up the GPIO pins, and performs any other necessary setup work.
			void begin(uint8_t id, Config const* config);

			// Releases any resources held by the tiller - currently, this just means resetting the GPIO pins.
			// This is implemented for completeness' sake, but it should really not be used in practice, as
			// I can't think of a logical reason for dynamically creating and then destroying a tiller object
			~Tiller();

			// Retrieves the ID of the tiller, which should be between 0 and Tiller::COUNT - 1.
			inline uint8_t getId(void) const { return (state & 0x30) >> 4; }
			// Retrieves the current direction of motion of the tiller (1 is up, 0 is stopped, -1 is down)
			inline int8_t getDH(void) const { return (state >= 0x80) ? -1 : (state & 0xC0) >> 6; }
			
			// Retrieves the cached height of the trailer. This is an actual, physical height of the trailer on a scale from 0-100,
			// and it may be different from the target height.
			inline uint8_t getActualHeight(void) const { return actualHeight; }
			// Reads and stores the height from the tiller's height sensor. This is the actual, physical height of the trailer on a scale from 0-100,
			// and as such it may be different from the target height.
			void updateActualHeight(void) const;

			// Returns the target height of the tiller (0-100 or a TillerCommand)
			inline uint8_t getTargetHeight(void) const { return targetHeight; }

			// Adds a command to the command list, to be executed after a delay. Commands are executed from the command list as their timers
			// expire. The most recently inserted command overrides all commands that would otherwise trigger after it; so, for example, if
			// the tiller is set to raise in 100ms, calling setHeight(TillerCommand::STOP, 0) will cancel that operation.
			// Arguments: command is either a height 0-100 or a TillerCommand. delay is a value in milliseconds.
			// returns: true if there is room in the command list for the command; false if the command list is full.
			bool setHeight(uint8_t command, uint32_t delay = 0);

			// Signals to the tiller that a weed has been sighted up ahead and the tiller should begin lowering at some point in the future.
			// The exact time is computed from the configuration settings. This command should be issued for every weed that is sighted,
			// even if the tiller is already lowered. If enough time passes after sending this command, the tiller will raise back up.
			void killWeed(void);

			// Checks for and performs any scheduled operations. This should be called every iteration of the main controller loop.
			void update(void);

			// Writes the information pertaining to this tiller to the given string. Writes at most n characters, and returns the number of
			// characters that written (or that would have been written, if the size exceeds n)
			size_t serialize(char* str, size_t n) const;
	};
}
