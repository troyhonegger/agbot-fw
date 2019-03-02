/*
 * SerialApi.h
 * Defines a set of functions for interfacing with the Serial API to communicate with the computer.
 * Most of these functions are concerned with the state of the serial buffer, but there are some
 * helper functions (for example, for parsing integers) as well. Note that this header does NOT
 * define functions to directly process messages - this was done to keep the message processing
 * code closer to the main loop function.
 * Created: 11/25/2018 10:59:16 PM
 *  Author: troy.honegger
 */ 


#ifndef SERIALAPI_H_
#define SERIALAPI_H_

#include <Arduino.h>

// General paradigm for error handling: Messages not starting with the '^' character, or overflowing
// the maximum message size, or not ending with a '\n' character will be ignored. See the readSerial()
// implementation and comments in SerialApi.cpp for details

#define MESSAGE_BUFFER_SIZE (8)
#define MAX_MESSAGE_SIZE (16)

#define BAUD_RATE (9600)

#define MESSAGE_START ('^')
#define MESSAGE_END ('\n')

enum msgCommandType {
	MSG_CMD_UNRECOGNIZED,
	MSG_CMD_RESET,
	MSG_CMD_SET_MODE,
	MSG_CMD_GET_STATE,
	MSG_CMD_SET_CONFIG,
	MSG_CMD_DIAG,
	MSG_CMD_PROCESS
};

enum msgLevel {
	// we probably won't even send anything of this level - if it's this insignificant, it's not worth the clock cycles to send it
	MSG_LVL_VERBOSE,
	// slightly more important events - still probably not significant enough to be worth the effort of sending
	MSG_LVL_DEBUG,
	// high-level actions (i.e. "entering diagnostics mode") and responses to MSG_GET_STATE requests
	MSG_LVL_INFORMATION,
	// exceptional but not irrecoverable errors (for example, "received unrecognized command")
	MSG_LVL_WARNING,
	// an e-stop was pressed, the world is ending, or something else of similar or greater importance
	MSG_LVL_ERROR
};

void initSerialApi(void);

enum msgCommandType getMessageType(char *message);

size_t printStartMessage(enum msgLevel level);
size_t printMessage(enum msgLevel level, const char *msg);
size_t printMessage(enum msgLevel level, const __FlashStringHelper *msg);

// Returns true if a serial message is available.
bool serialMessageAvailable(void);

// Updates the serial buffer with data read from the serial port. Note that once the buffer
// reaches its maximum size, it will wrap around and start overwriting already-received messages.
void readSerial(void);

// Returns a pointer to the next buffered message, or NULL if no message is available.
// Note that once this pointer is returned, it the serial processor releases the position
// to allow it to be overwritten in the future.
// Note that this message will eventually be overwritten as new messages take its place.
// To avoid this, call getSerialMessage() and immediately process the message - do not call
// readSerial() in between.
char *getSerialMessage(void);

#define PARSE_NUM_ERROR (-1)

// Parses str as a number of the specified type, in the specified radix (note that the parser is case-
// insensitive and therefore radixes must be between 1 and 36), and stores the result in value. Stops at
// the first invalid character and returns the number of characters read (if successful), 0 (if no
// characters were read), or -1 (in the event of an arithmetic overflow). If at any point value comes
// to exceed maxValue, or if parsing the next character of the string decreases rather than increases
// the value of the result, an overflow is considered to have occurred and -1 should be returned.
// Consequently, this function does NOT support negative numbers.
template <class number>
int8_t parseDigits(const char *str, number *value, number maxValue, uint8_t radix) {
	if (radix == 0 || radix > 36) return false;
	*value = 0;
	int8_t numCharsRead = 0;
	for (; *str != '\0'; str++) {
		uint8_t digit;
		if ('0' <= *str && *str <= '9') {
			digit = *str - '0';
		}
		else if ('a' <= *str && *str <= 'z') {
			digit = *str + 10 - 'a';
		}
		else if ('A' <= *str && *str <= 'Z') {
			digit = *str + 10 - 'A';
		}
		else { // non-alphanumeric character
			break;
		}
		if (digit >= radix) { // character too great for radix
			break;
		}
		number newValue = *value * radix + digit;
		if (newValue < *value || newValue > maxValue) { // overflow error
			return PARSE_NUM_ERROR;
		}
		else {
			*value = newValue;
			numCharsRead++;
		}
	}
	return numCharsRead;
}

// Parses str as a number of the specified type and stores the result in value. Stops at the first
// invalid character and returns the number of characters read (if successful), 0 (if no characters
// were read), or -1 (in the event of an arithmetic overflow). If at any point value comes to exceed
// maxValue, or if parsing the next character of the string decreases rather than increases the value
// of the result, an overflow is considered to have occurred and -1 should be returned. Consequently,
// this function does NOT support negative numbers. This function should assume that str is a decimal
// string unless it starts with 0x, in which case it will be parsed as hexadecimal
template <class number>
int8_t parseNum(const char *str, number *value, number maxValue) {
	if (str[0] == '0' && str[1] == 'x') {
		return parseDigits(str + 2, value, maxValue, 16);
	}
	else {
		return parseDigits(str, value, maxValue, 10);
	}
}

#endif /* SERIALAPI_H_ */