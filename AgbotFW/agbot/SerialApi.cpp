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

char messageBuffer[MESSAGE_BUFFER_SIZE][MAX_MESSAGE_SIZE + 1] = { 0 };

static uint8_t messageReadIndex = 0;
static uint8_t messageReadPosn = 0;
static uint8_t messageProcessIndex = 0;
// TODO: it'd maybe be nice to build in some better handling for buffer overflows. Maybe set a global flag and add
// a function to the header to return its value? Then the main loop could poll for errors. Alternatively, an event
// handler could be registered that would, say, print a message to the serial port every time an overflow occurs
static bool blockNewMessages = false;

// Private helper function that sets every byte in messageBuffer[index] to '\0'
static inline void clearMessage(uint8_t index) {
	for (uint8_t i = 0; i < MAX_MESSAGE_SIZE + 1; i++) {
		messageBuffer[index][i] = '\0';
	}
}

void initSerialApi(void) {
	for (uint8_t i = 0; i < MESSAGE_BUFFER_SIZE; i++) {
		clearMessage(i);
	}
	messageReadIndex = 0;
	messageReadPosn = 0;
	messageProcessIndex = 0;
	Serial.begin(BAUD_RATE);
}

enum msgCommandType getMessageType(char *message) {
	if (*message == MESSAGE_START) {
		switch (message[1]) {
			case 'R':
			case 'r':
				return MSG_CMD_RESET;
			case 'M':
			case 'm':
				return MSG_CMD_SET_MODE;
			case 'G':
			case 'g':
				return MSG_CMD_GET_STATE;
			case 'C':
			case 'c':
				return MSG_CMD_SET_CONFIG;
			case 'D':
			case 'd':
				return MSG_CMD_DIAG;
			case 'P':
			case 'p':
				return MSG_CMD_PROCESS;
		}
	}
	return MSG_CMD_UNRECOGNIZED;
};

size_t printStartMessage(enum msgLevel level) {
	size_t millisNumPrinted = Serial.print(millis());
	switch (level) {
		case MSG_LVL_VERBOSE:
			return millisNumPrinted + Serial.print(F(" [VERBOSE] "));
		case MSG_LVL_DEBUG:
			return millisNumPrinted + Serial.print(F(" [DEBUG] "));
		case MSG_LVL_INFORMATION:
			return millisNumPrinted + Serial.print(F(" [INFORMATION] "));
		case MSG_LVL_WARNING:
			return millisNumPrinted + Serial.print(F(" [WARNING] "));
		case MSG_LVL_ERROR:
			return millisNumPrinted + Serial.print(F(" [ERROR] "));
		default:
			return millisNumPrinted + Serial.print(F(" [UNKNOWN] "));
	}
}
size_t printMessage(enum msgLevel level, const char *msg) {
	size_t numPrinted = printStartMessage(level);
	numPrinted += Serial.print(msg);
	return numPrinted + Serial.print('\n');
}
size_t printMessage(enum msgLevel level, const __FlashStringHelper *msg) {
	size_t numPrinted = printStartMessage(level);
	numPrinted += Serial.print(msg);
	return numPrinted + Serial.print('\n');
}

// Reads character(s) from the ArduinoCore serial buffer into a message buffer for processing.
// Overflow flag ON - ignore all messages until it's cleared.
// Posn >= MAX_MESSAGE_SIZE: clear message buffer, reset posn = 0
// Posn zero, non-^ character: ignore
// Posn zero, ^ character: insert and increment
// Posn nonzero, ^ character: clear buffer, insert ^, and increment
// Posn nonzero, \n character: insert and go to next message (check for setting blockNewMessages flag)
// Posn nonzero, other character: insert and increment
void readSerial(void) {
	while (Serial.available()) {
		if (blockNewMessages) break;
		int i = Serial.read();
		if (i == -1) break;
		char c = (char) i;
		if (messageReadPosn >= MAX_MESSAGE_SIZE) {
			// message overflowed the buffer; discard the message and process the next character as normal
			clearMessage(messageReadIndex);
			messageReadPosn = 0;
		}
		if (messageReadPosn == 0) { // start of message - must be MESSAGE_START
			if (c == MESSAGE_START) {
				messageBuffer[messageReadIndex][messageReadPosn++] = c;
			}
			else {} // ignore all other characters until MESSAGE_START
		}
		else {
			switch (c) {
				// unexpected MESSAGE_START character, probably due to overflow of the ArduinoCore
				// buffer. Discard the message and start a new one in its place
				case MESSAGE_START:
					clearMessage(messageReadIndex);
					messageReadPosn = 0;
					messageBuffer[messageReadIndex][messageReadPosn++] = c;
					break;
				// end of message - move on to the next one
				case MESSAGE_END:
					messageBuffer[messageReadIndex][messageReadPosn++] = c;
					messageReadIndex = (messageReadIndex + 1) % MESSAGE_BUFFER_SIZE;
					if (messageReadIndex == messageProcessIndex) {
						// overflow flag ON - no room for next message
						blockNewMessages = true;
					}
					messageReadPosn = 0;
					break;
				// normal character
				default:
					messageBuffer[messageReadIndex][messageReadPosn++] = c;
					break;
			}
		}
	}
}

bool serialMessageAvailable(void) {
	return blockNewMessages || messageReadIndex != messageProcessIndex;
}

char *getSerialMessage(void) {
	if (serialMessageAvailable()) {
		char *returnValue = &messageBuffer[messageProcessIndex][0];
		messageProcessIndex = (messageProcessIndex + 1) % MESSAGE_BUFFER_SIZE;
		blockNewMessages = false;
		return returnValue;
	}
	else {
		return NULL;
	}
}

template <class number>
// Parses str as a number of the specified type and stores the result in value. Stops at the first
// invalid character and returns the number of characters read (if successful), 0 (if no characters
// were read), or -1 (in the event of an arithmetic overflow). If at any point value comes to exceed
// maxValue, or if parsing the next character of the string decreases rather than increases the value
// of the result, an overflow is considered to have occurred and -1 is returned. Consequently,
// this function does NOT support negative numbers. This function should assume that str is a decimal
// string unless it starts with 0x, in which case it will be parsed as hexadecimal
int8_t parseNum(const char *str, number *value, number maxValue) {
	if (str[0] == '0' && str[1] == 'x') {
		return parseDigits(str + 2, value, maxValue, 16);
	}
	else {
		return parseDigits(str, value, maxValue, 10);
	}
}

// Parses str as a number of the specified type, in the specified radix (note that the parser is case-
// insensitive and therefore radixes must be between 1 and 36), and stores the result in value. Stops at
// the first invalid character and returns the number of characters read (if successful), 0 (if no
// characters were read), or -1 (in the event of an arithmetic overflow). If at any point value comes
// to exceed maxValue, or if parsing the next character of the string decreases rather than increases
// the value of the result, an overflow is considered to have occurred and -1 is returned.
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