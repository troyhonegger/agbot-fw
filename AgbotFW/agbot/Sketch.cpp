#include <Arduino.h>

#include "Config.hpp"
#include "EthernetApi.hpp"
#include "Estop.hpp"
#include "Hitch.hpp"
#include "Tiller.hpp"
#include "Sprayer.hpp"

#include <string.h>

using namespace agbot;

static const char KEEPALIVE_RESPONSE[] PROGMEM = "KeepAlive Acknowledge";
static const char ESTOP_ENGAGED_RESPONSE[] PROGMEM = "Estop Engaged";
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

bool messageProcessor(EthernetApi::Command const& command, char* messageText) {
	switch (command.type) {
		case EthernetApi::CommandType::Estop: {
			estop.engage();
			strncpy_P(messageText, ESTOP_ENGAGED_RESPONSE, EthernetApi::MAX_MESSAGE_SIZE);
			return true;
		} break;
		case EthernetApi::CommandType::KeepAlive: {
			strncpy_P(messageText, KEEPALIVE_RESPONSE, EthernetApi::MAX_MESSAGE_SIZE);
			lastKeepAliveTime = millis();
			return true;
		} break;
		case EthernetApi::CommandType::SetMode: {
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
				strncpy_P(messageText, ERR_NOT_IN_PROCESS_MODE, EthernetApi::MAX_MESSAGE_SIZE);
				return true;
			}
			else {
				// Data corresponding to tillers is in least-significant nibble of each byte
				for (int i = 0; i < 3; i++) {
					if (command.data.process[i] & 0x07) { tillers[i].scheduleLower(); }
				}
				// line up the weed/corn data from the sprayers into a bitfield where each bit lines
				// up with the corresponding sprayer. Then check each bit and schedule as needed.
				uint8_t sprayerData = (command.data.process[0] >> 4) | command.data.process[2];
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
				snprintf_P(messageText, EthernetApi::MAX_MESSAGE_SIZE, ERR_OUT_OF_RANGE_FMT_STR, 0, 100);
				return true;
			}
			else {
				config.set(setting, value);
			}
		} break;
		case EthernetApi::CommandType::GetState: {
			switch (command.data.query.type) {
				case EthernetApi::QueryType::Mode:
					strncpy_P(messageText, currentMode == MachineMode::Process ? MODE_PROCESS : currentMode == MachineMode::Diag ? MODE_DIAG : MODE_UNSET,
						EthernetApi::MAX_MESSAGE_SIZE);
					break;
				case EthernetApi::QueryType::Configuration:
					snprintf_P(messageText, EthernetApi::MAX_MESSAGE_SIZE, "%ud", config.get(static_cast<Setting>(command.data.query.value)));
					break;
				case EthernetApi::QueryType::Tiller:
					if (command.data.query.value >= Tiller::COUNT) {
						snprintf(messageText, EthernetApi::MAX_MESSAGE_SIZE, ERR_OUT_OF_RANGE_FMT_STR, 0, Tiller::COUNT - 1);
					}
					else { tillers[command.data.query.value].serialize(messageText, EthernetApi::MAX_MESSAGE_SIZE); }
					break;
				case EthernetApi::QueryType::Sprayer:
					if (command.data.query.value >= Sprayer::COUNT) {
						snprintf(messageText, EthernetApi::MAX_MESSAGE_SIZE, ERR_OUT_OF_RANGE_FMT_STR, 0, Sprayer::COUNT - 1);
					}
					else { sprayers[command.data.query.value].serialize(messageText, EthernetApi::MAX_MESSAGE_SIZE); }
					break;
				case EthernetApi::QueryType::Hitch:
					hitch.serialize(messageText, EthernetApi::MAX_MESSAGE_SIZE);
					break;
			}
			return true;
		} break;
		case EthernetApi::CommandType::DiagSet: {
			if (currentMode != MachineMode::Diag) {
				strncpy_P(messageText, ERR_NOT_IN_DIAG_MODE, EthernetApi::MAX_MESSAGE_SIZE);
				return true;
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
						snprintf_P(messageText, EthernetApi::MAX_MESSAGE_SIZE, ERR_OUT_OF_RANGE_FMT_STR, 0, Tiller::MAX_HEIGHT);
						return true;
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
						snprintf_P(messageText, EthernetApi::MAX_MESSAGE_SIZE, ERR_OUT_OF_RANGE_FMT_STR, 0, Hitch::MAX_HEIGHT);
						return true;
					}
					else {
						hitch.setTargetHeight(command.data.diag.value);
					}
				} break;
			}
		} break;
		case EthernetApi::CommandType::ProcessLowerHitch: {
			if (currentMode != MachineMode::Process) {
				strncpy_P(messageText, ERR_NOT_IN_PROCESS_MODE, EthernetApi::MAX_MESSAGE_SIZE);
				return true;
			}
			else {
				hitch.lower();
			}
		} break;
		case EthernetApi::CommandType::ProcessRaiseHitch: {
			if (currentMode != MachineMode::Process) {
				strncpy_P(messageText, ERR_NOT_IN_PROCESS_MODE, EthernetApi::MAX_MESSAGE_SIZE);
				return true;
			}
			else {
				hitch.raise();
			}
		} break;
	}
	return false;
}

void loop() {
	while (EthernetApi::read(messageProcessor) != EthernetApi::ReadStatus::NoMessage);
	
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