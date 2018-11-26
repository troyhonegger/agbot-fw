/*
 * Estop.h
 * This header defines the necessary data and functions for handling a machine e-stop.
 * At present, this simply means that it defines the global estopEngaged flag, as well as
 * functions for entering and exiting the e-stopped state
 * Created: 11/25/2018 10:00:19 PM
 *  Author: troy.honegger
 */ 


#ifndef ESTOP_H_
#define ESTOP_H_

// Set to true if an e-stop is engaged and false otherwise. All sprayer/tiler functions should
// always check if this flag is set before doing any work. No function outside of the module
// that implements this header should set this flag manually - use estopMachine() and
// estopRecover() instead.
extern bool estopEngaged;

// This function should only be called as a last resort. It shuts off all sprayers, stops
// all tillers from raising and lowering, and sets the estopEngaged flag to prevent any
// sprayer or tiller from becoming active until the flag is cleared.
void estopMachine(void);

// This function can be used to recover from an e-stop. It should ONLY be called in response
// to a specific request, NOT as a part of any larger operation. Note that this function
// primarily just clears the estopEngaged flag, so it is still necessary to call
// tillersEnter???Mode() and sprayersEnter???Mode() before normal processing can resume
void estopRecover(void);

#endif /* ESTOP_H_ */