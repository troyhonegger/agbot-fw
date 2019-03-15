/*
 * EthernetApi.cpp
 * Implements the functions defined in EthernetApi.hpp for interacting with the Ethernet shield.
 * See EthernetApi.hpp for more info.
 * Created: 3/13/2019 5:13:49 PM
 *  Author: troy.honegger
 */ 

#include "EthernetApi.hpp"

#include <Arduino.h>
#include <string.h>
// TODO: this is ugly, but it's the only way Atmel studio currently recognizes the header. How to add Ethernet.h to the include path?
#include "../ArduinoCore/include/libraries/Ethernet/src/Ethernet.h"

#include "Common.hpp"
#include "Sprayer.hpp"
#include "Tiller.hpp"
#include "Hitch.hpp"

// Preprocessor magic, to automate defining the string constants. The downside of this approach is that when the API
// terminology changes, so do the variable names. The upside is that the variable names always match the API terminology ;)
// MAKE_CONST_STR_WITH_LEN(Text); expands to
//	static const char Text_STR[] PROGMEM = "Text";
// MAKE_CONST_STR_WITH_LEN(Text); expands to
//	static const char Text_STR[] PROGMEM = "Text"; static uint8_t Text_STR_LEN = strlen_P(Text_STR);
// The only requirement is that the argument must be a valid C/C++ variable name
#define MAKE_CONST_STR(token) static const char token##_STR[] PROGMEM = #token
#define MAKE_CONST_STR_WITH_LEN(token) static const char token##_STR[] PROGMEM = #token; static const uint8_t token##_STR_LEN = strlen_P(token##_STR)

// Since the API only speaks when spoken to, we can get away with one message buffer
// for both incoming and outgoing messages. Messages are read into here and parsed;
// if a response is merited, it is copied back into the message buffer and sent
static char messageBuffer[agbot::EthernetApi::MAX_MESSAGE_SIZE] = { 0 };
static EthernetServer server(8010); // use port 8010

// Constants used by message parsing code
static const char SetMode_FMT_STR[] PROGMEM = "SetMode %10s";
static const char GetState_FMT_STR[] PROGMEM = "GetState %13[^[ \t\f\r\n\v]%n";
static const char GetState_Configuration_FMT_STR[] PROGMEM = "GetState Configuration[%19s]";
static const char GetState_Sprayer_FMT_STR[] PROGMEM = "GetState Sprayer[%1hhx]";
static const char GetState_Tiller_FMT_STR[] PROGMEM = "GetState Tiller[%1hhx]";
static const char SetConfig_FMT_STR[] PROGMEM = "SetConfig %19[^=]=%ud";
static const char DiagSet_FMT_STR[] PROGMEM = "DiagSet %11[^=]=%7s";
static const char DiagSet_Tiller_FMT_STR[] PROGMEM = "DiagSet Tiller[%1hhx]=";
static const char DiagSet_Sprayer_FMT_STR[] PROGMEM = "DiagSet Sprayer[%2hhx]=";
static const char Process_FMT_STR[] PROGMEM = "Process #%lux";
static const char ParseByte_FMT_STR[] PROGMEM = "%3hhud";

MAKE_CONST_STR_WITH_LEN(Estop);
MAKE_CONST_STR_WITH_LEN(KeepAlive);
MAKE_CONST_STR_WITH_LEN(ProcessRaiseHitch);
MAKE_CONST_STR_WITH_LEN(ProcessLowerHitch);
static const char GetState_Hitch_STR[] PROGMEM = "GetState Hitch";
static uint8_t GetState_Hitch_STR_LEN = strlen_P(GetState_Hitch_STR);

MAKE_CONST_STR(Processing);
MAKE_CONST_STR(Diagnostics);

MAKE_CONST_STR(Precision);
MAKE_CONST_STR(ResponseDelay);
MAKE_CONST_STR(KeepAliveTimeout);
MAKE_CONST_STR(TillerRaiseTime);
MAKE_CONST_STR(TillerLowerTime);
MAKE_CONST_STR(TillerRaisedHeight);
MAKE_CONST_STR(TillerLoweredHeight);
MAKE_CONST_STR(TillerAccuracy);
MAKE_CONST_STR(HitchRaisedHeight);
MAKE_CONST_STR(HitchLoweredHeight);
MAKE_CONST_STR(HitchAccuracy);

MAKE_CONST_STR(Hitch);
MAKE_CONST_STR_WITH_LEN(ON);
MAKE_CONST_STR_WITH_LEN(OFF);
MAKE_CONST_STR_WITH_LEN(STOP);
MAKE_CONST_STR(ACK);

static const char ParseError_UnrecognizedCommand[] PROGMEM = "PARSE ERROR: Command did not match any of the expected formats.";
static const char ParseError_InvalidMachineMode[] PROGMEM = "PARSE ERROR: Invalid machine mode: ";
static const char ParseError_UnrecognizedSetting[] PROGMEM = "PARSE ERROR: Invalid config setting: ";
static const char ParseError_UnrecognizedQueryType[] PROGMEM = "PARSE ERROR: Invalid query type: ";
static const char ParseError_UnrecognizedDiagCommand[] PROGMEM = "PARSE ERROR: Invalid diag command: Should look like 'DiagSet Sprayer[##]/Tiller[#]/Hitch={value}'";
static const char ParseError_TillerDiag[] PROGMEM = "PARSE ERROR: Tiller value must be 'STOP' or an integer";
static const char ParseError_SprayerDiag[] PROGMEM = "PARSE ERROR: Sprayer value must be 'ON' or 'OFF'";
static const char ParseError_HitchDiag[] PROGMEM = "PARSE ERROR: Hitch value must be 'STOP' or an integer";

namespace agbot{
namespace EthernetApi {
	void begin() {
		uint8_t mac[6] = { 0, 0, 0, 0, 0, 0 }; // TODO: fill this in
		uint8_t controllerIP[4] = { 10, 0, 0, 2 };
		Ethernet.begin(mac, controllerIP);
		// adjust these two settings to taste
		Ethernet.setRetransmissionCount(3);
		Ethernet.setRetransmissionTimeout(150);
		server.begin();
	}

	static bool parseSetting(Command&, char*);
	static bool parseMessage(Command&, char*);

	ReadStatus read(void (*processor)(agbot::EthernetApi::Command const&, char*)) {
		EthernetClient client = server.available();
		if (!client) { return ReadStatus::NoMessage; }
		client.read((uint8_t*) messageBuffer, MAX_MESSAGE_SIZE - 1); // don't forget to leave the last byte for a null terminator!

		Command command;
		ReadStatus status;

		if (parseMessage(command, messageBuffer)) {
			messageBuffer[0] = '\0';

			processor(command, messageBuffer);

			if (messageBuffer[0]) { status = ReadStatus::ValidMessage_Response; }
			else {
				strcpy_P(messageBuffer, ACK_STR);
				status = ReadStatus::ValidMessage_NoResponse;
			}
		}
		else {
			// the error message has been loaded into the buffer
			client.print(messageBuffer);
			client.flush();
			status = ReadStatus::InvalidMessage;
		}
		client.print(messageBuffer);
		client.flush();

		// clear the message buffer for next time
		for (int i = 0; i < MAX_MESSAGE_SIZE; i++) { messageBuffer[i] = '\0'; }
		return status;
	}

	static bool parseSetting(Setting& setting, char* settingStr) {
		if (!strcmp_P(settingStr, Precision_STR)) {
			setting = Setting::Precision;
			return true;
		}
		else if (!strcmp_P(settingStr, KeepAliveTimeout_STR)) {
			setting = Setting::KeepAliveTimeout;
			return true;
		}
		else if (!strcmp_P(settingStr, ResponseDelay_STR)) {
			setting = Setting::ResponseDelay;
			return true;
		}
		else if (!strcmp_P(settingStr, TillerRaiseTime_STR)) {
			setting = Setting::TillerRaiseTime;
			return true;
		}
		else if (!strcmp_P(settingStr, TillerLowerTime_STR)) {
			setting = Setting::TillerLowerTime;
			return true;
		}
		else if (!strcmp_P(settingStr, TillerAccuracy_STR)) {
			setting = Setting::TillerAccuracy;
			return true;
		}
		else if (!strcmp_P(settingStr, TillerRaisedHeight_STR)) {
			setting = Setting::TillerRaisedHeight;
			return true;
		}
		else if (!strcmp_P(settingStr, TillerLoweredHeight_STR)) {
			setting = Setting::TillerLoweredHeight;
			return true;
		}
		else if (!strcmp_P(settingStr, HitchAccuracy_STR)) {
			setting = Setting::HitchAccuracy;
			return true;
		}
		else if (!strcmp_P(settingStr, HitchRaisedHeight_STR)) {
			setting = Setting::HitchRaisedHeight;
			return true;
		}
		else if (!strcmp_P(settingStr, HitchLoweredHeight_STR)) {
			setting = Setting::HitchLoweredHeight;
			return true;
		}
		else { return false; }
	}

	static bool parseMessage(Command& command, char* message) {
		// used to hold sscanf matches
		char data[20] = {0};
		uint32_t longData = 0;
		uint16_t intData = 0;
		uint8_t byteData = 0;
		uint8_t charsRead = 0;

		if (!strncmp_P(message, Estop_STR, Estop_STR_LEN)) {
			command.type = CommandType::Estop;
			return true;
		}
		else if (!strncmp_P(message, KeepAlive_STR, KeepAlive_STR_LEN)) {
			command.type = CommandType::KeepAlive;
			return true;
		}
		else if (sscanf_P(message, Process_FMT_STR, &longData) == 1) {
			command.type = CommandType::Process;
			command.data.process[0] = static_cast<uint8_t>((longData & 0x000F0000) >> 16);
			command.data.process[1] = static_cast<uint8_t>((longData & 0x0000FF00) >> 8);
			command.data.process[2] = static_cast<uint8_t>((longData & 0x000000FF) >> 0);
			return true;
		}
		else if (sscanf_P(message, SetMode_FMT_STR, data) == 1) {
			command.type = CommandType::SetMode;
			if (!strcmp_P(data, Processing_STR)) {
				command.data.machineMode = MachineMode::Process;
				return true;
			}
			else if (!strcmp_P(data, Diagnostics_STR)) {
				command.data.machineMode = MachineMode::Diag;
				return true;
			}
			else {
				strcpy_P(messageBuffer, ParseError_InvalidMachineMode);
				strcat(messageBuffer, data);
				return false;
			}
		}
		else if (sscanf_P(message, GetState_FMT_STR, data, &charsRead) == 1) {
			command.type = CommandType::GetState;
			if (sscanf_P(message, GetState_Configuration_FMT_STR, data) == 1) {
				command.data.query.type = QueryType::Configuration;
				Setting setting;
				if (parseSetting(setting, data)) {
					command.data.query.value = static_cast<uint8_t>(setting);
					return true;
				}
				else {
					strcpy_P(messageBuffer, ParseError_UnrecognizedSetting);
					strcat(messageBuffer, data);
					return false;
				}
			}
			else if (sscanf_P(message, GetState_Tiller_FMT_STR, &byteData) == 1) {
				command.data.query.type = QueryType::Tiller;
				command.data.query.value = byteData;
				return true;
			}
			else if (sscanf_P(message, GetState_Sprayer_FMT_STR, &byteData) == 1) {
				command.data.query.type = QueryType::Sprayer;
				command.data.query.value = byteData;
				return true;
			}
			else if (!strncmp_P(message, GetState_Hitch_STR, GetState_Hitch_STR_LEN)) {
				command.data.query.type = QueryType::Hitch;
				return true;
			}
			else {
				strcpy_P(messageBuffer, ParseError_UnrecognizedQueryType);
				strcat(messageBuffer, data);
				return false;
			}
		}
		else if (sscanf_P(message, SetConfig_FMT_STR, data, &intData) == 2) {
			command.type = CommandType::SetConfig;
			command.data.config.value = intData;
			if (parseSetting(command.data.config.setting, data)) { return true; }
			else {
				strcpy_P(messageBuffer, ParseError_UnrecognizedSetting);
				strcat(messageBuffer, data);
				return false;
			}
		}
		// Slight hack here - rather than declaring two separate arrays data1 and data2, we split data in
		// two pieces. The format string passed into sscanf ensures that the first string doesn't overwrite
		// the second, and that there is a null terminator between the two.
		else if (sscanf_P(message, DiagSet_FMT_STR, data, data + 12) == 2) {
			command.type = CommandType::DiagSet;
			if (sscanf_P(message, DiagSet_Tiller_FMT_STR, &byteData) == 1) {
				command.data.diag.type = PeripheralType::Tiller;
				command.data.diag.id = byteData;
				if (!strcmp(data + 12, STOP_STR)) {
					command.data.diag.value = agbot::Tiller::STOP;
					return true;
				}
				else if (sscanf_P(data + 12, ParseByte_FMT_STR, &byteData) == 1) {
					command.data.diag.value = byteData;
					return true;
				}
				else {
					strcpy_P(messageBuffer, ParseError_TillerDiag);
					return false;	
				}
			}
			else if (sscanf_P(message, DiagSet_Sprayer_FMT_STR, &byteData) == 1) {
				command.data.diag.type = PeripheralType::Sprayer;
				command.data.diag.id = byteData;
				if (!strcmp(data + 12, ON_STR)) {
					command.data.diag.value = Sprayer::ON;
					return true;
				}
				else if (!strcmp(data + 12, OFF_STR)) {
					command.data.diag.value = Sprayer::OFF;
					return true;
				}
				else {
					strcpy_P(messageBuffer, ParseError_SprayerDiag);
					return false;
				}
			}
			else if (!strcmp_P(data, Hitch_STR)) {
				command.data.diag.type = PeripheralType::Hitch;
				if (!strcmp, data + 12, STOP_STR) {
					command.data.diag.value = Hitch::STOP;
					return true;
				}
				else if (sscanf_P(data + 12, ParseByte_FMT_STR, &byteData) == 1) {
					command.data.diag.value = byteData;
					return true;
				}
				else {
					strcpy_P(messageBuffer, ParseError_HitchDiag);
					return false;
				}
			}
			else {
				strcpy_P(messageBuffer, ParseError_UnrecognizedDiagCommand);
				return false;
			}
		}
		else if (!strncmp_P(message, ProcessRaiseHitch_STR, ProcessRaiseHitch_STR_LEN)) {
			command.type = CommandType::ProcessRaiseHitch;
			return true;
		}
		else if (!strncmp_P(message, ProcessLowerHitch_STR, ProcessLowerHitch_STR_LEN)) {
			command.type == CommandType::ProcessLowerHitch;
			return true;
		}
		else {
			strcpy_P(messageBuffer, ParseError_UnrecognizedCommand);
			return false;
		}
	}
}
}