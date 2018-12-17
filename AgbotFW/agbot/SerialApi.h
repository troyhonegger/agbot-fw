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

#define MESSAGE_START ('^')
#define MESSAGE_END ('\n')

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

#ifdef OLD_SERIAL_API
// The maximum length of a message (note that the message buffer is one character larger than this to
// allow space for a null terminator).
#define MAX_MESSAGE_SIZE (64)

// The message buffer does not yet have a complete message.
#define MESSAGE_INCOMPLETE (0)
// The message buffer contains invalid data (for example, it does not start with a caret, it contains a
// caret in the body, or the message overflows the buffer). The buffer should be cleared (and a warning
// logged) when this is the state.
#define MESSAGE_INVALID (1)
// A valid message is ready and should be processed and cleared from the buffer to allow for more messages.
#define MESSAGE_READY (2)

// This string is guaranteed to be null-terminated at the end of the last message and at the end of the buffer,
// but every char after the last message may not necessarily be set to '\0'.
extern char messageBuffer2[MAX_MESSAGE_SIZE + 1];

// Initializes the serial port to begin sending and receiving messages.
void initSerialApi(void);

// Updates the message buffer with any data from the serial port and returns the buffer's state.
uint8_t updateMessageBuffer(void);

// Returns the buffer's state - one of MESSAGE_INCOMPLETE, MESSAGE_INVALID, or MESSAGE_READY
uint8_t getMessageState(void);

// Clears the current message from the buffer, until the end of the buffer is reached or a newline
// character '\n' is encountered, and returns the buffer's new state. It is expected that the buffer
// is in the MESSAGE_READY or MESSAGE_INVALID state at the time - if not, this function will clear the
// entire buffer. Note that this function clears only a single message, not necessarily the entire
// buffer, so to clear the entire buffer it may be necessary to call this method more than once.
uint8_t clearMessageFromBuffer(void);
#endif


template <class number>
// Attempts to parse str as a number of the specified type. If successful, stores the result in value
// and returns true. If unsuccessful, returns false (and the value of value is undefined). Note that the
// parser should include "overflow detection" - if at any point value comes to exceed maxValue, or
// if parsing the next character of the string decreases rather than increases the value of the result,
// an overflow is considered to have occurred and false should be returned. Consequently, this function
// does NOT support negative numbers. This function should assume that str is a decimal string unless
// it starts with 0x, in which case it will be parsed as hexadecimal
bool tryParseNum(const char *str, number *value, number maxValue);

// Attempts to parse str as a number of the specified type, in the specified radix (note that the parser
// is case-insensitive and therefore radixes are limited to between 1 and 36. If successful, stores the
// result in value and returns true. If unsuccessful, returns false (and the value of value is undefined).
// Note that the parser should include "overflow detection" - if at any point value comes to exceed maxValue,
// or if parsing the next character of the string decreases rather than increases the value of the result,
// an overflow is considered to have occurred and false should be returned. Consequently, this function
// does NOT support negative numbers.
template <class number>
bool tryParseDigits(const char *str, number *value, number maxValue, uint8_t radix);

#endif /* SERIALAPI_H_ */