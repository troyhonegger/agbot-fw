/*
 * Tiller.hpp
 * Encapsulates the data and logic necessary to interact with a tiller on the multivator in the agbot::Tiller class.
 * In particular, the class implements a state machine that allows you to send it either diagnostics (raise/lower/stop)
 * or processing (weed/no weed) commands, and in either case, it will coordinate all the task scheduling, height
 * sensing, and GPIO necessary to run the tiller.
 * 
 * The tiller class breaks from C++ RAII pattern, since needs to be initialized as a global variable, and the initialization
 * logic for a tiller depends on the completion of certain hardware setup that occurs in the Arduino library's main() method.
 * Accordingly, you must call begin() on every tiller before using it.
 * 
 * It is critically important to realize that tiller operations are scheduled, and not necessarily performed immediately.
 * When you call ANY tiller function (from scheduleLower() to setHeight() to stop()), the tiller itself does not change
 * until you call update(). This is the ONLY function that performs GPIO.
 * 
 * Usage example:
 *	agbot::Tiller tiller;
 *	tiller.begin(0, &config); // requires pre-initialized configuration - see Config.hpp
 *	tiller.setMode(agbot::MachineMode::Process);
 *	tiller.scheduleLower();
 *	tiller.update();
 * 
 * Created: 3/1/2019 11:12:49 PM
 *  Author: troy.honegger
 */ 

#pragma once

#include "Common.hpp"
#include "Config.hpp"

namespace agbot {
	enum class TillerState : uint8_t {
		Unset=0,
		Diag=1,
		ProcessRaising=2,
		ProcessLowering=3,
		ProcessScheduled=4
	};

	class Tiller {
		public:
			static const uint8_t COUNT = 3;
			static const uint8_t MAX_HEIGHT = 100;
			static const uint8_t STOP = 255;
		private:
			static const uint8_t ON_VOLTAGE = LOW; // TODO: toggle this if tillers are active high
			static const uint8_t OFF_VOLTAGE = !ON_VOLTAGE;
			unsigned long lowerTime; // If state is TillerState::ProcessScheuled, tiller must begin lowering by this time
			unsigned long raiseTime; // If state is TillerState::ProcessLowering, tiller must begin raising by this time
			Config const* config;
			uint8_t state; // To save space, encodes TillerState in least significant 4 bits, id in next 2, and dh in next 2
			uint8_t targetHeight;
			mutable uint8_t actualHeight;
			inline void setState(TillerState state) { this->state = (this->state & 0xF0) | static_cast<uint8_t>(state); }
			inline void setDH(int8_t dh) { state = (state & 0x3F) | ((dh & 3) << 6); }
			
			inline uint8_t getRaisePin() const { return getId() * 2 + 30; }
			inline uint8_t getLowerPin() const { return getRaisePin() + 1; }
			inline uint8_t getHeightSensorPin() const { return PIN_A9 + getId(); }

			// disallow copy constructor since Tiller interacts with hardware, which makes duplicate instances a bad idea
			void operator =(Tiller const&) {}
			Tiller(Tiller const& other) { }
		public:
			// Creates a new tiller object. Until begin() is called, any other member functions are still undefined.
			Tiller() {}

			// Initializes the tiller, sets up the GPIO pins, and performs any other necessary setup work.
			// note that setMode() still needs to be called before the tiller can be actually used.
			void begin(uint8_t id, Config const* config);

			// Releases any resources held by the tiller - currently, this just means resetting the GPIO pins.
			// This is implemented for completeness' sake, but it should really not be used in practice, as
			// I can't think of a logical reason for dynamically creating and then destroying a tiller object
			~Tiller();

			// Retrieves the ID of the tiller, which should be between 0 and 2.
			inline uint8_t getId() const { return (state & 0x30) >> 4; }
			// Retrieves the state of the tiller
			inline TillerState getState() const { return static_cast<TillerState>(state & 0xF); }
			// Retrieves the current direction of motion of the tiller (1 is up, 0 is stopped, -1 is down)
			inline int8_t getDH() const { return (state >= 0x80) ? -1 : (state & 0xC0) >> 6; }
			
			// Retrieves the cached height of the trailer. This is an actual, physical height of the trailer on a scale from 0-100,
			// and it may be different from the target height.
			inline uint8_t getActualHeight() const { return actualHeight; }
			// Reads and stores the height from the tiller's height sensor. This is the actual, physical height of the trailer on a scale from 0-100,
			// and as such it may be different from the target height.
			void updateActualHeight() const;

			// Puts the tiller in processing or diagnostics mode. This should be called every time the machine mode changes.
			// It is also possible to put the tiller in MachineMode::Unset, though this state was really designed to refer to
			// the uninitialized state of a tiller when the machine first powers up and should, in general, not be used after
			// startup. A possible exception is that MachineMode::Unset may be used to represent the state after an e-stop.
			void setMode(MachineMode);
			
			// Returns the target height of the tiller (0-100 or Tiller::STOP)
			inline uint8_t getTargetHeight() const { return targetHeight; }
			// Sets the tiller to target the specified height. This is valid only in diagnostics mode; in process mode, use scheduleLower()
			void setTargetHeight(uint8_t);

			// Instructs the tiller to stop at its current height. This is always valid, but in process mode, the raise/lower timer may
			// override the stop command and spontaneously start the tiller at any time, even if it was just stopped. To ensure that
			// the tiller actually stops, you can call stop(true) instead, which, rather than scheduling a stop for the next update(),
			// actually performs the GPIO work to do it immediately, thus overriding any scheduled operations. Note that even then, the
			// next time you call update() the stop command can still be overridden.
			void stop(bool now = false);

			// Signals to the tiller that a weed has been sighted up ahead and the tiller should begin lowering at some point in the future.
			// The exact time is computed from the configuration settings. This command should be issued for every weed that is sighted,
			// even if the tiller is already lowered. If enough time passes without receiving a scheduleLower() command, the tiller will raise
			// back up. This function is valid only in processing mode
			void scheduleLower();
			// Cancels any scheduled "tiller lower" operation and instructs the tiller to raise immediately. This is only valid in processing mode.
			void cancelLower();

			// The only function in Tiller that actually sets voltages on GPIO pins. It is CRITICAL that this be called every iteration of the main
			// controller loop; other commands (including diagnostics commands, stop commands, etc) only schedule operations to be performed in
			// the update() function
			void update();

			// Writes the information pertaining to this tiller to the given string. Writes at most n characters, and returns the number of
			// characters that written (or that would have been written, if the size exceeds n)
			size_t serialize(char* str, size_t n) const;
	};
}