#ifndef SPRAYER_H
#define SPRAYER_H

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

#endif //SPRAYER_H