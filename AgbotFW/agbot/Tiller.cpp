/*
 * Tiller.cpp
 * Implements the Tiller class defined in Tiller.hpp for interacting with the tillers on the machine.
 * See Tiller.hpp for more info.
 * Created: 11/21/2018 8:13:00 PM
 *  Author: troy.honegger
 */

#include <Arduino.h>

#include "Common.hpp"
#include "Config.hpp"
#include "Tiller.hpp"

#include <string.h>

void Tiller::begin(uint8_t id, Config const* config) {
	state = (id & 3) << 4;
	assert(config);
	targetHeight = STOP;
	this->config = config;
	pinMode(getRaisePin(), OUTPUT);
	digitalWrite(getRaisePin(), getOffVoltage());
	pinMode(getLowerPin(), OUTPUT);
	digitalWrite(getLowerPin(), getOffVoltage());
	pinMode(getHeightSensorPin(), INPUT);

	updateActualHeight();
}

Tiller::~Tiller() {
	pinMode(getRaisePin(), INPUT);
	pinMode(getLowerPin(), INPUT);
}

inline void Tiller::updateActualHeight() const { actualHeight = map(analogRead(getHeightSensorPin()), 1023, 204, 0, MAX_HEIGHT); }

bool Tiller::setHeight(uint8_t command, uint32_t delay) {
	uint32_t triggerTime = millis() + delay;
	// stop all timers that are triggered to fire after this timer
	for (unsigned int i = 0; i < sizeof(timers) / sizeof(timers[0]); i++) {
		if (timers[i].isSet && timeCmp(triggerTime, timers[i].time) <= 0) {
			timers[i].stop();
		}
	}
	if (delay) {
		for (unsigned int i = 0; i < sizeof(timers) / sizeof(timers[0]); i++) {
			if (!timers[i].isSet) {
				timers[i].start(delay);
				commandList[i] = command;
				goto foundSlot;
			}
		}
		// error - didn't find a slot to put the command
		return false;
		foundSlot: ;
	}
	else {
		targetHeight = command;
	}
	return true;
}

// NOWNOW make sure there's enough room to store two commands - if we only have
// room for one, (which may be quite likely) the tiller will stay down forever
void Tiller::killWeed() {
	uint32_t delay;
	// lowerTime = millis() + responseDelay - (raisedHeight - loweredHeight)*tillerLowerTime/100 - precision/2
	delay = config->get(Setting::ResponseDelay)
		- (config->get(Setting::TillerRaisedHeight) - config->get(Setting::TillerLoweredHeight)) * static_cast<long>(config->get(Setting::TillerLowerTime)) / 100l
		- config->get(Setting::Precision) / 2;
	setHeight(TillerCommand::LOWERED, delay);
	delay = config->get(Setting::ResponseDelay) + config->get(Setting::Precision) / 2;
	setHeight(TillerCommand::RAISED, delay);
}

void Tiller::update() {
	// see if any commands are done waiting and ready to be executed.
	for (unsigned int i = 0; i < sizeof(commandList) / sizeof(commandList[0]); i++) {
		if (timers[i].isUp()) {
			targetHeight = commandList[i];
			break;
		}
	}

	updateActualHeight();
	int8_t newDh;
	switch (targetHeight) {
		case TillerCommand::STOP:
			newDh = 0;
			break;
		case TillerCommand::UP:
			newDh = 1;
			break;
		case TillerCommand::DOWN:
			newDh = -1;
			break;
		//NOWNOW use actual actuator feedback (and in the case of RAISED and LOWERED, height sensor data) to compute newDh
		case TillerCommand::LOWERED:
			newDh = -1;
			break;
		case TillerCommand::RAISED:
			newDh = 1;
			break;
		default: // integer 0-100
			if (targetHeight < MAX_HEIGHT / 2) { newDh = -1; }
			else { newDh = 1; }
	}

	if (newDh != getDH()) {
		setDH(newDh);
		switch (newDh) {
			case 0:
				digitalWrite(getRaisePin(), getOffVoltage());
				digitalWrite(getLowerPin(), getOffVoltage());
				break;
			case 1:
				digitalWrite(getLowerPin(), getOffVoltage());
				digitalWrite(getRaisePin(), getOnVoltage());
				break;
			case -1:
				digitalWrite(getRaisePin(), getOffVoltage());
				digitalWrite(getLowerPin(), getOnVoltage());
				break;
		}
	}
}

size_t Tiller::serialize(char *str, size_t n) const {
	updateActualHeight();
	char targetStr[10];
	switch (targetHeight) {
		case TillerCommand::STOP:
			strcpy_P(targetStr, PSTR("\"STOP\""));
			break;
		case TillerCommand::DOWN:
			strcpy_P(targetStr, PSTR("\"DOWN\""));
			break;
		case TillerCommand::LOWERED:
			strcpy_P(targetStr, PSTR("\"LOWERED\""));
			break;
		case TillerCommand::RAISED:
			strcpy_P(targetStr, PSTR("\"RAISED\""));
			break;
		case TillerCommand::UP:
			strcpy_P(targetStr, PSTR("\"UP\""));
			break;
		default:
			sprintf_P(targetStr, PSTR("%d"), targetHeight);
			break;
	}
	return snprintf_P(str, n, PSTR("{\"height\":%hhu,\"dh\":%hhd,\"target\":%s}"), actualHeight, getDH(), targetStr);
}
