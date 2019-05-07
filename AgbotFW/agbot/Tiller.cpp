/*
 * Tiller.cpp
 * Implements the agbot::Tiller class defined in Tiller.hpp for interacting with the tillers on the machine.
 * See Tiller.hpp for more info.
 * Created: 11/21/2018 8:13:00 PM
 *  Author: troy.honegger
 */

#include <Arduino.h>

#include "Common.hpp"
#include "Config.hpp"
#include "Tiller.hpp"

#include <string.h>

static const char TILLER_FMT_STR[] PROGMEM = "{\"height\":%hhu,\"dh\":%hhd,\"target\":%hhu,\"until\":%ld}";
static const char TILLER_NO_UNTIL_FMT_STR[] PROGMEM = "{\"height\":%hhu,\"dh\":%hhd,\"target\":%hhu}";
static const char TILLER_STOPPED_FMT_STR[] PROGMEM = "{\"height\":%hhu,\"dh\":%hhd,\"target\":\"STOP\",\"until\":%ld}";
static const char TILLER_STOPPED_NO_UNTIL_FMT_STR[] PROGMEM = "{\"height\":%hhu,\"dh\":%hhd,\"target\":\"STOP\"}";

namespace agbot {
	void Tiller::begin(uint8_t id, Config const* config) {
		state = ((id & 3) << 4) | static_cast<uint8_t>(TillerState::Unset);
		raiseTime = lowerTime = millis();
		*config; // dereference seg faults if null
		targetHeight = STOP;
		actualHeight = MAX_HEIGHT;
		this->config = config;
		pinMode(getRaisePin(), OUTPUT);
		digitalWrite(getRaisePin(), OFF_VOLTAGE);
		pinMode(getLowerPin(), OUTPUT);
		digitalWrite(getLowerPin(), OFF_VOLTAGE);
		pinMode(getHeightSensorPin(), INPUT);
	}

	Tiller::~Tiller() {
		pinMode(getRaisePin(), INPUT);
		pinMode(getLowerPin(), INPUT);
	}

	inline void Tiller::updateActualHeight() const { actualHeight = map(analogRead(getHeightSensorPin()), 0, 1023, 0, MAX_HEIGHT); }

	void Tiller::setMode(MachineMode mode) {
		switch (mode) {
			case MachineMode::Unset:
				lowerTime = raiseTime = millis();
				setState(TillerState::Unset);
				targetHeight = STOP;
				break;
			case MachineMode::Process:
				lowerTime = raiseTime = millis();
				setState(TillerState::ProcessRaising);
				targetHeight = config->get(Setting::TillerRaisedHeight);
				break;
			case MachineMode::Diag:
				lowerTime = raiseTime = millis();
				setState(TillerState::Diag);
				targetHeight = STOP;
				break;
			default:
				break;
		}
	}

	void Tiller::setTargetHeight(uint8_t height) {
		if (getState() == TillerState::Diag) {
			targetHeight = height;
		}
	}
	
	void Tiller::stop(bool now, bool temporary) {
		if (!temporary) { targetHeight = STOP; }
		if (now && getDH()) {
			setDH(0);
			digitalWrite(getRaisePin(), OFF_VOLTAGE);
			digitalWrite(getLowerPin(), OFF_VOLTAGE);
		}
	}

	void Tiller::scheduleLower() {
		bool isProcessing = false;
		switch (getState()) {
			case TillerState::ProcessRaising:
			case TillerState::ProcessLowering:
				isProcessing = true;
				// lowerTime = millis() + responseDelay - (raisedHeight - loweredHeight)*tillerLowerTime/100 - precision/2
				lowerTime = millis() + config->get(Setting::ResponseDelay) -
					((config->get(Setting::TillerRaisedHeight) - config->get(Setting::TillerLoweredHeight))*config->get(Setting::TillerLowerTime)
					+ config->get(Setting::Precision) * 50) / 100;
				break;
			case TillerState::ProcessScheduled:
				isProcessing = true;
				break;
		}
		if (isProcessing) {
			raiseTime = millis() + config->get(Setting::ResponseDelay) + config->get(Setting::Precision) / 2;
			setState(TillerState::ProcessScheduled);
		}
	}

	void Tiller::cancelLower() {
		if (getState() == TillerState::ProcessScheduled || getState() == TillerState::ProcessRaising) {
			raiseTime = lowerTime = millis();
			setState(TillerState::ProcessRaising);
			targetHeight = config->get(Setting::TillerRaisedHeight);
		}
	}

	void Tiller::update() {
		if (getState() == TillerState::ProcessScheduled && isElapsed(lowerTime)) {
			setState(TillerState::ProcessLowering);
			targetHeight = config->get(Setting::TillerLoweredHeight);
		}
		else if (getState() == TillerState::ProcessLowering && isElapsed(raiseTime)) {
			setState(TillerState::ProcessRaising);
			targetHeight = config->get(Setting::TillerRaisedHeight);
		}

		updateActualHeight();
		int8_t newDh = 0;
		if (targetHeight != STOP) {
			// The following applies a sort of software hysteresis to the tiller control logic.
			// If |target - initial| <= accuracy / 2, the tiller must be stoppped;
			// if |target - initial| > accuracy, the tiller must be started;
			// if accuracy / 2 <= |target - initial| < accuracy, the tiller will maintain the
			// previous state, unless doing so would push it further away from the target height
			uint16_t accuracy = config->get(Setting::TillerAccuracy);
			if (targetHeight > actualHeight) {
				if (targetHeight - actualHeight > accuracy) { newDh = 1; }
				else if (targetHeight - actualHeight <= (accuracy / 2)) { newDh = 0; }
				else { newDh = getDH() == 1 ? 1 : 0; }
			}
			else if (targetHeight < actualHeight) {
				if (actualHeight - targetHeight > accuracy) { newDh = -1; }
				else if (actualHeight - targetHeight <= (accuracy / 2)) { newDh = 0; }
				else { newDh = getDH() == -1 ? -1 : 0; }
			}
		}

		if (newDh != getDH()) {
			setDH(newDh);
			switch (newDh) {
				case 0:
					digitalWrite(getRaisePin(), OFF_VOLTAGE);
					digitalWrite(getLowerPin(), OFF_VOLTAGE);
					break;
				case 1:
					digitalWrite(getLowerPin(), OFF_VOLTAGE);
					digitalWrite(getRaisePin(), ON_VOLTAGE);
					break;
				case -1:
					digitalWrite(getRaisePin(), OFF_VOLTAGE);
					digitalWrite(getLowerPin(), ON_VOLTAGE);
					break;
			}
		}
	}

	size_t Tiller::serialize(char *str, size_t n) const {
		updateActualHeight();
		TillerState state = getState();
		if (state != TillerState::ProcessLowering && state != TillerState::ProcessScheduled) {
			if (targetHeight == Tiller::STOP) {
				return snprintf_P(str, n, TILLER_STOPPED_NO_UNTIL_FMT_STR, actualHeight, getDH());
			}
			else {
				return snprintf_P(str, n, TILLER_NO_UNTIL_FMT_STR, actualHeight, getDH(), targetHeight);
			}
		}
		else {
			int32_t until = state == TillerState::ProcessScheduled ? lowerTime - millis() : raiseTime - millis();
			if (targetHeight == Tiller::STOP) {
				return snprintf_P(str, n, TILLER_STOPPED_FMT_STR, actualHeight, getDH(), until);
			}
			else {
				return snprintf_P(str, n, TILLER_FMT_STR, actualHeight, getDH(), targetHeight, until);
			}
		}
	}
}