#include <Arduino.h>

#include "SerialApi.h"

#include "Config.hpp"
#include "Estop.hpp"
#include "Hitch.hpp"
#include "Tiller.hpp"
#include "Sprayer.hpp"

agbot::Config config;
agbot::Estop estop;
agbot::Hitch hitch;
agbot::Tiller tillers[agbot::Tiller::COUNT];
agbot::Sprayer sprayers[agbot::Sprayer::COUNT];
agbot::MachineMode currentMode = agbot::MachineMode::Unset;

void setup() {
	config.begin();
	estop.begin();
	hitch.begin(&config);
	for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
		tillers[i].begin(i, &config);
	}
	for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) {
		sprayers[i].begin(i, &config);
	}
	initSerialApi();
}

bool processResetCommand(char *message) {
	switch (currentMode) {
		case agbot::MachineMode::Process:
			for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) {
				sprayers[i].cancelSpray();
			}
			for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
				tillers[i].cancelLower();
			}
			break;
		case agbot::MachineMode::Diag:
			for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) {
				sprayers[i].setStatus(agbot::Sprayer::OFF);
			}
			for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
				tillers[i].stop();
			}
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
			if (currentMode != agbot::MachineMode::Process) {
				currentMode = agbot::MachineMode::Process;
				for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
					tillers[i].setMode(currentMode);
				}
				for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) {
					sprayers[i].setMode(currentMode);
				}
				printMessage(MSG_LVL_INFORMATION, F("Machine is now in process mode."));
			}
			return true;
		case 'D':
		case 'd':
			if (currentMode != agbot::MachineMode::Diag) {
				currentMode = agbot::MachineMode::Diag;
				for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
					tillers[i].setMode(currentMode);
				}
				for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) {
					sprayers[i].setMode(currentMode);
				}
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
				case agbot::MachineMode::Process:
					Serial.print(F("PROCESS"));
					break;
				case agbot::MachineMode::Diag:
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
					Serial.print(F("Precision=")); Serial.print(config.get(agbot::Setting::Precision)); Serial.print('\n');
					return true;
				case 'D':
				case 'd':
					printStartMessage(MSG_LVL_INFORMATION);
					Serial.print(F("Response Delay=")); Serial.print(config.get(agbot::Setting::ResponseDelay)); Serial.print('\n');
					return true;
				case 'A':
				case 'a':
					printStartMessage(MSG_LVL_INFORMATION);
					Serial.print(F("Tiller Accuracy=")); Serial.print(config.get(agbot::Setting::TillerAccuracy)); Serial.print('\n');
					return true;
				case 'R':
				case 'r':
					printStartMessage(MSG_LVL_INFORMATION);
					Serial.print(F("Tiller Raise Time=")); Serial.print(config.get(agbot::Setting::TillerLowerTime)); Serial.print('\n');
					return true;
				case 'L':
				case 'l':
					printStartMessage(MSG_LVL_INFORMATION);
					Serial.print(F("Tiller Lower Time=")); Serial.print(config.get(agbot::Setting::TillerLowerTime)); Serial.print('\n');
					return true;
				default:
					return false;
			}
		case 'S':
		case 's':
			uint8_t sprayerId;
			if (parseNum<uint8_t>(message + 3, &sprayerId, agbot::Sprayer::COUNT - 1) >= 1) {
				printStartMessage(MSG_LVL_INFORMATION);
				Serial.print(F("Sprayer ")); Serial.print((int) sprayerId, 10);
				if (sprayers[sprayerId].getStatus() == agbot::Sprayer::ON) {
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
			if (parseNum<uint8_t>(message + 3, &tillerId, agbot::Tiller::COUNT) >= 1) {
				printStartMessage(MSG_LVL_INFORMATION);
				Serial.print(F("Tiller ")); Serial.print((int) tillerId, 10);
				Serial.print(F(": Actual Height=")); Serial.print((int) tillers[tillerId].getActualHeight(), 10);
				Serial.print(F(", Target Height="));
				if (tillers[tillerId].getTargetHeight() == agbot::Tiller::STOP) {
					Serial.print(F("<stop>"));
				}
				else {
					Serial.print((int) tillers[tillerId].getTargetHeight(), 10);
				}
				Serial.print(F(", Status="));
				switch (tillers[tillerId].getDH()) {
					case -1:
						Serial.print(F(", Status=LOWERING\n"));
						break;
					case 1:
						Serial.print(F(", Status=RAISING\n"));
						break;
					case 0:
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
	uint16_t newValue;
	if (parseNum(message + 4, &newValue, 0xFFFFU) <= 0) { return false; }
	switch (message[2]) {
		case 'P': // precision
		case 'p':
			config.set(agbot::Setting::Precision, newValue);
			printStartMessage(MSG_LVL_INFORMATION);
			Serial.print(F("Set precision to ")); Serial.print(newValue); Serial.print('\n');
			return true;
		case 'D': // response delay
		case 'd':
			config.set(agbot::Setting::ResponseDelay, newValue);
			printStartMessage(MSG_LVL_INFORMATION);
			Serial.print(F("Set response delay to ")); Serial.print(newValue); Serial.print('\n');
			return true;
		case 'A': // tiller accuracy
		case 'a':
			if (newValue > 100) {
				return false;
			}
			else {
				config.set(agbot::Setting::TillerAccuracy, newValue);
				printStartMessage(MSG_LVL_INFORMATION);
				Serial.print(F("Set tiller accuracy to ")); Serial.print(newValue); Serial.print('\n');
				return true;
			}
		case 'R': // tiller raise time
		case 'r':
			config.set(agbot::Setting::TillerRaiseTime, newValue);
			printStartMessage(MSG_LVL_INFORMATION);
			Serial.print(F("Set tiller raise time to ")); Serial.print(newValue); Serial.print('\n');
			return true;
		case 'L': // tiller lower time
		case 'l':
			config.set(agbot::Setting::TillerLowerTime, newValue);
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
	if (currentMode != agbot::MachineMode::Process) {
		return false;
	}
	unsigned long code;
	if (parseDigits(message + 2, &code, 0xFFFFFLU, 16) == 5) {
		uint8_t tillerCode = ((code & 0x70000) ? 1 : 0) | ((code & 0x700) ? 2 : 0) | ((code & 0x7) ? 4 : 0);
		for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
			if (tillerCode & (1<<i)) {
				tillers[i].scheduleLower();
			}
		}
		// NOTE: the following mapping assumes that sprayer 0 sprays foxtail, sprayer 1 sprays cocklebur, sprayer 2 sprays ragweed,
		// and sprayer 3 sprays fertilizer in order to be compliant with the API specification for the bit order. If necessary,
		// a different mapping could technically be configured in firmware, and even set on-the-fly via a saved setting. However,
		// it's probably best to leave the mapping as it is and allow the software to map which weed corresponds to which bit.
		uint8_t sprayerCode = ((code & 0xF000) >> 12) | (code & 0xF0);
		for (uint8_t i = 1; i < agbot::Sprayer::COUNT; i++) {
			if (sprayerCode & (1<<i)) {
				sprayers[i].scheduleSpray();
			}
		}
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
	
	estop.update();
	bool hitchNeedsUpdate = hitch.needsUpdate();
	if (hitchNeedsUpdate || hitch.getDH()) {
		for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) { tillers[i].stop(true); }
		for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) { sprayers[i].stop(true); }
		if (hitchNeedsUpdate) { hitch.update(); }
	}
	if (!hitch.getDH()) {
		for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) { tillers[i].update(); }
		for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) { sprayers[i].update(); }
	}
}