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
	uint32_t timestamp; /* set every time the sprayer changes state */
	uint8_t state; /* state (on, off, scheduled, or diag) - should not be accessed externally; use status instead */
	bool status; /* on or off */
};

extern struct sprayer g_sprayers[NUM_SPRAYERS];


/* Initializes g_sprayers, configures the GPIO pins, and performs all other necessary initialization work for the sprayers.
 * Note that sprayers_enter_<diag/process>_mode() still needs to be called before diag or process functions are called. */
void init_sprayers(void);

/* Shuts off all sprayers and switches them to process mode. */
void sprayers_enter_process_mode(void);

/* Shuts off all sprayers and switches them to diag mode. */
void sprayers_enter_diag_mode(void);


/* sprayers is a bitfield here, NOT an array index. This should only be called when in process mode. */
void schedule_spray(uint8_t sprayers);

/* Performs any scheduled set sprayer operations. This should be called every loop iteration. In diag mode, it will have no effect. */
void update_sprayers(void);


/* sprayers is a bitfield here, NOT an array index. This should only be called when in diag mode. */
void diag_set_sprayer(uint8_t sprayers, bool status);

#endif //SPRAYER_H