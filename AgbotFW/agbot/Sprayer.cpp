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

namespace agbot {
	void Sprayer::begin(uint8_t id, Config const* config) {
		state = ((id++ & 15) << 3) | static_cast<uint8_t>(SprayerState::Unset);
		onTime = offTime = millis();
		*config; // dereference seg faults if null
		Sprayer::config = config;
		pinMode(getPin(), OUTPUT);
		digitalWrite(getPin(), OFF_VOLTAGE);
	}

	Sprayer::~Sprayer() {
		setStatus(false);
		pinMode(getPin(), INPUT);
	}

	inline void Sprayer::setStatus(bool status) {
		if (isOn() != status) {
			if (status) {
				state |= 0x80;
				digitalWrite(getPin(), ON_VOLTAGE);
			}
			else {
				state &= 0x7F;
				digitalWrite(getPin(), OFF_VOLTAGE);
			}
		}
	}

	void Sprayer::setMode(MachineMode mode) {
		onTime = offTime = millis();
		setStatus(false);
		switch (mode) {
			case MachineMode::Unset:
				setState(SprayerState::Unset);
				break;
			case MachineMode::Diag:
				setState(SprayerState::DiagOff);
				break;
			case MachineMode::Process:
				setState(SprayerState::ProcessOff);
				break;
		}
	}

	void Sprayer::scheduleSpray() {
		bool isProcessing = false;
		switch (getState()) {
			case SprayerState::ProcessOff:
			case SprayerState::ProcessOn:
				isProcessing = true;
				onTime = millis() + config->get(Setting::ResponseDelay) - config->get(Setting::Precision) / 2;
				break;
			case SprayerState::ProcessScheduled:
				isProcessing = true;
				break;
		}

		if (isProcessing) {
			setState(SprayerState::ProcessScheduled);
			offTime = millis() + config->get(Setting::ResponseDelay) + config->get(Setting::Precision) / 2;
		}
	}
	void Sprayer::cancelSpray() {
		if (getState() == SprayerState::ProcessScheduled || getState() == SprayerState::ProcessOn) {
			onTime = offTime = millis();
			setState(SprayerState::ProcessOff);
			setStatus(false);
		}
	}

	void Sprayer::stop() {
		switch (getState()) {
			case SprayerState::DiagOn:
				setIsOn(false);
				break;
			case SprayerState::ProcessOn:
			case SprayerState::ProcessOff:
			case SprayerState::ProcessScheduled:
				onTime = offTime = millis();
				setState(SprayerState::ProcessStopped);
				setStatus(false);
				break;
		}
	}
	void Sprayer::resume() {
		if (getState() == SprayerState::ProcessStopped) {
			onTime = offTime = millis();
			setState(SprayerState::ProcessOff);
			setStatus(false); // status should already be false, but no harm checking
		}
	}

	void Sprayer::setIsOn(bool on) {
		if (on && getState() == SprayerState::DiagOff) {
			setStatus(on);
			setState(SprayerState::DiagOn);
		}
		else if (!on && getState() == SprayerState::DiagOn) {
			setStatus(on);
			setState(SprayerState::DiagOff);
		}
	}

	void Sprayer::update() {
		if (getState() == SprayerState::ProcessScheduled && isElapsed(onTime)) {
			setState(SprayerState::ProcessOn);
			setStatus(true);
		}
		else if (getState() == SprayerState::ProcessOn && isElapsed(offTime)) {
			setState(SprayerState::ProcessOff);
			setStatus(false);
		}
	}
}