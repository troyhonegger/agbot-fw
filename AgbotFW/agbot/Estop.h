/*
 * Estop.h
 * Contains the necessary logic for the controller to interface with the machine's hardware e-stop.
 * This module is critical for safety, but it should be handled with care - once the e-stop is triggered,
 * the user must manually disengage it to restore power to the implements.
 * 
 * This e-stop implementation uses a "fire-and-forget" strategy. If something happens that triggers an
 * e-stop, call estop.engage() right away. If something happens again, call estop.engage() again.
 * each call to engage() sends a pulse to the e-stop line that causes the e-stop line to latch on.
 * The pulse must last a few milliseconds at least, so to avoid a blocking call, call estop.update()
 * repeatedly throughout the code, to check if the e-stop has been engaged and it is time to disengage it.
 * 
 * Usage example:
 *	Estop estop;
 *	estop.begin();
 *	estop.engage();
 *	estop.update();
 * 
 * Created: 3/6/2019 1:30:36 PM
 *  Author: troy.honegger
 */ 

#pragma once

class Estop {
	private:
		static const uint8_t HW_PIN = 53;
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
