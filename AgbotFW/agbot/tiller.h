/*
 * Tiller.h
 * This header defines a set of data and functions to be used to interact with the tillers on the machine. It defines the tiller
 * structure, which encapsulates the state of a single tiller, as well as a global array containing all three tillers on the machine.
 * It also defines diagnostic operations (to raise, lower, and stop tillers manually) as well as processing operations (to schedule
 * tiller operations as weeds are encountered).
 * Created: 11/21/2018 8:13:00 PM
 *  Author: troy.honegger
 */

#ifndef TILLER_H_
#define TILLER_H_

#include <Arduino.h>

// tiller states
#define TILLER_STATE_UNSET (0)
#define TILLER_PROCESS_RAISING (1)
#define TILLER_PROCESS_LOWERING (2)
#define TILLER_PROCESS_SCHEDULED (3)
#define TILLER_DIAG_TARGET_VALUE (4)
#define TILLER_DIAG_RAISING (5)
#define TILLER_DIAG_LOWERING (6)
#define TILLER_DIAG_STOPPED (7)

#define TILLER_DH_UP (-1)
#define TILLER_DH_STOPPED (0)
#define TILLER_DH_DOWN (1)

#define MAX_TILLER_HEIGHT (100)
#define TILLER_STOP (MAX_TILLER_HEIGHT + 1)

#define NUM_TILLERS (3)

struct tiller {
	unsigned long lowerTime; // the tiller must be lowered or lowering once this time has elapsed
	unsigned long raiseTime; // the tiller must be lowered or lowering until this time has elapsed
	uint8_t state; // state (raising, lowering, scheduled, or diag) - used only by internal functions; should not be accessed externally
	uint8_t dh; // change in height of the tiller (raising, lowering, or stopped)
	uint8_t targetHeight; // target height of the tiller
	uint8_t actualHeight; // actual height of the tiller
};

// Tillers are numbered left to right starting from zero
extern struct tiller tillerList[NUM_TILLERS];
// This flag should NEVER be set by any function except those defined in this header
extern bool estopEngaged;

//////////////////////////////////////////////////////////////////////////
// Initialization functions
//////////////////////////////////////////////////////////////////////////

// Initializes tillerList, configures the GPIO pins, and performs all other necessary initialization work for the tillers.
// Note that tillersEnter???Mode() still needs to be called before diag or process functions are called.
void initTillers(void);

// Raises all tillers and switches them to process mode. This can be used to recover from an e-stop and, as such, should ONLY be called
// upon request over the serial port.
void tillersEnterProcessMode(void);

// Stops all tillers and switches them to diag mode. This can be used to recover from an e-stop and, as such, should ONLY be called
// upon request over the serial port.
void tillersEnterDiagMode(void);

//////////////////////////////////////////////////////////////////////////
// Processing functions
//////////////////////////////////////////////////////////////////////////

// tillers is a bitfield here, NOT an array index. This should only be called when in process mode.
void scheduleTillerLower(uint8_t tillers);

// Performs any scheduled operations and updates the actualHeight and dh fields of all tillers, performing
// I/O where necessary. This should be called every loop iteration (in both diag and process mode), as neither
// diag nor processing functions will directly perform I/O - instead, they will schedule the operation(s) to
// be performed by this function.
void updateTillers(void);

//////////////////////////////////////////////////////////////////////////
// Diag functions
//////////////////////////////////////////////////////////////////////////

// tillers is a bitfield here, NOT an array index. This should only be called when in diag mode.
void diagRaiseTiller(uint8_t tillers);

// tillers is a bitfield here, NOT an array index. This should only be called when in diag mode.
void diagLowerTiller(uint8_t tillers);

// tillers is a bitfield here, NOT an array index. This should only be called when in diag mode.
void diagStopTiller(uint8_t tillers);

// tillers is a bitfield here, NOT an array index. This should only be called when in diag mode.
void diagSetTiller(uint8_t tillers, uint8_t target);

//////////////////////////////////////////////////////////////////////////
// E-Stop functions
//////////////////////////////////////////////////////////////////////////

// Stops every tiller raise operation and locks each tiller so updateTillers() knows not to do anything.
// Note that EVERY diagnostic and processing command should ALWAYS check if an e-stop is engaged and, if
// so, return without performing any operation. Thus, the ONLY way to recover from an e-stop is by
// calling tillersEnter???Mode()
void estopTillers(void);

#endif /* TILLER_H_ */