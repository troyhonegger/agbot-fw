/*
 * ApiComs.h
 *
 * Created: 11/25/2018 10:59:16 PM
 *  Author: troy.honegger
 */ 


#ifndef APICOMS_H_
#define APICOMS_H_

#define MAX_MESSAGE_SIZE (24)

// the message buffer is empty and waiting for a message
#define MESSAGE_WAITING (0)
// the message buffer contains invalid data (for example, it does not start with a caret, or it reaches MAX_MESSAGE_SIZE
// without a newline). The buffer may be cleared (though a warning should probably be logged) when this is the state.
#define MESSAGE_INVALID (1)
// the message is valid so far but has not yet been terminated with a newline
#define MESSAGE_INCOMPLETE (2)
// the message is valid and should be processed and cleared from the buffer to allow for more messages.
#define MESSAGE_COMPLETE (3)

extern char messageBuffer[MAX_MESSAGE_SIZE];

// Initializes the serial port to begin sending and receiving messages.
void initApiComs(void);

// Updates the message buffer with any data from the serial buffer and returns the buffer's state.
uint8_t updateApiComs(void);

// Returns the current state of the message buffer.
uint8_t getMessageState(void);

// Clears the current message from the buffer, until the end of the buffer is reached or a newline
// character '\n' is encountered. Note that this may not entirely clear the buffer - subsequent
// calls may be necessary to flush all data
uint8_t clearMessageFromBuffer(void);

#endif /* APICOMS_H_ */