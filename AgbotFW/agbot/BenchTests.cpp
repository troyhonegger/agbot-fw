/*
 * BenchTests.cpp
 *
 * These tests are designed to run on Arduino hardware, and test functions one-at-a-time.
 * They are designed to verify the code in small units, rather than the entire system.
 * They are similar to unit tests in a few ways, but there are some important differences.
 * For one, the code is running on the hardware itself. For another, rather than using IOC
 * to mock out dependencies, this simply tests core functions first and dependent functions
 * later, and stops at the first failure. If the timer code has a bug, the tiller code that
 * depends on it will probably be broken; but rather than mocking out the timer and testing
 * the tiller independently, we just run the timer tests first.
 *
 * Created: 11/20/2021 12:43:04 PM
 *  Author: troy.honegger
 */ 

#include "Common.hpp"
#include "Log.hpp"

#ifdef BENCH_TESTS

#if !defined(DEBUG) || defined(ASSERT_FAIL_RETRY)
#error To run bench tests, ensure DEBUG is defined and ASSERT_FAIL_RETRY is not
#endif

void timerTests(void);

void setup() {
#if LOG_LEVEL != LOG_LEVEL_OFF
	Serial.begin(115200);
	Log.begin(&Serial);
#endif

	LOG_INFO("Beginning bench tests");
	
	timerTests();
	
	LOG_INFO("All tests passed");
}

void timerTests(void) {
	LOG_INFO("Timer tests");
	assert(timeCmp(0, 0) == 0);
	assert(timeCmp(0x7fffffff, 0x7fffffff) == 0);
	assert(timeCmp(10, 20) == -1);
	assert(timeCmp(0xffffff00, 1000) == -1);
	assert(timeCmp(20, 10) == 1);
	assert(timeCmp(1000, 0xffffff00) == 1);
	
	assert(!isElapsed(millis() + 0x3fffffff));
	assert(isElapsed(millis() - 0x3fffffff));

	Timer t;
	t.start(100);
	assert(t.isSet);
	assert(!isElapsed(t.time));
	delay(50);
	assert(t.isSet);
	assert(!isElapsed(t.time));
	assert(!t.isUp());
	assert(!t.hasElapsed());
	
	t.start(200); // should do nothing as timer is already running
	delay(75);
	assert(t.isSet);
	assert(isElapsed(t.time));
	assert(t.hasElapsed());
	assert(t.isSet);
	assert(t.isUp());
	assert(!t.isSet);
	assert(t.hasElapsed());
	assert(!t.isUp());
	
	t.start(100);
	assert(t.isSet); // test that it resets correctly after triggering
	delay(50);
	t.restart(100); // should reset the timer, so it has 100ms left instead of 50
	assert(t.isSet);
	delay(75);
	assert(!t.isUp());
	assert(!t.hasElapsed());
	delay(50);
	assert(t.isUp());
	assert(!t.isSet);

	t.start(100);
	assert(t.isSet);
	t.stop();
	assert(!t.isSet);
}

void loop() {
	// do nothing - if we get here, tests are complete and we passed
}

#endif // UNIT_TESTS