/*
 * Common.hpp
 * Includes functionality, helper functions, and definitions used across multiple modules of the agBot firmware.
 * Created: 3/1/2019 11:23:45 PM
 *  Author: troy.honegger
 */ 
#pragma once

#include <Arduino.h>

// Comment this line to run the "production" code; un-comment to run "TillAndSprayDemo.cpp"
//#define DEMO_MODE

namespace agbot {
	enum class MachineMode : uint8_t {
		Unset = 0, Process = 1, Diag = 2
	};

	// technically, this function isn't perfect - if time is more than about 25 days in the future, it will be reported as elapsed,
	// and if it is more than about 25 days in the past, it will be reported as not elapsed, due to arithmetic overflow. However,
	// it is as accurate as possible given the constraints of the system architecture.
	inline bool isElapsed(unsigned long time) { return millis() - time < ((~0UL)>>1); }
}