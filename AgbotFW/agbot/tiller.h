/*
 * TimeHelper.h
 * This header defines a set of data and functions to be used to interact with the sprayers on the machine. It defines the tiller
 * structure, which encapsulates the state of a single tiller, as well as a global array containing all eight tillers on the machine.
 * It also defines diagnostic operations (to raise, lower, and stop tillers manually) as well as processing operations (to schedule
 * tiller operations as weeds are encountered).
 * Created: 11/21/2018 8:13:00 PM
 *  Author: troy.honegger
 */

#ifndef TILLER_H_
#define TILLER_H_

#include <Arduino.h>

#define TILLER_STATELESS (0)
#define TILLER_DIAG (1)
#define TILLER_RAISED (2)
#define TILLER_LOWER_SCHEDULED (3)
#define TILLER_LOWERING (4)

#define TILLER_UP (1)
#define TILLER_DOWN (-1)
#define TILLER_STOPPED (0)

#define MAX_TILLER_HEIGHT (100)
#define TILLER_STOP (MAX_TILLER_HEIGHT + 1)

#define NUM_TILLERS (3)

struct tiller {
	uint32_t timestamp;
	uint8_t height;
	uint8_t target;
	int8_t direction;
	uint8_t state;
};

extern struct tiller g_tillers[NUM_TILLERS];

void init_tillers();

void schedule_tiller_lower(uint8_t);

void update_tillers(); //should update height and direction of all tillers, performing I/O where necessary

void tillers_start_processing();
void tillers_start_diag();
//should be called in diag mode ONLY
void tiller_up(uint8_t);
void tiller_down(uint8_t);
void tiller_stop(uint8_t);
void tiller_set(uint8_t tiller, uint8_t target);

#endif /* TILLER_H_ */