#include <Arduino.h>

#include "Common.hpp"
#include "Devices.hpp"
#include "Http.hpp"
#include "HttpApi.hpp"

#include <string.h>

using namespace agbot;

Config agbot::config;
Hitch agbot::hitch;
Tiller agbot::tillers[Tiller::COUNT];
Sprayer agbot::sprayers[Sprayer::COUNT];
Throttle agbot::throttle;
MachineMode agbot::currentMode = MachineMode::Run;

EthernetServer ethernetSrvr(80);
HttpServer server(ethernetSrvr, 4, apiHandler);

#ifndef DEMO_MODE

void setup() {
	config.begin();
	hitch.begin(&config);
	for (uint8_t i = 0; i < Tiller::COUNT; i++) {
		tillers[i].begin(i, &config);
	}
	for (uint8_t i = 0; i < Sprayer::COUNT; i++) {
		sprayers[i].begin(i, &config);
	}
	throttle.begin();

	#ifdef SERIAL_DEBUG
	Serial.begin(9600);
	Serial.println("Serial debug mode ON");
	#endif

	uint8_t mac[6] = { 0xA8, 0x61, 0x0A, 0xAE, 0x11, 0xF6 };
	uint8_t controllerIP[4] = { 192, 168, 4, 2 };
	Ethernet.begin(mac, controllerIP);
	// adjust these two settings to taste
	Ethernet.setRetransmissionCount(3);
	Ethernet.setRetransmissionTimeout(150);
	server.begin();
}

/*

static const char ERR_NOT_IN_PROCESS_MODE[] PROGMEM = "ERROR: Command invalid - not in process mode";
static const char ERR_NOT_IN_DIAG_MODE[] PROGMEM = "ERROR: Command invalid - not in diag mode";

static const char MODE_PROCESS[] PROGMEM = "Processing";
static const char MODE_DIAG[] PROGMEM = "Diagnostics";
static const char MODE_UNSET[] PROGMEM = "Unset";

static const char ERR_OUT_OF_RANGE_FMT_STR[] PROGMEM = "ERROR: Value must be between %d and %d";

void messageProcessor(EthernetApi::Command const& command, char* response) {
	lastKeepAliveTime = millis();
	switch (command.type) {
		case EthernetApi::CommandType::Estop: { estop.engage(); } break;
		case EthernetApi::CommandType::KeepAlive: break; // do nothing
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
					if (command.data.process[i] & 0x0F) { tillers[i].scheduleLower(); }
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
}*/

void loop() {
	server.serve();
	
	//hitch.getActualHeight();
	if (hitch.needsUpdate()) { hitch.update(); }
	for (uint8_t i = 0; i < Tiller::COUNT; i++) { tillers[i].update(); }
	for (uint8_t i = 0; i < Sprayer::COUNT; i++) { sprayers[i].update(); }

	// throttle up if the hitch or any tillers are moving (but NOT if the sprayers or the
	// clutch are active - they shouldn't suck too much power)
	int8_t throttleUp = hitch.getDH();
	for (uint8_t i = 0; i < Tiller::COUNT; i++) { throttleUp |= tillers[i].getDH(); }
	if (throttleUp) { throttle.up(); }
	else { throttle.down(); }
	throttle.update();
}

#endif // DEMO_MODE
