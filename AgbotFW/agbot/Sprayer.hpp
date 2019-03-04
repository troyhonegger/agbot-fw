/*
 * Sprayer.hpp
 * Encapsulates the data and logic necessary to interact with a sprayer on the multivator.
 * Created: 3/4/2019 1:00:05 PM
 *  Author: troy.honegger
 */ 

#pragma once

#include "Common.hpp"
#include "Config.hpp"

namespace agbot {
	enum class SprayerState : uint8_t {
		Unset = 0,
		DiagOff = 1,
		DiagOn = 2,
		ProcessOff = 3,
		ProcessScheduled = 4,
		ProcessOn = 5,
		ProcessStopped = 6
	};

	class Sprayer {
		private:
			static const uint8_t ON_VOLTAGE = LOW;
			static const uint8_t OFF_VOLTAGE = HIGH;

			unsigned long onTime; // If state is SprayerState::ProcessScheduled, sprayer must be on by this time
			unsigned long offTime; // If state is SprayerState::ProcessOn, sprayer must be off by this time
			Config const& config;
			uint8_t state; // To save space, encodes SprayerState in least significant 3 bits, ID in next 4, and status (on/off) in MSB

			inline void setState(SprayerState state) { Sprayer::state = (Sprayer::state & 0xF8) | static_cast<uint8_t>(state); }
			// status should be true for on; false for off
			void setStatus(bool status);
			inline uint8_t getPin() const { return getId() + 9; }

			// disallow copy constructor since Sprayer interacts with hardware, which makes duplicate instances a bad idea
			void operator =(Sprayer const&) {}
			Sprayer(Sprayer const& other) : config(other.config) { }
		public:
			// Initializes the sprayer, sets up the GPIO pins, and performs any other necessary setup work.
			// note that setMode() still needs to be called before the sprayer can be actually used.
			Sprayer(uint8_t id, Config const& config);

			// Releases any resources held by the sprayer - currently, this just means resetting the GPIO pins.
			// This is implemented for completeness' sake, but it should really not be used in practice, as
			// I can't think of a logical reason for dynamically creating and then destroying a sprayer object
			~Sprayer();

			// Retrieves the ID of the sprayer, which should be between 0 and 7.
			inline uint8_t getId() const { return (state & 0x78) >> 3; }
			// Retrieves the state of the sprayer
			inline SprayerState getState() const { return static_cast<SprayerState>(state & 0x07); }
			inline bool isOn() const { return (state & 0x80) != 0; }

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

			// Shuts off the sprayer, cancels any scheduled spray operation, and sets the sprayer to ignore all future scheduleSpray()
			// calls until resume() is called. This is designed to be called when the machine reaches the end of a row.
			void stop();
			// The sprayers should be stopped at the end of a row while the multivator three-point hitch is raised. Once the multivator
			// enters the next row, calling resume() tells the sprayer that it can resume "normal" processing
			void resume();

			// Manually turns the sprayer on or off. This function is valid only in diagnostics mode.
			void setIsOn(bool on);

			// Checks for and performs any scheduled spray operations. It is CRITICAL that this be called every iteration of the controller
			// loop, as scheduleSpray() does not actually interact with the GPIO pins; it only schedules work to be done in update()
			void update();
	};
}