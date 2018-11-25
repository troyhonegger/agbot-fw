/*
 * Tiller.cpp
 * This module contains a set of functions to be used to interact with the tillers on the machine. It declares a global array
 * containing all three tillers on the machine. It also defines diagnostic operations (to raise, lower, and stop tillers manually),
 * as well as processing operations (to schedule tiller operations as weeds are encountered).
 * Created: 11/21/2018 8:13:00 PM
 *  Author: troy.honegger
 */

#include "Tiller.h"

#include <Arduino.h>

#include "Config.h"
#include "TimeHelper.h"

struct tiller tillerList[NUM_TILLERS];
bool estopEngaged;

//////////////////////////////////////////////////////////////////////////
// I/O helper functions
//////////////////////////////////////////////////////////////////////////

#define MIN_TILLER_PIN (2)
static inline int tillerRaisePin(uint8_t tillerId) { return MIN_TILLER_PIN + tillerId * 2; }
static inline int tillerLowerPin(uint8_t tillerId) { return MIN_TILLER_PIN + tillerId * 2 + 1; }

#define TILLER_PIN_ACTIVE_VOLTAGE (HIGH);
#define TILLER_PIN_INACTIVE_VOLTAGE (LOW);

// This function ONLY performs I/O. It does NOT check to make sure if stopping a tiller is consistent with that tiller's state,
// and it does NOT update a tiller's state after the operation - it is meant to be used purely as a helper function.
// Note that tillerId is an array index, NOT a bitfield, so this function can only stop one tiller at a time.
static inline void stopTiller(uint8_t tillerId) {
	digitalWrite(tillerRaisePin(tillerId), TILLER_PIN_INACTIVE_VOLTAGE);
	digitalWrite(tillerLowerPin(tillerId), TILLER_PIN_INACTIVE_VOLTAGE);
}

// This function ONLY performs I/O. It does NOT check to make sure if raising a tiller is consistent with that tiller's state,
// and it does NOT update a tiller's state after the operation - it is meant to be used purely as a helper function.
// Note that tillerId is an array index, NOT a bitfield, so this function can only raise one tiller at a time.
static inline void raiseTiller(uint8_t tillerId) {
	digitalWrite(tillerLowerPin(tillerId), TILLER_PIN_INACTIVE_VOLTAGE);
	digitalWrite(tillerRaisePin(tillerId), TILLER_PIN_ACTIVE_VOLTAGE);
}

// This function ONLY performs I/O - it does NOT check to make sure if lowering a tiller is consistent with that tiller's state,
// and it does NOT update a tiller's state after the operation - it is meant to be used purely as a helper function.
// Note that tillerId is an array index, NOT a bitfield, so this function can only lower one tiller at a time.
static inline void lowerTiller(uint8_t tillerId) {
	digitalWrite(tillerRaisePin(tillerId), TILLER_PIN_INACTIVE_VOLTAGE);
	digitalWrite(tillerLowerPin(tillerId), TILLER_PIN_ACTIVE_VOLTAGE);
}

//////////////////////////////////////////////////////////////////////////
// Initialization functions
//////////////////////////////////////////////////////////////////////////

// Initializes tillerList, configures the GPIO pins, and performs all other necessary initialization work for the tillers.
// Note that tillersEnter???Mode() still needs to be called before diag or process functions are called.
void initTillers(void) {
	estopEngaged = false;
	for (uint8_t i = 0; i < NUM_TILLERS; i++) {
		pinMode(tillerRaisePin(i), OUTPUT);
		pinMode(tillerLowerPin(i), OUTPUT);
		stopTiller(i);
		tillerList[i].lowerTime = tillerList[i].raiseTime = millis();
		tillerList[i].state = TILLER_STATE_UNSET;
		tillerList[i].dh = TILLER_DH_STOPPED;
		tillerList[i].targetHeight = TILLER_STOP;
		tillerList[i].actualHeight = MAX_TILLER_HEIGHT;
	}
}

// Raises all tillers and switches them to process mode. This can be used to recover from an e-stop and, as such, should ONLY be called
// upon request over the serial port.
void tillersEnterProcessMode(void) {
	estopEngaged = false;
	for (uint8_t i = 0; i < NUM_TILLERS; i++) {
		tillerList[i].lowerTime = tillerList[i].raiseTime = millis();
		tillerList[i].state = TILLER_PROCESS_RAISING;
		tillerList[i].targetHeight = MAX_TILLER_HEIGHT;
	}
}

// Stops all tillers and switches them to diag mode. This can be used to recover from an e-stop and, as such, should ONLY be called
// upon request over the serial port.
void tillersEnterDiagMode(void) {
	estopEngaged = false;
	for (uint8_t i = 0; i < NUM_TILLERS; i++) {
		tillerList[i].lowerTime = tillerList[i].raiseTime = millis();
		tillerList[i].state = TILLER_DIAG_STOPPED;
		tillerList[i].targetHeight = TILLER_STOP;
	}
}

//////////////////////////////////////////////////////////////////////////
// Processing functions
//////////////////////////////////////////////////////////////////////////

// tillers is a bitfield here, NOT an array index. This should only be called when in process mode.
void scheduleTillerLower(uint8_t tillers) {
	if (estopEngaged) return;
	uint8_t bitmask = 1;
	for (int i = 0; i < NUM_TILLERS; i++) {
		if (tillers & bitmask) {
			if (tillerList[i].state != TILLER_PROCESS_SCHEDULED) {
				tillerList[i].lowerTime = millis() + getTotalDelay() - getTillerRaiseTime() - getPrecision() / 2;
			}
			tillerList[i].raiseTime = millis() + getTotalDelay() + getPrecision() / 2;
			tillerList[i].state = TILLER_PROCESS_SCHEDULED;
		}
		bitmask <<= 1;
	}
}

// TODO: I forget if this should be analog pin 0 or analog pin 3 so as to not conflict with the sprayer pins
#define MIN_TILLER_HEIGHT_SENSOR_PIN (0)
static inline uint8_t tillerHeightSensorPin(uint8_t tillerId) { return MIN_TILLER_HEIGHT_SENSOR_PIN + tillerId; }

// Updates the actualHeight property of the specified tiller. This should only be called from inside
// updateTillers(). Note that tillerId is an array index, NOT a bitfield, so this function can only update
// one tiller at a time. This may be implemented in different ways depending on the hardware. If there are
// potentiometers that can be used to read the tiller height, they may be read from via the analog inputs
// in this function. Otherwise, this function may use a simple clock and estimate the actual height from
// the previous value.
static inline void updateTillerActualHeight(uint8_t tillerId) {
	// TODO: implementation may vary, depending on whether there are height sensors at a hardware level.
	tillerList[tillerId].actualHeight = map(analogRead(tillerHeightSensorPin(tillerId)), 0, 1023, 0, MAX_TILLER_HEIGHT);
}

// Updates the state property of the specified tiller by performing any necessary scheduled state changes.
// Note that tillerId is an array index, NOT a bitfield, so this function can only update one tiller at a
// time. This should only be called from inside updateTillers().
static inline void updateTillerState(uint8_t tillerId) {
	if (tillerList[tillerId].state == TILLER_PROCESS_SCHEDULED && timeElapsed(tillerList[tillerId].lowerTime)) {
		tillerList[tillerId].state = TILLER_PROCESS_LOWERING;
		tillerList[tillerId].targetHeight = 0;
	}
	else if (tillerList[tillerId].state == TILLER_PROCESS_LOWERING && timeElapsed(tillerList[tillerId].raiseTime)) {
		tillerList[tillerId].state = TILLER_PROCESS_RAISING;
		tillerList[tillerId].targetHeight = MAX_TILLER_HEIGHT;
	}
}

// Updates the dh property of the specified tiller (taking state into account) by comparing the targetHeight
// and actualHeight values. Note that tillerId is an array index, NOT a bitfield, so this function can only
// update one tiller at a time. Also note that this function DOES perform I/O when necessary, so it should
// only be called from inside updateTillers().
static inline void updateTillerDH(uint8_t tillerId) {
	if (estopEngaged) return;
	
	// compute newDh
	uint8_t newDh = TILLER_DH_STOPPED;
	switch (tillerList[tillerId].state){
		case TILLER_STATE_UNSET: break;
		case TILLER_DIAG_STOPPED: break;
		case TILLER_DIAG_RAISING:
			newDh = TILLER_DH_UP;
		case TILLER_DIAG_LOWERING:
			newDh = TILLER_DH_DOWN;
		default:
			if (tillerList[tillerId].targetHeight == TILLER_STOP) {
				newDh = TILLER_DH_STOPPED;
			}
			else {
				int8_t diff = tillerList[tillerId].targetHeight - tillerList[tillerId].actualHeight;
				if (abs(diff) <= getTillerAccuracy()) {
					newDh = TILLER_DH_STOPPED;
				}
				else {
					newDh = diff > 0 ? TILLER_DH_UP : TILLER_DH_DOWN;
				}
			}
			break;
	}

	// compare newDh to current dh and update if necessary
	if (newDh != tillerList[tillerId].dh) {
		tillerList[tillerId].dh = newDh;
		switch (newDh) {
			case TILLER_DH_UP:
				raiseTiller(tillerId);
				break;
			case TILLER_DH_DOWN:
				lowerTiller(tillerId);
				break;
			default:
				stopTiller(tillerId);
				break;
		}
	}
}

// Performs any scheduled operations and updates the actualHeight and dh fields of all tillers, performing
// I/O where necessary. This should be called every loop iteration (in both diag and process mode), as neither
// diag nor processing functions will directly perform I/O - instead, they will schedule the operation(s) to
// be performed by this function.
void updateTillers(void) {
	if (estopEngaged) return;
	for (int i = 0; i < NUM_TILLERS; i++) {
		updateTillerActualHeight(i);
		updateTillerState(i);
		updateTillerDH(i);
	}
}

//////////////////////////////////////////////////////////////////////////
// Diag functions
//////////////////////////////////////////////////////////////////////////

// tillers is a bitfield here, NOT an array index. This should only be called when in diag mode.
void diagRaiseTiller(uint8_t tillers) {
	if (estopEngaged) return;
	uint8_t bitmask = 1;
	for (int i = 0; i < NUM_TILLERS; i++) {
		if (tillers & bitmask) {
			tillerList[i].lowerTime = tillerList[i].raiseTime = millis();
			tillerList[i].state = TILLER_DIAG_RAISING;
			tillerList[i].targetHeight = MAX_TILLER_HEIGHT;
		}
	}
}

// tillers is a bitfield here, NOT an array index. This should only be called when in diag mode.
void diagLowerTiller(uint8_t tillers) {
	if (estopEngaged) return;
	uint8_t bitmask = 1;
	for (int i = 0; i < NUM_TILLERS; i++) {
		if (tillers & bitmask) {
			tillerList[i].lowerTime = tillerList[i].raiseTime = millis();
			tillerList[i].state = TILLER_DIAG_LOWERING;
			tillerList[i].targetHeight = 0;
		}
	}
}

// tillers is a bitfield here, NOT an array index. This should only be called when in diag mode.
void diagStopTiller(uint8_t tillers) {
	if (estopEngaged) return;
	uint8_t bitmask = 1;
	for (int i = 0; i < NUM_TILLERS; i++) {
		if (tillers & bitmask) {
			tillerList[i].lowerTime = tillerList[i].raiseTime = millis();
			tillerList[i].state = TILLER_DIAG_STOPPED;
			tillerList[i].targetHeight = TILLER_STOP;
		}
	}
}

// tillers is a bitfield here, NOT an array index. This should only be called when in diag mode.
void diagSetTiller(uint8_t tillers, uint8_t target) {
	if (estopEngaged) return;
	uint8_t bitmask = 1;
	for (int i = 0; i < NUM_TILLERS; i++) {
		if (tillers & bitmask) {
			tillerList[i].lowerTime = tillerList[i].raiseTime = millis();
			tillerList[i].state = TILLER_DIAG_TARGET_VALUE;
			tillerList[i].targetHeight = target;
		}
		bitmask <<= 1;
	}
}

//////////////////////////////////////////////////////////////////////////
// E-Stop functions
//////////////////////////////////////////////////////////////////////////

// Stops every tiller raise/lower operation and sets the estopEngaged flag so updateTillers() knows not to do anything.
void estopTillers(void) {
	estopEngaged = true;
	// first, we 
	for (int i = 0; i < NUM_TILLERS; i++) {
		stopTiller(i);
	}
	// though not strictly necessary, we modify every tiller's state to values that minimize the chance
	// that any function will try to turn one on.
	for (int i = 0; i < NUM_TILLERS; i++) {
		tillerList[i].state = TILLER_STATE_UNSET;
		tillerList[i].targetHeight = TILLER_STOP;
	}
}