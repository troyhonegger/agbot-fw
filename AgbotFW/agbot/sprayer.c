#include "sprayer.h"

#define MIN_SPRAYER_PIN (9) /* sprayer pins are numbered 9, 10, 11, 12, 13, 14, 15, 16 */
#define SPRAYER_OFF_VOLTAGE (HIGH) /* sprayers are active low */
#define SPRAYER_ON_VOLTAGE (LOW) /* sprayers are active low */

struct sprayer g_sprayers[NUM_SPRAYERS];

/* Initializes g_sprayers, configures the GPIO pins, and performs all other necessary initialization work for the sprayers.
 * Note that sprayers_enter_<diag/process>_mode() still needs to be called before diag or process functions are called. */
void init_sprayers(void) {
	for (uint8_t i = 0; i < NUM_SPRAYERS; i++) {
		pinMode(MIN_SPRAYER_PIN + i, OUTPUT);
		digitalWrite(MIN_SPRAYER_PIN + i, SPRAYER_OFF_VOLTAGE);
		g_sprayers[i].timestamp = millis();
		g_sprayers[i].state = SPRAYER_STATE_UNSET;
		g_sprayers[i].status = SPRAYER_OFF;
	}
} /* init_sprayers() */

/* Shuts off all sprayers and sets them to process mode. Note that the sprayers will be turned off (and all scheduled sprayer
 * operations will be cancelled) even if the sprayers were already in process mode. If the machine is already in process mode,
 * this generally should not be called (though it might make sense under exceptional circumstances). */
void sprayers_enter_process_mode(void) {
	for (uint8_t i = 0; i < NUM_SPRAYERS; i++) {
		digitalWrite(MIN_SPRAYER_PIN + i, SPRAYER_OFF_VOLTAGE);
		g_sprayers[i].timestamp = millis();
		g_sprayers[i].state = SPRAYER_PROCESS_OFF;
		g_sprayers[i].status = SPRAYER_OFF;
	}
} /* sprayers_enter_process_mode() */

/* Shuts off all sprayers and sets them to diag mode. Note that the sprayers will be turned off even if the sprayers were already
 * in diag mode. If the machine is already in diag mode, this generally should not be called (though it might make sense under
 * exceptional circumstances). */
void sprayers_enter_diag_mode(void) {
	for (uint8_t i = 0; i < NUM_SPRAYERS; i++) {
		digitalWrite(MIN_SPRAYER_PIN + i, SPRAYER_OFF_VOLTAGE);
		g_sprayers[i].timestamp = millis();
		g_sprayers[i].state = SPRAYER_DIAG_OFF;
		g_sprayers[i].status = SPRAYER_OFF;
	}
} /* sprayers_enter_diag_mode() */

/* Schedules a spray operation to be performed after a length of time. This should only be called when in process mode. This
 * operation performs no I/O, as it is only scheduling an operation to be performed later. Note that sprayers is referring
 * to a bitfield, not an array index, so multiple sprayers can be scheduled at once. */
void schedule_spray(uint8_t sprayers) {
	uint8_t bitmask = 1;
	for (uint8_t i = 0; i < NUM_SPRAYERS; i++) {
		if (sprayers & bitmask && g_sprayers[i].status != SPRAYER_PROCESS_SCHEDULED) {
			g_sprayers[i].timestamp = millis();
			g_sprayers[i].status = SPRAYER_PROCESS_SCHEDULED;
		}
		bitmask <<= 1;
	}
} /* schedule_spray() */

/* Performs any scheduled set sprayer operations. This should be called every loop iteration. In diag mode, it will have no effect. */
void update_sprayers(void) {
	unsigned long time = millis();
	for (uint8_t i = 0; i < 8; i++) {
		switch (g_sprayers[i].state) {
			case SPRAYER_PROCESS_SCHEDULED:
				if (time - g_sprayers[i].timestamp > get_sprayer_delay()) {
					/* Turn the sprayer on */
					digitalWrite(MIN_SPRAYER_PIN + i, SPRAYER_ON_VOLTAGE);
					g_sprayers[i].timestamp = time;
					g_sprayers[i].state = SPRAYER_PROCESS_ON;
					g_sprayers[i].status = SPRAYER_ON;
				}
				break;
			case SPRAYER_PROCESS_ON:
				if (time - g_sprayers[i].timestamp > get_precision()) {
					/* Turn the sprayer off */
					digitalWrite(MIN_SPRAYER_PIN + i, SPRAYER_OFF_VOLTAGE);
					g_sprayers[i].timestamp = time;
					g_sprayers[i].state = SPRAYER_PROCESS_OFF;
					g_sprayers[i].status = SPRAYER_OFF;
				}
				break;
		}
	}
}

/* Sets the specified sprayers to the specified mode. The machine should be in diag mode when this is called. Note that sprayers
 * is referring to a bitfield, not an array index, so multiple sprayers can be set at once. */
void diag_set_sprayer(uint8_t sprayers, bool status) {
	uint8_t bitmask = 1;
	for (uint8_t i = 0; i < NUM_SPRAYERS; i++) {
		if (sprayers & bitmask) {
			if (status == SPRAYER_ON) {
				digitalWrite(MIN_SPRAYER_PIN + i, SPRAYER_ON_VOLTAGE);
				g_sprayers[i].timestamp = millis();
				g_sprayers[i].state = SPRAYER_DIAG_ON;
				g_sprayers[i].status = SPRAYER_ON;
			}
			else {
				digitalWrite(MIN_SPRAYER_PIN + i, SPRAYER_OFF_VOLTAGE);
				g_sprayers[i].timestamp = millis();
				g_sprayers[i].state = SPRAYER_DIAG_OFF;
				g_sprayers[i].status = SPRAYER_OFF;
			}
		}
		bitmask <<= 1;
	}
} /* diag_set_sprayer() */