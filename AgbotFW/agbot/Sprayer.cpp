/*
 * Sprayer.cpp
 * This module contains a set of functions to be used to interact with the sprayers on the machine. It declares a global array
 * containing all eight sprayers on the machine. It also defines diagnostic operations (to turn sprayers on or off manually)
 * as well as processing operations (to schedule spraying as weeds are encountered).
 * Created: 11/21/2018 8:00:00 PM
 *  Author: troy.honegger
 */

#include "Sprayer.h"

#include "Config.h"
#include "Estop.h"
#include "TimeHelper.h"

#define SPRAYER_DELAY (1000)
#define SPRAYER_PRECISION (250)

// sprayer pins are numbered 9, 10, 11, 12, 13, 14, 15, 16
#define MIN_SPRAYER_PIN (9)

// Maps sprayerId to a pin number. Note that sprayerId is an array index, NOT a bitfield.
static inline uint8_t sprayerPinNumber(uint8_t sprayerId) { return MIN_SPRAYER_PIN + sprayerId; }

// sprayers are active low
#define SPRAYER_OFF_VOLTAGE (HIGH)
#define SPRAYER_ON_VOLTAGE (LOW)

struct sprayer sprayerList[NUM_SPRAYERS];

// Initializes sprayerList, configures the GPIO pins, and performs all other necessary initialization work for the sprayers.
// Note that sprayersEnter???Mode() still needs to be called before diag or process functions are called.
void initSprayers(void) {
	for (uint8_t i = 0; i < NUM_SPRAYERS; i++) {
		pinMode(sprayerPinNumber(i), OUTPUT);
		digitalWrite(sprayerPinNumber(i), SPRAYER_OFF_VOLTAGE);
		sprayerList[i].offTime = sprayerList[i].onTime = millis();
		sprayerList[i].state = SPRAYER_STATE_UNSET;
		sprayerList[i].status = SPRAYER_OFF;
	}
}

// Shuts off all sprayers and sets them to process mode. Note that the sprayers will be turned off (and all scheduled sprayer
// operations will be canceled) even if the sprayers were already in process mode. If the machine is already in process mode,
// this generally should not be called (though it might make sense under exceptional circumstances).
void sprayersEnterProcessMode(void) {
	if (estopEngaged) return;
	for (uint8_t i = 0; i < NUM_SPRAYERS; i++) {
		digitalWrite(sprayerPinNumber(i), SPRAYER_OFF_VOLTAGE);
		sprayerList[i].offTime = sprayerList[i].onTime = millis();
		sprayerList[i].state = SPRAYER_PROCESS_OFF;
		sprayerList[i].status = SPRAYER_OFF;
	}
}

// Shuts off all sprayers and sets them to diag mode. Note that the sprayers will be turned off even if the sprayers were already
// in diag mode. If the machine is already in diag mode, this generally should not be called (though it might make sense under
// exceptional circumstances).
void sprayersEnterDiagMode(void) {
	if (estopEngaged) return;
	for (uint8_t i = 0; i < NUM_SPRAYERS; i++) {
		digitalWrite(sprayerPinNumber(i), SPRAYER_OFF_VOLTAGE);
		sprayerList[i].offTime = sprayerList[i].onTime = millis();
		sprayerList[i].state = SPRAYER_DIAG_OFF;
		sprayerList[i].status = SPRAYER_OFF;
	}
}

// Schedules a spray operation to be performed after a length of time. This should only be called when in process mode. This
// operation performs no I/O, as it is only scheduling an operation to be performed later. Note that sprayers is referring
// to a bitfield, not an array index, so multiple sprayers can be scheduled at once.
void scheduleSpray(uint8_t sprayers) {
	if (estopEngaged) return;
	uint8_t bitmask = 1;
	for (uint8_t i = 0; i < NUM_SPRAYERS; i++) {
		if (sprayers & bitmask) {
			if (sprayerList[i].status != SPRAYER_PROCESS_SCHEDULED) {
				sprayerList[i].onTime = millis() + getTotalDelay() - getPrecision() / 2;
			}
			sprayerList[i].offTime = millis() + getTotalDelay() + getPrecision() / 2;
			sprayerList[i].status = SPRAYER_PROCESS_SCHEDULED;
		}
		bitmask <<= 1;
	}
}

// Performs any scheduled set sprayer operations. This should be called every loop iteration. In diag mode, it will have no effect.
void updateSprayers(void) {
	if (estopEngaged) return;
	for (uint8_t i = 0; i < 8; i++) {
		switch (sprayerList[i].state) {
			case SPRAYER_PROCESS_SCHEDULED:
				if (timeElapsed(sprayerList[i].onTime)) {
					digitalWrite(sprayerPinNumber(i), SPRAYER_ON_VOLTAGE);
					sprayerList[i].state = SPRAYER_PROCESS_ON;
					sprayerList[i].status = SPRAYER_ON;
				}
			break;
			case SPRAYER_PROCESS_ON:
				if (timeElapsed(sprayerList[i].offTime)) {
					digitalWrite(sprayerPinNumber(i), SPRAYER_OFF_VOLTAGE);
					sprayerList[i].state = SPRAYER_PROCESS_OFF;
					sprayerList[i].status = SPRAYER_OFF;
				}
			break;
			default: break;
		}
	}
}


// Sets the specified sprayers to the specified mode. The machine should be in diag mode when this is called. Note that sprayers
// is referring to a bitfield, not an array index, so multiple sprayers can be set at once.
void diagSetSprayer(uint8_t sprayers, bool status) {
	if (estopEngaged) return;
	uint8_t bitmask = 1;
	for (uint8_t i = 0; i < NUM_SPRAYERS; i++) {
		if (sprayers & bitmask) {
			if (status == SPRAYER_ON) {
				digitalWrite(sprayerPinNumber(i), SPRAYER_ON_VOLTAGE);
				sprayerList[i].offTime = sprayerList[i].onTime = millis();
				sprayerList[i].state = SPRAYER_DIAG_ON;
				sprayerList[i].status = SPRAYER_ON;
			}
			else {
				digitalWrite(sprayerPinNumber(i), SPRAYER_OFF_VOLTAGE);
				sprayerList[i].offTime = sprayerList[i].onTime = millis();
				sprayerList[i].state = SPRAYER_DIAG_OFF;
				sprayerList[i].status = SPRAYER_OFF;
			}
		}
		bitmask <<= 1;
	}
}

// Manually shuts off each sprayer - designed to be called only from estopMachine()
void estopSprayers(void) {
	for (uint8_t i = 0; i < NUM_SPRAYERS; i++) {
		// first, we manually shut off the sprayer
		digitalWrite(sprayerPinNumber(i), SPRAYER_OFF_VOLTAGE);
		// though not technically necessary, we modify the sprayer's state to a value that minimizes the chance
		// that any function will try to turn it on.
		sprayerList[i].state = SPRAYER_STATE_UNSET;
		sprayerList[i].status = SPRAYER_OFF;
	}
}