#include <Arduino.h>

#include "Config.hpp"
#include "EthernetApi.hpp"
#include "Estop.hpp"
#include "Hitch.hpp"
#include "Tiller.hpp"
#include "Sprayer.hpp"

#include <string.h>

using namespace agbot;

static const char ERR_NOT_IN_PROCESS_MODE[] PROGMEM = "ERROR: Command invalid - not in process mode";
static const char ERR_NOT_IN_DIAG_MODE[] PROGMEM = "ERROR: Command invalid - not in diag mode";

static const char MODE_PROCESS[] PROGMEM = "Processing";
static const char MODE_DIAG[] PROGMEM = "Diagnostics";
static const char MODE_UNSET[] PROGMEM = "Unset";

static const char ERR_OUT_OF_RANGE_FMT_STR[] PROGMEM = "ERROR: Value must be between %d and %d";

Config config;
Estop estop;
Hitch hitch;
Tiller tillers[Tiller::COUNT];
Sprayer sprayers[Sprayer::COUNT];
MachineMode currentMode = MachineMode::Unset;
unsigned long lastKeepAliveTime;

#ifndef DEMO_MODE

/*
 * TODO: before running this sketch, verify the following constants are set to the correct values:
 * 1. In EthernetApi::begin - set MAC address of Ethernet shield
 *		If the shield doesn't have a MAC address, the current value (12:34:56:78:9A:BC) should work, but remember not to re-use the same address!
 * 2. In Tiller::getHeightSensorPin(), Tiller::getRaisePin(), Tiller::getLowerPin() - map the pins by sprayer ID
 * 3. In Sprayer::getPin() - map the pins by sprayer ID
 * 4. Hitch::RAISE_PIN, Hitch::LOWER_PIN, and Hitch::HEIGHT_SENSOR_PIN - map the pins
 * 5. Tiller::OFF_VOLTAGE and Tiller::ON_VOLTAGE - set tillers active high or active low
 * 6. Sprayer::OFF_VOLTAGE and Sprayer::ON_VOLTAGE - set sprayers active high or active low
 * 7. Hitch::OFF_VOLTAGE and Hitch::ON_VOLTAGE - set hitch active high or active low
 * 8. Estop::HW_PIN (and possibly Estop::PULSE_LEN) - set to the correct value(s)
 * 9. EthernetApi::MAX_CLIENTS - currently set to 8 in expectation of a W5500 board. if we use a W5100 board, it should be 4
 *		Leaving it at 8 will waste a lot of memory but shouldn't hurt anything
*/

void setup() {
	config.begin();
	estop.begin();
	hitch.begin(&config);
	for (uint8_t i = 0; i < Tiller::COUNT; i++) {
		tillers[i].begin(i, &config);
	}
	for (uint8_t i = 0; i < Sprayer::COUNT; i++) {
		sprayers[i].begin(i, &config);
	}
	EthernetApi::begin();
	lastKeepAliveTime = millis();
}

void messageProcessor(EthernetApi::Command const& command, char* response) {
	lastKeepAliveTime = millis();
	switch (command.type) {
		case EthernetApi::CommandType::Estop: { estop.engage(); } break;
		case EthernetApi::CommandType::KeepAlive: { /* Do nothing */ } break;
		case EthernetApi::CommandType::SetMode: {
			// we don't have to validate the mode because the parser only knows of Processing and Diagnostics modes
			currentMode = command.data.machineMode;
			for (uint8_t i = 0; i < Sprayer::COUNT; i++) {
				sprayers[i].setMode(currentMode);
			}
			for (uint8_t i = 0; i < Tiller::COUNT; i++) {
				tillers[i].setMode(currentMode);
			}
		} break;
		case EthernetApi::CommandType::Process: {
			if (currentMode != MachineMode::Process) {
				strncpy_P(response, ERR_NOT_IN_PROCESS_MODE, EthernetApi::MAX_MESSAGE_SIZE);
			}
			else {
				// Data corresponding to tillers is in least-significant nibble of each byte
				for (int i = 0; i < 3; i++) {
					if (command.data.process[i] & 0x07) { tillers[i].scheduleLower(); }
				}
				// line up the weed/corn data from the sprayers into a bitfield where each bit lines
				// up with the corresponding sprayer. Then check each bit and schedule as needed.
				uint8_t sprayerData = (command.data.process[1] >> 4) | (command.data.process[2] & 0xF0);
				for (int i = 0; i < 8; i++) {
					if (sprayerData & (1 << i)) { sprayers[i].scheduleSpray(); }
				}
			}
		} break;
		case EthernetApi::CommandType::SetConfig: {
			Setting setting = command.data.config.setting;
			uint16_t value = command.data.config.value;
			if (value > 100 && setting != Setting::Precision && setting != Setting::ResponseDelay && setting != Setting::KeepAliveTimeout
					&& setting != Setting::TillerLowerTime && setting != Setting::TillerRaiseTime) {
				snprintf_P(response, EthernetApi::MAX_MESSAGE_SIZE, ERR_OUT_OF_RANGE_FMT_STR, 0, 100);
			}
			else { config.set(setting, value); }
		} break;
		case EthernetApi::CommandType::GetState: {
			switch (command.data.query.type) {
				case EthernetApi::QueryType::Mode:
					strncpy_P(response, currentMode == MachineMode::Process ? MODE_PROCESS : currentMode == MachineMode::Diag ? MODE_DIAG : MODE_UNSET,
						EthernetApi::MAX_MESSAGE_SIZE);
					break;
				case EthernetApi::QueryType::Configuration:
					snprintf(response, EthernetApi::MAX_MESSAGE_SIZE, "%u", config.get(static_cast<Setting>(command.data.query.value)));
					break;
				case EthernetApi::QueryType::Tiller:
					if (command.data.query.value >= Tiller::COUNT) {
						snprintf_P(response, EthernetApi::MAX_MESSAGE_SIZE, ERR_OUT_OF_RANGE_FMT_STR, 0, Tiller::COUNT - 1);
					}
					else { tillers[command.data.query.value].serialize(response, EthernetApi::MAX_MESSAGE_SIZE); }
					break;
				case EthernetApi::QueryType::Sprayer:
					if (command.data.query.value >= Sprayer::COUNT) {
						snprintf_P(response, EthernetApi::MAX_MESSAGE_SIZE, ERR_OUT_OF_RANGE_FMT_STR, 0, Sprayer::COUNT - 1);
					}
					else { sprayers[command.data.query.value].serialize(response, EthernetApi::MAX_MESSAGE_SIZE); }
					break;
				case EthernetApi::QueryType::Hitch:
					hitch.serialize(response, EthernetApi::MAX_MESSAGE_SIZE);
					break;
			}
		} break;
		case EthernetApi::CommandType::DiagSet: {
			if (currentMode != MachineMode::Diag) {
				strncpy_P(response, ERR_NOT_IN_DIAG_MODE, EthernetApi::MAX_MESSAGE_SIZE);
				break;
			}
			switch (command.data.diag.type) {
				case EthernetApi::PeripheralType::Sprayer: {
					bool status = static_cast<bool>(command.data.diag.value);
					for (uint8_t i = 0; i < Sprayer::COUNT; i++) {
						if (command.data.diag.id & (1 << i)) {
							sprayers[i].setStatus(status);
						}
					}
				} break;
				case EthernetApi::PeripheralType::Tiller: {
					if (command.data.diag.value > Tiller::MAX_HEIGHT && command.data.diag.value != Tiller::STOP) {
						snprintf_P(response, EthernetApi::MAX_MESSAGE_SIZE, ERR_OUT_OF_RANGE_FMT_STR, 0, Tiller::MAX_HEIGHT);
					}
					else {
						for (uint8_t i = 0; i < Tiller::COUNT; i++) {
							if (command.data.diag.id & (1 << i)) {
								tillers[i].setTargetHeight(command.data.diag.value);
							}
						}
					}
				} break;
				case EthernetApi::PeripheralType::Hitch: {
					if (command.data.diag.value > Hitch::MAX_HEIGHT && command.data.diag.value != Hitch::STOP) {
						snprintf_P(response, EthernetApi::MAX_MESSAGE_SIZE, ERR_OUT_OF_RANGE_FMT_STR, 0, Hitch::MAX_HEIGHT);
					}
					else { hitch.setTargetHeight(command.data.diag.value); }
				} break;
			}
		} break;
		case EthernetApi::CommandType::ProcessLowerHitch: {
			if (currentMode != MachineMode::Process) {
				strncpy_P(response, ERR_NOT_IN_PROCESS_MODE, EthernetApi::MAX_MESSAGE_SIZE);
			}
			else { hitch.lower(); }
		} break;
		case EthernetApi::CommandType::ProcessRaiseHitch: {
			if (currentMode != MachineMode::Process) {
				strncpy_P(response, ERR_NOT_IN_PROCESS_MODE, EthernetApi::MAX_MESSAGE_SIZE);
			}
			else { hitch.raise(); }
		} break;
	}
}

void loop() {
	EthernetApi::read(messageProcessor);
	
	if (isElapsed(lastKeepAliveTime + config.get(Setting::KeepAliveTimeout))) {
		estop.engage();
	}

	estop.update();
	bool hitchNeedsUpdate = hitch.needsUpdate();
	if (hitchNeedsUpdate || hitch.getDH()) {
		for (uint8_t i = 0; i < Tiller::COUNT; i++) { tillers[i].stop(true); }
		for (uint8_t i = 0; i < Sprayer::COUNT; i++) { sprayers[i].stop(true); }
		if (hitchNeedsUpdate) { hitch.update(); }
	}
	if (!hitch.getDH()) {
		for (uint8_t i = 0; i < Tiller::COUNT; i++) { tillers[i].update(); }
		for (uint8_t i = 0; i < Sprayer::COUNT; i++) { sprayers[i].update(); }
	}
}

#endif // DEMO_MODE