/*
 * Common.cpp
 *
 * Created: 2/26/2020 3:58:49 PM
 *  Author: troy.honegger
 */ 

#include "Common.hpp"
#include "Log.hpp"

void assertImpl(bool condition, const char* conditionStr, const char* file, int line) {
	if (!condition) {
		LOG_ERROR("%S:%d - assert(%S) failed.", file, line, conditionStr);
#ifdef ASSERT_FAIL_RETRY
		delay(50); // approximately enough time to write the error message
		asm("jmp 0x0"); // address zero is the "reset" interrupt vector
#else
		while(1);
#endif
	}
}

Timer::Timer() : time(0), isSet(false), wasSet(false) {}

void Timer::start(uint32_t delay) {
	if (!isSet) {
		time = millis() + delay;
		isSet = true;
		wasSet = false;
	}
}

void Timer::restart(uint32_t delay) {
	time = millis() + delay;
	isSet = true;
	wasSet = false;
}

void Timer::stop(void) {
	isSet = false;
	wasSet = false;
}

bool Timer::isUp(void) {
	if (isSet && isElapsed(time)) {
		isSet = false;
		wasSet = true;
		return true;
	}
	return false;
}

bool Timer::hasElapsed(void) {
	if (wasSet) {
		return true;
	}
	else if (isSet && isElapsed(time)) {
		wasSet = true;
		return true;
	}
	else {
		return false;
	}
}
