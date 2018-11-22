/*
 * Sprayer.h
 * This header defines a set of data and functions to be used to interact with the sprayers on the machine. It defines the sprayer
 * structure, which encapsulates the state of a single sprayer, as well as a global array containing all eight sprayers on the machine.
 * It also defines diagnostic operations (to turn sprayers on or off manually) as well as processing operations (to schedule spraying
 * as weeds are encountered).
 * Created: 11/21/2018 8:00:00 PM
 *  Author: troy.honegger
 */ 

#ifndef SPRAYER_H_
#define SPRAYER_H_

#include <Arduino.h>

#define SPRAYER_PROCESS_OFF (0)
#define SPRAYER_PROCESS_ON (1)
#define SPRAYER_PROCESS_SCHEDULED (2)
#define SPRAYER_DIAG_OFF (3)
#define SPRAYER_DIAG_ON (4)
#define SPRAYER_STATE_UNSET (5)

#define NUM_SPRAYERS (8)

#define SPRAYER_OFF (false)
#define SPRAYER_ON (true)


struct sprayer {
	uint32_t offTime; // the sprayer must be on until this time has elapsed
	uint32_t onTime; // the sprayer must be on once this time has elapsed
	uint8_t state; // state (on, off, scheduled, or diag) - should not be accessed externally; use status instead
	bool status; // on or off - can be read externally, but should only be set by the sprayer library functions
};

// Physical significance of sprayer numbering: Sprayers 0 through 3 represent the left row, and sprayer 4 through 7 represent the right row.
// Within a row, sprayers are numbered front to back - the front left sprayer is number 0, and the back right sprayer is number 7. This is
// summarized in the following diagram:
//     Kubota
//      /|\      <--trailer hitch
//    ///|\\\
//  X #0 X #4 X  <--X's represent spaces in between rows (where the tillers are)
//  X #1 X #5 X
//  X #2 X #6 X
//  X #3 X #7 X
extern struct sprayer sprayerList[NUM_SPRAYERS];


// Initializes sprayerList, configures the GPIO pins, and performs all other necessary initialization work for the sprayers.
// Note that sprayersEnter???Mode() still needs to be called before diag or process functions are called.
void initSprayers(void);

// Shuts off all sprayers and switches them to process mode.
void sprayersEnterProcessMode(void);

// Shuts off all sprayers and switches them to diag mode.
void sprayersEnterDiagMode(void);


// sprayers is a bitfield here, NOT an array index. This should only be called when in process mode.
void scheduleSpray(uint8_t sprayers);

// Performs any scheduled set sprayer operations. This should be called every loop iteration. In diag mode, it will have no effect.
void updateSprayers(void);


// sprayers is a bitfield here, NOT an array index. This should only be called when in diag mode.
void diagSetSprayer(uint8_t sprayers, bool status);

#endif /* SPRAYER_H_ */