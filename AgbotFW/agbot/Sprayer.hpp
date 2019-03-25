/*
 * Sprayer.hpp
 * Encapsulates the data and logic necessary to interact with a sprayer on the multivator in the agbot::Sprayer class.
 * In particular, this class implements a state machine that allows you to send it either diagnostics (on/off) or
 * processing (weed/no weed) commands, and in either case, it will coordinate the task scheduling and GPIO necessary
 * to run the sprayer.
 * 
 * The sprayer class breaks from C++ RAII pattern, since needs to be initialized as a global variable, and the initialization
 * logic for a sprayer depends on the completion of certain hardware setup that occurs in the Arduino library's main() method.
 * Accordingly, you must call begin() on every sprayer before using it.
 * 
 * It is critical to remember to call sprayer.update() every loop iteration. Few of the sprayer operations (including diagnostics)
 * directly write to the GPIO pins to control the physical sprayer; instead, they tell the sprayer class it needs to change, and
 * update() performs the change.
 * 
 * Usage example:
 *	agbot::Sprayer sprayer;
 *	sprayer.begin(0, &config); // requires pre-initialized configuration - see Config.hpp
 *	sprayer.setMode(agbot::MachineMode::Process);
 *	sprayer.scheduleSpray();
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
	enum class SprayerState : uint8_t {
		Unset = 0,
		Diag = 1,
		ProcessOff = 2,
		ProcessScheduled = 3,
		ProcessOn = 4
	};

	class Sprayer {
		public:
			static const uint8_t COUNT = 8;
			static const bool ON = true;
			static const bool OFF = false;
		private:
			static const uint8_t ON_VOLTAGE = LOW; // TODO: toggle this if sprayers are active high
			static const uint8_t OFF_VOLTAGE = HIGH; // TODO: toggle this if sprayers are active high

			unsigned long onTime; // If state is SprayerState::ProcessScheduled, sprayer must be on by this time
			unsigned long offTime; // If state is SprayerState::ProcessOn, sprayer must be off by this time
			Config const* config;
			// To save space, encodes SprayerState in least significant 3 bits, ID in next 3, desired status (on/off) in next bit, actual status in MSB
			uint8_t state;

			inline void setState(SprayerState state) { this->state = (this->state & 0xF8) | static_cast<uint8_t>(state); }

			// Note the somewhat confusingly named functions here. writeStatus() sets the "actual status" and performs a GPIO write if necessary.
			// setDesiredStatus() merely flips the bit indicating whether the spray should be on - this bit is checked in update() and writeStatus()
			// is called if necessary. getStatus() returns the "actual status," getDesiredStatus() gets the desired status, and setStatus(bool)
			// sets the DESIRED state, IF the sprayer is in diag mode. All this is happening to abstract the difference between desired and actual
			// status away from the consumer - in other words, the public methods are named in terms of status, not desired status. However, that
			// leads to naming conflicts with the otherwise obvious member function names.

			// Helper method; performs the physical GPIO work on the sprayer. With few exceptions,
			// this should be called only by update()
			void writeStatus(bool status);
			// This method should be called by all the sprayer member functions. Whenever desired status and actual
			// status get out of sync, update() will toggle the actual status.
			void setDesiredStatus(bool status);
			inline bool getDesiredStatus() const { return state & 0x40 ? ON : OFF; }
			inline uint8_t getPin() const { return getId() + 9; } // TODO: assign the sprayer pins here by sprayer ID

			// disallow copy constructor since Sprayer interacts with hardware, so duplicate instances are a bad idea
			void operator=(Sprayer const&) {}
			Sprayer(Sprayer const& other) { }
		public:
			// Creates a new sprayer object. Until begin() is called, any other member functions are still undefined.
			Sprayer() {}
			
			// Initializes the GPIO pins, sets the initial states, etc. so the sprayer can begin running.
			// This should be called from inside setup(). Note that setMode() still needs to be called
			// before the sprayer can actually be used.
			void begin(uint8_t id, Config const* config);

			// Releases any resources held by the sprayer - currently, this just means resetting the GPIO pins.
			// This is implemented for completeness' sake, but it should really not be used in practice, as
			// I can't think of a logical reason for dynamically creating and then destroying a sprayer object
			~Sprayer();

			// Retrieves the ID of the sprayer, which should be between 0 and 7.
			inline uint8_t getId() const { return (state & 0x38) >> 3; }
			// Retrieves the state of the sprayer
			inline SprayerState getState() const { return static_cast<SprayerState>(state & 0x07); }
			// Retrieves the status (ON or OFF) of the sprayer
			inline bool getStatus() const { return state & 0x80 ? ON : OFF; }

			// Puts the sprayer in processing or diagnostics mode. This should be called every time the machine mode changes.
			// It is also possible to put the sprayer in MachineMode::Unset, though this state was really designed to refer to
			// the uninitialized state of a sprayer when the machine first powers up and should, in general, not be used after
			// startup. A possible exception is that MachineMode::Unset may be used to represent the state after an e-stop.
			void setMode(MachineMode);

			// Signals to the sprayer that a weed has been sighted up ahead and the sprayer should turn on at some point in the future.
			// The exact time is computed from the configuration settings. This command should be issued for every weed that is sighted,
			// even if the sprayer is already on. If enough time passes without receiving a scheduleSpray() command, the sprayer will
			// shut off. This function is valid only in processing mode
			void scheduleSpray();
			// Cancels any scheduled spray operation and shuts off the sprayer. This is only valid in processing mode.
			void cancelSpray();

			// Instructs the sprayer to shut off. This is always valid, but in process mode, the on/off timer may override the stop
			// and spontaneously start the sprayer at any time, even if it was just stopped. To ensure that the sprayer actually stops,
			// you can call stop(true) instead, which, rather than scheduling a stop for the next update(), actually performs the GPIO
			// thus overriding any scheduled events. Note that even then, the next time you call update() the stop command can still
			// be overridden.
			void stop(bool now = false);

			// Tells the sprayer to turn ON or OFF. This is valid only in diagnostics mode
			void setStatus(bool status);

			// Checks for and performs any scheduled spray operations. It is CRITICAL that this be called every iteration of the controller
			// loop, as scheduleSpray() does not actually interact with the GPIO pins; it only schedules work to be done in update()
			void update();

			// Writes the information pertaining to this sprayer to the given string. Writes at most n characters, and returns the number of
			// characters that written (or that would have been written, if the size exceeds n)
			size_t serialize(char* str, size_t n) const;
	};
}