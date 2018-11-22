/*
 * Tiller.cpp
 * This module contains a set of functions to be used to interact with the tillers on the machine. It declares a global array
 * containing all three tillers on the machine. It also defines diagnostic operations (to raise, lower, and stop tillers manually),
 * as well as processing operations (to schedule tiller operations as weeds are encountered).
 * Created: 11/21/2018 8:13:00 PM
 *  Author: troy.honegger
 */

#include "Tiller.h"

#include <Arduino.h>

struct tiller g_tillers[NUM_TILLERS];

/*
 * Initializes the contents of g_tillers to reasonable start values. The tillers start out "stateless" - they are neither in diag
 * nor in normal processing mode. As such, no operations should be performed until tillers_start_processing() or tillers_start_diag()
 * is called. The behavior is non-deterministic if other functions are called first.
 */

void init_tillers() {
	for (uint8_t i = 0; i < NUM_TILLERS; i++) {
		g_tillers[i].timestamp = millis();
		g_tillers[i].height = MAX_TILLER_HEIGHT;
		g_tillers[i].target = TILLER_STOP;
		g_tillers[i].direction = TILLER_STOPPED;
		g_tillers[i].state = TILLER_STATELESS;
	}
} /* init_tillers() */

/*
 * Helper function to raise a single tiller. This schedules the raise operation, but it does NOT perform any I/O.
 */

static void raise_tiller(uint8_t tiller_id) {
	g_tillers[tiller_id].timestamp = millis();
	g_tillers[tiller_id].target = MAX_TILLER_HEIGHT;
	g_tillers[tiller_id].state = TILLER_RAISED;
} /* raise_tiller() */

/*
 * Updates the tiller's height property to reflect the actual height of the tiller. Depending on the implementation of the multivator,
 * this operation could be timing-based, or it could be based on reading an analog input.
 */

static void update_tiller_height(uint8_t tiller_id) {
	//TODO: Implement this
} /* update_tiller_height() */