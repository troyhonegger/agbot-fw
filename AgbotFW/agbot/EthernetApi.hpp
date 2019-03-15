/*
 * EthernetApi.hpp
 * Encapsulates the logic necessary for sending and receiving TCP messages through the Ethernet
 * shield, according to the multivator API spec.
 * Created: 3/13/2019 4:04:40 PM
 *  Author: troy.honegger
 */ 

#pragma once

#include "Common.hpp"
#include "Config.hpp"

namespace agbot {
namespace EthernetApi {
	const uint8_t MAX_MESSAGE_SIZE = 128;

	enum class CommandType : uint8_t {
		Estop = 0,
		KeepAlive = 1,
		SetMode = 2,
		GetState = 3,
		SetConfig = 4,
		DiagSet = 5,
		Process = 6,
		ProcessRaiseHitch = 7,
		ProcessLowerHitch = 8
	};
	enum class QueryType : uint8_t {
		Mode = 0,
		Configuration = 1,
		Tiller = 2,
		Sprayer = 3,
		Hitch = 4
	};
	enum class PeripheralType : uint8_t {
		Sprayer = 0,
		Tiller = 1,
		Hitch = 2
	};

	// Represents any possible command the controller can receive over the Ethernet API, all in 4 bytes
	struct Command {
		CommandType type;
		union {
			// Estop, KeepAlive, ProcessRaiseHitch, ProcessLowerHitch: no extra data necessary
			// SetMode: MachineMode
			MachineMode machineMode;
			// GetState: type, value
			struct {
				QueryType type;
				// if type == QueryType::Configuration, value is static casted from Setting.
				// if type == QueryType::Tiller or QueryType::Sprayer, value is the ID of the tiller/sprayer
				uint8_t value;
			} query;
			// SetConfig: setting, value
			struct {
				Setting setting;
				uint16_t value;
			} config;
			// DiagSet: type, ID, value
			struct {
				PeripheralType type;
				uint8_t id;
				uint8_t value;
			} diag;
			// Process (bytes stored in big-endian form to better match the string representation, most significant nibble ignored)
			uint8_t process[3];
		} data;
	};

	// Initializes the Ethernet shield and gets the controller ready to receive messages.
	// This should be called inside the setup() function before read() is first called.
	void begin();

	enum class ReadStatus : uint8_t {
		NoMessage = 0,
		InvalidMessage = 1,
		ValidMessage_NoResponse = 2,
		ValidMessage_Response = 3
	};

	// Checks the Ethernet port for incoming messages. If it finds one, it parses it and calls processor().
	// The first argument to processor() is the parsed command. The second is the place to put the response
	// string. If the processor wants to respond to the message, it should copy it into the response buffer
	// (being sure to keep it within the MAX_MESSAGE_SIZE buffer limit). Otherwise, it should ignore the
	// response pointer, and the Ethernet API will take care of responding with "ACK". read() will then
	// take care of sending the response and return a ReadStatus describing what it did.
	ReadStatus read(void (*processor)(agbot::EthernetApi::Command const&, char*));
}
}