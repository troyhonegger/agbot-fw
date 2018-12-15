/*
 * SerialApi.cpp
 * Implements SerialApi.h with a set of functions for interfacing with the Serial API.
 * Most of these functions are concerned with the state of the serial buffer, but there are some
 * helper functions (for example, for parsing integers) as well. Note that this header does NOT
 * define functions to directly process messages - this was done to keep the message processing
 * code closer to the main loop function.
 * Created: 11/26/2018 10:25:49 PM
 *  Author: troy.honegger
 */ 

#include "SerialApi.h"

#include <Arduino.h>

// This string is guaranteed to be null-terminated at the end of the last message and at the end of the buffer,
// but every char after the last message may not necessarily be set to '\0'.
char messageBuffer[MAX_MESSAGE_SIZE + 1];

// This flag is used internally by this module to reflect the state of the buffer. It is updated after every
// read and update operation.
static uint8_t messageBufferState;

// This flag is used internally by this module to reflect the index of the first empty character in the buffer.
// It is updated after every read and update operation.
static uint8_t messageBufferPos;

// Initializes the serial port to begin sending and receiving messages.
void initSerialApi(void) {
	Serial.begin(9600);
	messageBuffer[0] = '\0';
	messageBuffer[MAX_MESSAGE_SIZE] = '\0';
	messageBufferPos = 0;
	messageBufferState = MESSAGE_INCOMPLETE;
}

// Updates the message buffer with any data from the serial port and returns the buffer's state.
uint8_t updateMessageBuffer(void) {
	uint8_t numRead = 0;
	while (Serial.available()) {
		if (messageBufferPos + numRead == MAX_MESSAGE_SIZE) { // always check for buffer overflow
			messageBufferState = MESSAGE_INVALID;
			break;
		}

		int c = Serial.read();
		if (c == -1 || c == '\0') {
			continue;
		}
		numRead++;
		
		messageBuffer[messageBufferPos + numRead] = (char) c;
		messageBuffer[messageBufferPos + numRead + 1] = '\0';
		if (messageBufferState == MESSAGE_INCOMPLETE) {
			if ((messageBufferPos + numRead != 1) && (c == '^')) {
				messageBufferState = MESSAGE_INVALID;
			}
			else if (c == '\n') {
				messageBufferState = MESSAGE_READY;
			}
		}
	}
	return messageBufferState;
}

// Returns the current message's state - one of MESSAGE_INCOMPLETE, MESSAGE_INVALID, or MESSAGE_READY
uint8_t getMessageState(void) { return messageBufferState; }

// Clears the current message from the buffer, until the end of the buffer is reached or a newline
// character '\n' is encountered, and returns the buffer's new state. It is expected that the buffer
// is in the MESSAGE_READY or MESSAGE_INVALID state at the time - if not, this function will clear the
// entire buffer. Note that this function clears only a single message, not necessarily the entire
// buffer, so to clear the entire buffer it may be necessary to call this method more than once.
uint8_t clearMessageFromBuffer(void) {
	// TODO: Implement
	return MESSAGE_INCOMPLETE;
}

template <class number>
// Attempts to parse str as a number of the specified type. If successful, stores the result in value
// and returns true. If unsuccessful, returns false (and the value of value is undefined). Note that the
// parser should include "overflow detection" - if at any point value comes to exceed maxValue, or
// if parsing the next character of the string decreases rather than increases the value of the result,
// an overflow is considered to have occurred and false should be returned. Consequently, this function
// does NOT support negative numbers. This function should assume that str is a decimal string unless
// it starts with 0x, in which case it will be parsed as hexadecimal
bool tryParseNum(const char *str, number *value, number maxValue) {
	if (str[0] == '0' && str[2] == 'x') {
		return tryParseDigits(str + 2, value, maxValue, 16);
	}
	else {
		return tryParseDigits(str, value, maxValue, 10);
	}
}

// Attempts to parse str as a number of the specified type, in the specified radix (note that the parser
// is case-insensitive and therefore radixes are limited to between 1 and 36. If successful, stores the
// result in value and returns true. If unsuccessful, returns false (and the value of value is undefined).
// Note that the parser should include "overflow detection" - if at any point value comes to exceed maxValue,
// or if parsing the next character of the string decreases rather than increases the value of the result,
// an overflow is considered to have occurred and false should be returned. Consequently, this function
// does NOT support negative numbers.
template <class number>
bool tryParseDigits(const char *str, number *value, number maxValue, uint8_t radix) {
	if (radix == 0 || radix > 36) return false;
	*value = 0;
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
		else { // invalid character
			return false;
		}
		if (digit >= radix) { // invalid character
			return false;
		}
		number newValue = *value * radix + digit;
		if (newValue < *value || newValue > maxValue) { // overflow error
			return false;
		}
		else {
			*value = newValue;
		}
	}
	return true;
}