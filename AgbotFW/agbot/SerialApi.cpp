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

#ifdef OLD_SERIAL_API
// This string is guaranteed to be null-terminated at the end of the last message and at the end of the buffer,
// but every char after the last message may not necessarily be set to '\0'.
char messageBuffer2[MAX_MESSAGE_SIZE + 1];

// This flag is used internally by this module to reflect the state of the buffer. It is updated after every
// read and update operation.
static uint8_t messageBufferState;

// This flag is used internally by this module to reflect the index of the first empty character in the buffer.
// It is updated after every read and update operation.
static uint8_t messageBufferPos;

// Initializes the serial port to begin sending and receiving messages.
void initSerialApi(void) {
	Serial.begin(9600);
	messageBuffer2[0] = '\0';
	messageBuffer2[MAX_MESSAGE_SIZE] = '\0';
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
		
		messageBuffer2[messageBufferPos + numRead] = (char) c;
		messageBuffer2[messageBufferPos + numRead + 1] = '\0';
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
#endif

template <class number>
// Attempts to parse str as a number of the specified type. If successful, stores the result in value
// and returns true. If unsuccessful, returns false (and the value of value is undefined). Note that the
// parser should include "overflow detection" - if at any point value comes to exceed maxValue, or
// if parsing the next character of the string decreases rather than increases the value of the result,
// an overflow is considered to have occurred and false should be returned. Consequently, this function
// does NOT support negative numbers. This function should assume that str is a decimal string unless
// it starts with 0x, in which case it will be parsed as hexadecimal
bool tryParseNum(const char *str, number *value, number maxValue) {
	if (str[0] == '0' && str[1] == 'x') {
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