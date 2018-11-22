/*
 * TimeHelper.cpp
 * This module implements the TiemHelper header with a set of helper functions for working with time. Currently, the only helper is the timeElapsed function
 * which determines if a given time has already elapsed or not. However, it may be expanded to include more functions in the future
 * Created: 11/21/2018 7:03:00 PM
 *  Author: troy.honegger
 */ 

 #include "TimeHelper.h"

 #include <Arduino.h>

 #define MAX_ULONG (~0UL)

 // technically, this function isn't perfect - if time is more than about 25 days in the future, it will be reported as elapsed,
 // and if it is more than about 25 days in the past, it will be reported as not elapsed, due to arithmetic overflow. However,
 // it is as accurate as possible given the constraints of the system architecture.
 bool timeElapsed(unsigned long time) {
	return millis() - time < (MAX_ULONG / 2);
 }