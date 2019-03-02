/*
 * Common.hpp
 * Includes functionality, helper functions, and definitions used across multiple modules of the agBot firmware.
 * Created: 3/1/2019 11:23:45 PM
 *  Author: troy.honegger
 */ 
#pragma once

namespace agbot {
	enum class MachineMode : uint8_t {
		Unset = 0, Process = 1, Diag = 2
	};

	inline bool isElapsed(unsigned long time) { return millis() - time < ((~0UL)>>1); }
}