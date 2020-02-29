/*
 * Sprayer.cpp
 * Implements the agbot::Sprayer class defined in Sprayer.hpp for interacting with the sprayers on the machine.
 * See Sprayer.hpp for more info.
 * Created: 11/21/2018 8:00:00 PM
 *  Author: troy.honegger
 */

#include "Common.hpp"
#include "Config.hpp"
#include "Sprayer.hpp"

#define SET_BIT(x, i)		((x) |=  (1<<(i)))
#define UNSET_BIT(x, i)		((x) &= ~(1<<(i)))
#define IS_SET(x, i)		(!!((x) & (1<<(i))))

namespace agbot {
	void Sprayer::begin(uint8_t id, Config const* config) {
		UNSET_BIT(state, 7); // status = OFF
		state = id & 0xF;
		assert(config);
		this->config = config;
		pinMode(getPin(), OUTPUT);
		digitalWrite(getPin(), OFF_VOLTAGE);
	}

	Sprayer::~Sprayer() {
		setActualStatus(OFF);
		pinMode(getPin(), INPUT);
	}

	inline void Sprayer::setActualStatus(bool status) {
		if (getStatus() != status) {
			if (status == ON) {
				state |= 0x80;
				digitalWrite(getPin(), ON_VOLTAGE);
			}
			else {
				state &= 0x7F;
				digitalWrite(getPin(), OFF_VOLTAGE);
			}
		}
	}

	bool Sprayer::setStatus(bool status, uint32_t delay) {
		uint32_t triggerTime = millis() + delay;
		for (unsigned int i = 0; i < sizeof(timers) / sizeof(timers[0]); i++) {
			if (timers[i].isSet && timeCmp(triggerTime, timers[i].time) <= 0) {
				timers[i].stop();
			}
		}
		if (delay) {
			for (unsigned int i = 0; i < sizeof(timers) / sizeof(timers[0]); i++) {
				if (!timers[i].isSet) {
					timers[i].start(delay);
					if (status) { SET_BIT(commandList, i); }
					else { UNSET_BIT(commandList, i); }
					goto foundSlot;
				}
			}
			// error - didn't find a slot to put the command
			return false;
			foundSlot: ;
		}
		else {
			setActualStatus(status);
		}
		return true;
	}

	// NOWNOW make sure there's enough room to store two commands - if we only have
	// room for one, (which may be quite likely) the sprayer will lock on forever
	void Sprayer::killWeed() {
		setStatus(ON, config->get(Setting::ResponseDelay) - config->get(Setting::Precision) / 2);
		setStatus(OFF, config->get(Setting::ResponseDelay) + config->get(Setting::Precision) / 2);
	}

	void Sprayer::update() {
		// see if any commands are done waiting and ready to be executed.
		for (unsigned int i = 0; i < sizeof(timers) / sizeof(timers[0]); i++) {
			if (timers[i].isUp()) {
				setActualStatus(IS_SET(commandList, i));
				break;
			}
		}
	}

	size_t Sprayer::serialize(char* str, size_t n) const {
		if (getStatus()) {
			return snprintf_P(str, n, PSTR("{\"status\": \"ON\"}"));
		}
		else {
			return snprintf_P(str, n, PSTR("{\"status\": \"OFF\"}"));
		}
	}
}