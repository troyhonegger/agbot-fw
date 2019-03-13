/*
 * Tiller.cpp
 * This module contains a set of functions to be used to interact with the tillers on the machine. It declares a global array
 * containing all three tillers on the machine. It also defines diagnostic operations (to raise, lower, and stop tillers manually),
 * as well as processing operations (to schedule tiller operations as weeds are encountered).
 * Created: 11/21/2018 8:13:00 PM
 *  Author: troy.honegger
 */

#include <Arduino.h>

#include "Common.hpp"
#include "Config.hpp"
#include "Tiller.hpp"

namespace agbot {
	void Tiller::begin(uint8_t id, Config const* config) {
		state = ((id & 3) << 4) | static_cast<uint8_t>(TillerState::Unset);
		raiseTime = lowerTime = millis();
		targetHeight = STOP;
		actualHeight = MAX_HEIGHT;
		Tiller::config = config;
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

	inline void Tiller::updateActualHeight() { actualHeight = map(analogRead(getHeightSensorPin()), 0, 1023, 0, MAX_HEIGHT); }

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

	void Tiller::setHeight(uint8_t height) {
		if (getState() == TillerState::Diag) {
			targetHeight = height;
		}
	}
	
	void Tiller::stop() {
		switch (getState()) {
			case TillerState::Diag:
				targetHeight = STOP;
				break;
			case TillerState::ProcessLowering:
			case TillerState::ProcessRaising:
			case TillerState::ProcessScheduled:
				raiseTime = lowerTime = millis();
				setState(TillerState::ProcessStopped);
				targetHeight = STOP;
				break;
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

	void Tiller::resume() {
		if (getState() == TillerState::ProcessStopped) {
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
			if ((targetHeight > actualHeight) && (targetHeight - actualHeight > config->get(Setting::TillerAccuracy))) { newDh = 1; }
			else if ((targetHeight < actualHeight) && (actualHeight - targetHeight > config->get(Setting::TillerAccuracy))) { newDh = -1; }
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
}