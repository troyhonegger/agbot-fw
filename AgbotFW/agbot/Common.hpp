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

// Comment this line to use a static IP address of 10.0.0.2; un-comment to use DHCP
#define DHCP

void assertImpl(bool condition, const char* conditionStr, const char* file, int line);

#ifdef DEBUG
#define assert(x) assertImpl(x, PSTR(#x), PSTR(__FILE__), __LINE__)
#else
#define assert(x)
#endif

// returns: > 0 if t1 comes after t2; < 0 if t1 comes before t2; 0 if t1 equals t2
inline bool timeCmp(unsigned long t1, unsigned long t2) {
	unsigned long diff = t1 - t2;
	if (diff == 0) {
		return 0;
	}
	else if (diff < ((~0UL)>>1)) {
		return 1;
	}
	else {
		return -1;
	}
}

// technically, this function isn't perfect - if time is more than about 25 days in the future, it will be reported as elapsed,
// and if it is more than about 25 days in the past, it will be reported as not elapsed, due to arithmetic overflow. However,
// it is as accurate as possible given the constraints of the system architecture.
inline bool isElapsed(unsigned long time) { return timeCmp(millis(), time) >= 0; }

struct Timer {
	uint32_t time;
	bool isSet;
	// start the timer. If it is already started, do nothing.
	void start(uint32_t delay);
	// restart the timer. If the timer is not already started, start it.
	void restart(uint32_t delay);
	// stop the timer. If it is already stopped, do nothing.
	void stop(void);
	// if the timer is set and the time has elapsed, stop the timer and return true. Otherwise, return false.
	bool isUp(void);
	// returns true as long as the timer was set in the past, and has now elapsed. Upon triggering, subsequent calls
	// will return true until the timer is started again.
	bool hasElapsed(void);

	Timer();
private:
	bool wasSet;
};