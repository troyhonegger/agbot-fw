#include <Arduino.h>

#include "SerialApi.h"
#include "Config.h"
#include "Estop.h"
#include "Sprayer.h"
#include "Tiller.h"

void setup() {
	initConfig();
	initTillers();
	initSprayers();
	initSerialApi();
	pinMode(8, INPUT_PULLUP);
}

#define UNSET_MODE (0)
#define PROCESS_MODE (1)
#define DIAG_MODE (2)

static uint8_t currentMode = UNSET_MODE;

bool processResetCommand(char *message) {
	switch (currentMode) {
		case PROCESS_MODE:
			resetSprayers((2 << NUM_SPRAYERS) - 1);
			resetTillers((2 << NUM_TILLERS) - 1);
			break;
		case DIAG_MODE:
			diagSetSprayer((2 << NUM_SPRAYERS) - 1, SPRAYER_DIAG_OFF);
			diagSetTiller((2 << NUM_TILLERS) - 1, MAX_TILLER_HEIGHT);
			break;
		default: // unset mode - everything is already stopped
			break;
	}
	return true;
}

bool processSetModeCommand(char *message) {
	if (message[2] != '=') { return false; }
	switch(message[3]) {
		case 'P':
		case 'p':
			if (currentMode != PROCESS_MODE) {
				currentMode = PROCESS_MODE;
				sprayersEnterProcessMode();
				tillersEnterProcessMode();
				printMessage(MSG_LVL_INFORMATION, F("Machine is now in process mode."));
			}
			return true;
		case 'D':
		case 'd':
			if (currentMode != DIAG_MODE) {
				currentMode = DIAG_MODE;
				sprayersEnterDiagMode();
				tillersEnterDiagMode();
				printMessage(MSG_LVL_INFORMATION, F("Machine is now in diag mode."));
			}
			return true;
		default:
			return false;
	}
}

bool processGetStateCommand(char *message) {
	switch (message[2]) {
		case 'M':
		case 'm':
			printStartMessage(MSG_LVL_INFORMATION);
			Serial.print(F("Machine mode="));
			switch (currentMode) {
				case PROCESS_MODE:
					Serial.print(F("PROCESS"));
					break;
				case DIAG_MODE:
					Serial.print(F("DIAG"));
					break;
				default:
					Serial.print(F("UNSET"));
					break;
			}
			Serial.print('\n');
			return true;
		case 'C':
		case 'c':
			switch (message[3]) {
				case 'P':
				case 'p':
					printStartMessage(MSG_LVL_INFORMATION);
					Serial.print(F("Precision=")); Serial.print(getPrecision()); Serial.print('\n');
					return true;
				case 'D':
				case 'd':
					printStartMessage(MSG_LVL_INFORMATION);
					Serial.print(F("Response Delay=")); Serial.print(getTotalDelay()); Serial.print('\n');
					return true;
				case 'A':
				case 'a':
					printStartMessage(MSG_LVL_INFORMATION);
					Serial.print(F("Tiller Accuracy=")); Serial.print(getTillerAccuracy()); Serial.print('\n');
					return true;
				case 'R':
				case 'r':
					printStartMessage(MSG_LVL_INFORMATION);
					Serial.print(F("Tiller Raise Time=")); Serial.print(getTillerRaiseTime()); Serial.print('\n');
					return true;
				case 'L':
				case 'l':
					printStartMessage(MSG_LVL_INFORMATION);
					Serial.print(F("Tiller Lower Time=")); Serial.print(getTillerLowerTime()); Serial.print('\n');
					return true;
				default:
					return false;
			}
		case 'S':
		case 's':
			uint8_t sprayerId;
			if (parseNum(message + 3, &sprayerId, NUM_SPRAYERS - 1) >= 1) {
				printStartMessage(MSG_LVL_INFORMATION);
				Serial.print(F("Sprayer ")); Serial.printNumber(sprayerId, 10);
				if (sprayerList[sprayerId].status == SPRAYER_ON) {
					Serial.print(F(" ON\n"));
				}
				else {
					Serial.print(F(" OFF\n"));
				}
				return true;
			}
			else {
				return false;
			}
		case 'T':
		case 't':
			uint8_t tillerId;
			if (parseNum(message + 3, &tillerId, NUM_TILLERS - 1) >= 1) {
				printStartMessage(MSG_LVL_INFORMATION);
				Serial.print(F("Tiller ")); Serial.printNumber(tillerId, 10);
				Serial.print(F(": Actual Height=")); Serial.printNumber(tillerList[tillerId].actualHeight, 10);
				Serial.print(F(", Target Height="));
				if (estopEngaged ||
					tillerList[tillerId].state == TILLER_DIAG_RAISING ||
					tillerList[tillerId].state == TILLER_DIAG_LOWERING ||
					tillerList[tillerId].state == TILLER_DIAG_STOPPED) {
					Serial.print(F("<na>"));
				}
				else if (tillerList[tillerId].targetHeight == TILLER_STOP) {
					Serial.print(F("<stop>"));
				}
				else {
					Serial.printNumber(tillerList[tillerId].actualHeight, 10);
				}
				Serial.print(F(", Status="));
				switch (tillerList[tillerId].dh) {
					case TILLER_DH_DOWN:
						Serial.print(F(", Status=LOWERING\n"));
						break;
					case TILLER_DH_UP:
						Serial.print(F(", Status=RAISING\n"));
						break;
					case TILLER_DH_STOPPED:
						Serial.print(F(", Status=STOPPED\n"));
						break;
					default:
						Serial.print(F(", Status=??????\n"));
						break;
				}
				return true;
			}
			else {
				return false;
			}
			
		default:
			return false;
	}
}

bool processsSetConfigCommand(char *message) {
	if (!message[2] || (message[3] != '=')) { return false; }
	unsigned long newValue;
	if (parseNum(message + 4, &newValue, 0xFFFFFFFFL) <= 0) { return false; }
	switch (message[2]) {
		case 'P': // precision
		case 'p':
			setPrecision(newValue);
			printStartMessage(MSG_LVL_INFORMATION);
			Serial.print(F("Set precision to ")); Serial.print(newValue); Serial.print('\n');
			return true;
		case 'D': // total delay
		case 'd':
			setTotalDelay(newValue);
			printStartMessage(MSG_LVL_INFORMATION);
			Serial.print(F("Set total delay to ")); Serial.print(newValue); Serial.print('\n');
			return true;
		case 'A': // tiller accuracy
		case 'a':
			if (newValue > 100) {
				return false;
			}
			else {
				setTillerAccuracy((uint8_t) newValue);
				printStartMessage(MSG_LVL_INFORMATION);
				Serial.print(F("Set tiller accuracy to ")); Serial.print(newValue); Serial.print('\n');
				return true;
			}
		case 'R': // tiller raise time
		case 'r':
			setTillerRaiseTime(newValue);
			printStartMessage(MSG_LVL_INFORMATION);
			Serial.print(F("Set tiller raise time to ")); Serial.print(newValue); Serial.print('\n');
			return true;
		case 'L': // tiller lower time
		case 'l':
			setTillerLowerTime(newValue);
			printStartMessage(MSG_LVL_INFORMATION);
			Serial.print(F("Set tiller lower time to ")); Serial.print(newValue); Serial.print('\n');
			return true;
		default:
			return false;
	}
}

bool processDiagCommand(char *message) {
	// TODO: implement
	printMessage(MSG_LVL_ERROR, F("Diagnostic commands are not yet implemented"));
	return false;
}

bool processProcessCommand(char *message) {
	if (currentMode != PROCESS_MODE) {
		return false;
	}
	unsigned long code;
	if (parseDigits(message + 2, &code, 0xFFFFFLU, 16) == 5) {
		scheduleTillerLower(((code & 0x70000) ? 1 : 0) | ((code & 0x700) ? 2 : 0) | ((code & 0x7) ? 4 : 0));
		// NOTE: the following mapping assumes that sprayer 0 sprays foxtail, sprayer 1 sprays cocklebur, sprayer 2 sprays ragweed,
		// and sprayer 3 sprays fertilizer in order to be compliant with the API specification for the bit order. If necessary,
		// a different mapping could technically be configured in firmware, and even set on-the-fly via a saved setting. However,
		// it's probably best to leave the mapping as it is and allow the software to map which weed corresponds to which bit.
		scheduleSpray(((code & 0xF000) >> 12) | (code & 0xF0));
		return true;
	}
	else {
		return false;
	}
}

void processMessage(char *message) {
	if (*message != MESSAGE_START) {
		goto msgInvalid;
	}
	switch (getMessageType(message)) {
		case MSG_CMD_RESET:
			if (processResetCommand(message)) { break; }
			else { goto msgInvalid; }
			break;
		case MSG_CMD_SET_MODE:
			if (processSetModeCommand(message)) { break; }
			else { goto msgInvalid; }
			break;
		case MSG_CMD_GET_STATE:
			if (processGetStateCommand(message)) { break; }
			else { goto msgInvalid; }
			break;
		case MSG_CMD_SET_CONFIG:
			if (processsSetConfigCommand(message)) { break; }
			else { goto msgInvalid; }
			break;
		case MSG_CMD_DIAG:
			if (processDiagCommand(message)) { break; }
			else { goto msgInvalid; }
			break;
		case MSG_CMD_PROCESS:
			if (processProcessCommand(message)) { break; }
			else { goto msgInvalid; }
			break;
		default: // MSG_CMD_UNRECOGNIZED
			goto msgInvalid;
			break;
	}
	return;

	msgInvalid:
		printStartMessage(MSG_LVL_WARNING);
		Serial.print(F("Skipping invalid message: "));
		Serial.print(message);
		Serial.print('\n');
		return;
}

void loop() {
	readSerial();
	if (serialMessageAvailable()) {
		processMessage(getSerialMessage());
	}
	updateSprayers();
	updateTillers();
}