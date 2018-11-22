/*
 * Config.cpp
 * There are a handful of settings that need to be stored in EEPROM so that they can be configured at runtime and persisted
 * across application instances. This module implements functions for reading and writing those values. The "getter" functions
 * can be called anytime - the settings are buffered into memory so there is no data read overhead. The "setter" functions,
 * however, do have overhead and, in general, should not be called except upon request over the serial port.
 * Created: 11/21/2018 8:24:59 PM
 *  Author: troy.honegger
 */

#include "Config.h"

#include <Arduino.h>
#include <EEPROM.h>

// This setting represents the length, in milliseconds, of the window for weed elimination - smaller values mean more precision.
// For example, if the precision is 500, the tillers will be lowered and the sprayers will be activated for 500 milliseconds
// around the weed. The time interval is centered around total_delay, so if total_delay is 1250, the window will begin 1000ms
// after receiving the message and end 1500ms after receiving the message.
#define PRECISION_EEPROM_LOCATION (0)
static unsigned long precision;
unsigned long getPrecision(void) { return precision; }
void setPrecision(unsigned long a_precision) {
	precision = a_precision;
	EEPROM.put(PRECISION_EEPROM_LOCATION, precision);
}

// This setting represents the total length of time between when the Arduino is notified of the weed and when the weed is eliminated.
#define TOTAL_DELAY_EEPROM_LOCATION (PRECISION_EEPROM_LOCATION + sizeof(precision));
static unsigned long totalDelay;
unsigned long getTotalDelay(void) { return totalDelay; }
void setTotalDelay(unsigned long a_totalDelay) {
	totalDelay = a_totalDelay;
	EEPROM.put(TOTAL_DELAY_EEPROM_LOCATION, totalDelay);
}

// This setting represents the amount of time, in milliseconds, the tiller takes to raise.
#define TILLER_RAISE_TIME_EEPROM_LOCATION (TOTAL_DELAY_EEPROM_LOCATION + sizeof(totalDelay));
static unsigned long tillerRaiseTime;
unsigned long getTillerRaiseTime(void) { return tillerRaiseTime; }
void setTillerRaiseTime(unsigned long a_raiseTime) {
	tillerRaiseTime = a_raiseTime;
	EEPROM.put(TILLER_RAISE_TIME_EEPROM_LOCATION, tillerRaiseTime);
}

// This setting represents the amount of time, in milliseconds, the tiller takes to lower.
#define TILLER_LOWER_TIME_EEPROM_LOCATION (TILLER_RAISE_TIME_EEPROM_LOCATION + sizeof(tillerRaiseTime));
static unsigned long tillerLowerTime;
unsigned long getTillerLowerTime(void) { return tillerLowerTime; }
void setTillerLowerTime(unsigned long a_lowerTime) {
	tillerLowerTime = a_lowerTime;
	EEPROM.put(TILLER_LOWER_TIME_EEPROM_LOCATION, tillerLowerTime);
}

// This setting represents how closely the tillers should be kept to their target value. A value of 10 allows a +/-10% variation,
// a value of 5 allows a +/-5% variation, etc. This should be a number between 0 and 100, though the implementation does nothing
// to enforce this - the enforcement is up to the consumer.
#define TILLER_ACCURACY_EEPROM_LOCATION (TILLER_LOWER_TIME_EEPROM_LOCATION + sizeof(tillerLowerTime));
static uint8_t tillerAccuracy;
uint8_t getTillerAccuracy(void) { return tillerAccuracy; }
void setTillerAccuracy(uint8_t a_tillerAccuracy) {
	tillerAccuracy = a_tillerAccuracy;
	EEPROM.put(TILLER_ACCURACY_EEPROM_LOCATION, tillerAccuracy);
}

void initConfig(void) {
	EEPROM.get(PRECISION_EEPROM_LOCATION, precision);
	EEPROM.get(TOTAL_DELAY_EEPROM_LOCATION, totalDelay);
	EEPROM.get(TILLER_RAISE_TIME_EEPROM_LOCATION, tillerRaiseTime);
	EEPROM.get(TILLER_LOWER_TIME_EEPROM_LOCATION, tillerLowerTime);
	EEPROM.get(TILLER_ACCURACY_EEPROM_LOCATION, tillerAccuracy);
}