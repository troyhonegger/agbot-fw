/*
 * Tiller.cpp
 * This module contains a set of functions to be used to interact with the tillers on the machine. It declares a global array
 * containing all three tillers on the machine. It also defines diagnostic operations (to raise, lower, and stop tillers manually),
 * as well as processing operations (to schedule tiller operations as weeds are encountered).
 * Created: 11/21/2018 8:13:00 PM
 *  Author: troy.honegger
 */

// comment this out if tillers do not have analog height sensors to resort to a software-based timing approach.
#define TILLER_HEIGHT_SENSORS

#include "Tiller.h"

#include <Arduino.h>

#include "Config.h"
#include "Estop.h"
#include "TimeHelper.h"

struct tiller tillerList[NUM_TILLERS];

//////////////////////////////////////////////////////////////////////////
// I/O helper functions
//////////////////////////////////////////////////////////////////////////

#define MIN_TILLER_PIN (2)
static inline int tillerRaisePin(uint8_t tillerId) { return MIN_TILLER_PIN + tillerId * 2; }
static inline int tillerLowerPin(uint8_t tillerId) { return MIN_TILLER_PIN + tillerId * 2 + 1; }

#define TILLER_PIN_ACTIVE_VOLTAGE (LOW)
#define TILLER_PIN_INACTIVE_VOLTAGE (HIGH)

#ifdef TILLER_HEIGHT_SENSORS

#define MIN_TILLER_HEIGHT_SENSOR_PIN (PIN_A3)
static inline uint8_t tillerHeightSensorPin(uint8_t tillerId) { return MIN_TILLER_HEIGHT_SENSOR_PIN + tillerId; }

#endif // TILLER_HEIGHT_SENSORS

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
// This should only be called from inside the setup() function. Note that tillersEnter???Mode() still needs to be called
// before diag or process functions can be used.
void initTillers(void) {
	for (uint8_t i = 0; i < NUM_TILLERS; i++) {
		pinMode(tillerRaisePin(i), OUTPUT);
		pinMode(tillerLowerPin(i), OUTPUT);
		
		#ifdef TILLER_HEIGHT_SENSORS
		pinMode(tillerHeightSensorPin(i), INPUT);
		#endif

		stopTiller(i);
		
		tillerList[i].lowerTime = tillerList[i].raiseTime = millis();
		tillerList[i].state = TILLER_STATE_UNSET;
		tillerList[i].dh = TILLER_DH_STOPPED;
		tillerList[i].targetHeight = TILLER_STOP;
		tillerList[i].actualHeight = MAX_TILLER_HEIGHT;
	}
}

// Raises all tillers and switches them to process mode
void tillersEnterProcessMode(void) {
	if (estopEngaged) return;
	for (uint8_t i = 0; i < NUM_TILLERS; i++) {
		tillerList[i].lowerTime = tillerList[i].raiseTime = millis();
		tillerList[i].state = TILLER_PROCESS_RAISING;
		tillerList[i].targetHeight = MAX_TILLER_HEIGHT;
	}
}

// Stops all tillers and switches them to diag mode
void tillersEnterDiagMode(void) {
	if (estopEngaged) return;
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

// Cancels any scheduled tiller operations for the specified tillers and raises the given tillers. tillers is a bitfield
// here, NOT an array index. This should only be called when in process mode - in diag mode, use diagRaiseTiller(),
// diagStopTiller(), or diagSetTiller(MAX_TILLER_HEIGHT)
void resetTillers(uint8_t tillers) {
	if (estopEngaged) return;
	uint8_t bitmask = 1;
	for (int i = 0; i < NUM_TILLERS; i++) {
		if (tillers & bitmask) {
			if (tillerList[i].state == TILLER_PROCESS_SCHEDULED || tillerList[i].state == TILLER_PROCESS_LOWERING) {
				tillerList[i].lowerTime = tillerList[i].raiseTime = millis();
				tillerList[i].state = TILLER_PROCESS_RAISING;
			}
		}
		bitmask <<= 1;
	}
}

#ifdef TILLER_HEIGHT_SENSORS

// Updates the actualHeight property of the specified tiller by reading the height sensor associated with the tiller.
// This should only be called from inside updateTillers(). Note that tillerId is an array index, NOT a bitfield, so
// this function can only update one tiller at a time. This function is only defined if the multivator is equipped
// with height sensors for the tillers
static inline void updateTillerActualHeight(uint8_t tillerId) {
	tillerList[tillerId].actualHeight = map(analogRead(tillerHeightSensorPin(tillerId)), 0, 1023, 0, MAX_TILLER_HEIGHT);
}

#else

// Estimates the actual height of all tillers by multiplying the time difference since the last update by the tiller
// raise/lower speed. This should only be called from inside updateTilers() This is a somewhat crude method that may
// cause drift over time, so it is only defined if the multivator is not equipped with height sensors for the tillers.
static inline void estimateTillerHeightsFromTimer(void) {
	// TODO: implement
}

#endif // TILLER_HEIGHT_SENSORS

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
	int8_t newDh = TILLER_DH_STOPPED;
	switch (tillerList[tillerId].state){
		case TILLER_STATE_UNSET: break;
		case TILLER_DIAG_STOPPED: break;
		case TILLER_DIAG_RAISING:
			newDh = TILLER_DH_UP;
			break;
		case TILLER_DIAG_LOWERING:
			newDh = TILLER_DH_DOWN;
			break;
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

	#ifndef TILLER_HEIGHT_SENSORS
	estimateTillerHeightsFromTimer();
	#endif
	
	for (int i = 0; i < NUM_TILLERS; i++) {
		
		#ifdef TILLER_HEIGHT_SENSORS
		updateTillerActualHeight(i);
		#endif
		
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
		bitmask <<= 1;
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
		bitmask <<= 1;
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
		bitmask <<= 1;
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

// Manually stops each tiller - designed to be called only from estopMachine()
void estopTillers(void) {
	for (int i = 0; i < NUM_TILLERS; i++) {
		// first, we manually stop the tiller
		stopTiller(i);
		// though not technically necessary, we modify the tiller's state to values that minimize the chance
		// that any function will try to turn it on.
		tillerList[i].state = TILLER_STATE_UNSET;
		tillerList[i].targetHeight = TILLER_STOP;
	}
}

#include "Tiller.hpp"

namespace agbot {
	Tiller::Tiller(uint8_t id, Config const& config) :
			state(((id & 3) << 4) | static_cast<uint8_t>(TillerState::Unset)),
			raiseTime(millis()), lowerTime(millis()),
			targetHeight(STOP), actualHeight(MAX_HEIGHT), config(config) {
		pinMode(getRaisePin(), OUTPUT);
		digitalWrite(getRaisePin(), OFF_VOLTAGE);
		pinMode(getLowerPin(), OUTPUT);
		digitalWrite(getLowerPin(), OFF_VOLTAGE);
		pinMode(getHeightSensorPin(), INPUT);
	}

	Tiller::~Tiller() {
		pinMode(getRaisePin(), INPUT);
		pinMode(getLowerPin(), INPUT);
	}

	inline void Tiller::updateActualHeight() { actualHeight = map(analogRead(getHeightSensorPin()), 0, 1023, 0, MAX_HEIGHT); }

	void Tiller::setMode(MachineMode mode) {
		switch (mode) {
			case MachineMode::Unset:
				lowerTime = raiseTime = millis();
				setState(TillerState::Unset);
				targetHeight = STOP;
				break;
			case MachineMode::Process:
				lowerTime = raiseTime = millis();
				setState(TillerState::ProcessRaising);
				targetHeight = config.get(Setting::TillerRaisedHeight);
				break;
			case MachineMode::Diag:
				lowerTime = raiseTime = millis();
				setState(TillerState::Diag);
				targetHeight = STOP;
				break;
			default:
				break;
		}
	}

	void Tiller::setHeight(uint8_t height) {
		if (getState() == TillerState::Diag) {
			targetHeight = height;
		}
	}
	
	void Tiller::stop() {
		switch (getState()) {
			case TillerState::Diag:
				targetHeight = STOP;
				break;
			case TillerState::ProcessLowering:
			case TillerState::ProcessRaising:
			case TillerState::ProcessScheduled:
				raiseTime = lowerTime = millis();
				setState(TillerState::ProcessStopped);
				targetHeight = STOP;
				break;
		}
	}

	void Tiller::scheduleLower() {
		bool isProcessing = false;
		switch (getState()) {
			case TillerState::ProcessRaising:
			case TillerState::ProcessLowering:
				isProcessing = true;
				// lowerTime = millis() + responseDelay - (raisedHeight - loweredHeight)*tillerLowerTime/100 - precision/2
				lowerTime = millis() + config.get(Setting::ResponseDelay) -
					((config.get(Setting::TillerRaisedHeight) - config.get(Setting::TillerLoweredHeight))*config.get(Setting::TillerLowerTime)
					+ config.get(Setting::Precision) * 50) / 100;
				break;
			case TillerState::ProcessScheduled:
				isProcessing = true;
				break;
		}
		if (isProcessing) {
			raiseTime = millis() + config.get(Setting::ResponseDelay) + config.get(Setting::Precision) / 2;
			setState(TillerState::ProcessScheduled);
		}
	}

	void Tiller::cancelLower() {
		if (getState() == TillerState::ProcessScheduled || getState() == TillerState::ProcessRaising) {
			raiseTime = lowerTime = millis();
			setState(TillerState::ProcessRaising);
			targetHeight = config.get(Setting::TillerRaisedHeight);
		}
	}

	void Tiller::resume() {
		if (getState() == TillerState::ProcessStopped) {
			raiseTime = lowerTime = millis();
			setState(TillerState::ProcessRaising);
			targetHeight = config.get(Setting::TillerRaisedHeight);
		}
	}

	void Tiller::update() {
		if (getState() == TillerState::ProcessScheduled && timeElapsed(lowerTime)) {
			setState(TillerState::ProcessLowering);
			targetHeight = config.get(Setting::TillerLoweredHeight);
		}
		else if (getState() == TillerState::ProcessLowering && timeElapsed(raiseTime)) {
			setState(TillerState::ProcessRaising);
			targetHeight = config.get(Setting::TillerRaisedHeight);
		}

		updateActualHeight();
		int8_t newDh = 0;
		if (targetHeight != STOP) {
			if (targetHeight - actualHeight > config.get(Setting::TillerAccuracy)) { newDh = 1; }
			else if (targetHeight - actualHeight < -config.get(Setting::TillerAccuracy)) { newDh = -1; }
		}

		if (newDh != getDH()) {
			setDH(newDh);
			switch (newDh) {
				case 0:
					digitalWrite(getRaisePin(), OFF_VOLTAGE);
					digitalWrite(getLowerPin(), OFF_VOLTAGE);
					break;
				case 1:
					digitalWrite(getLowerPin(), OFF_VOLTAGE);
					digitalWrite(getRaisePin(), ON_VOLTAGE);
					break;
				case -1:
					digitalWrite(getRaisePin(), OFF_VOLTAGE);
					digitalWrite(getLowerPin(), ON_VOLTAGE);
					break;
			}
		}
	}
}