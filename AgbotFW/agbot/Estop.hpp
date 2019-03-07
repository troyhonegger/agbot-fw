/*
 * Estop.h
 * Contains the necessary logic for the controller to interface with the machine's hardware e-stop.
 * Created: 3/6/2019 1:30:36 PM
 *  Author: troy.honegger
 */ 

#pragma once

namespace agbot{
	class Estop {
		private:
			static const uint8_t HW_PIN = 0; // TODO: find this
			static const uint16_t PULSE_LEN = 100;
			
			unsigned long whenEngaged;
			bool engaged;
			
			// disallow copy constructor
			void operator=(Estop const&) {}
			Estop(Estop const&) {}
		public:
			// Creates a new config object. Until begin() is called, any other member functions are still undefined.
			Estop();

			// Initializes the e-stop (configures GPIO pins, sets internal state, etc)
			void begin();

			// returns true if and only if the controller, or any other system connected to the e-stop line, is currently holding
			// the line low. Note that this will still return false even if the e-stop has already latched on, but the e-stop line
			// is no longer low. There is currently no way to get the state of the e-stop relay itself.
			bool isEngaged() const;
			
			// switches on the e-stop, shutting off power to all implements until the operator manually disengages it.
			// This should only be called if something very wrong is happening, and under well-documented cases
			void engage();

			// checks if the controller has been asserting the e-stop line for long enough to engage the e-stop.
			// The e-stop contains a hardware latch, so once the e-stop line is pulled low for long enough to energize
			// the relay coil, the e-stop will engage until it is manually reset. Calling update() causes the controller
			// to stop asserting the e-stop line after enough time has passed, which allows for painless e-stop recovery.
			// As such, it should be called every iteration of the main control loop.
			void update();
	};
}