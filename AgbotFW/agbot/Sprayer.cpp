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
		state = ((id & 7) << 3) | static_cast<uint8_t>(SprayerState::Unset);
		onTime = offTime = millis();
		*config; // dereference seg faults if null
		Sprayer::config = config;
		pinMode(getPin(), OUTPUT);
		digitalWrite(getPin(), OFF_VOLTAGE);
	}

	Sprayer::~Sprayer() {
		writeStatus(OFF);
		pinMode(getPin(), INPUT);
	}

	inline void Sprayer::writeStatus(bool status) {
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
	inline void Sprayer::setDesiredStatus(bool status) {
		if (status == ON) { state |= 0x40; }
		else { state &= 0xBF; }
	}

	void Sprayer::setMode(MachineMode mode) {
		onTime = offTime = millis();
		setDesiredStatus(OFF);
		switch (mode) {
			case MachineMode::Unset:
				setState(SprayerState::Unset);
				break;
			case MachineMode::Diag:
				setState(SprayerState::Diag);
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
			setDesiredStatus(OFF);
		}
	}

	void Sprayer::stop(bool now) {
		setDesiredStatus(OFF);
		if (now) { writeStatus(OFF); }
	}

	void Sprayer::setStatus(bool status) {
		if (getState() == SprayerState::Diag) {
			setDesiredStatus(status);
		}
	}

	void Sprayer::update() {
		if (getState() == SprayerState::ProcessScheduled && isElapsed(onTime)) {
			setState(SprayerState::ProcessOn);
			setDesiredStatus(ON);
		}
		else if (getState() == SprayerState::ProcessOn && isElapsed(offTime)) {
			setState(SprayerState::ProcessOff);
			setDesiredStatus(OFF);
		}

		// reconcile "desired state" and actual, physical state
		if (getDesiredStatus() != getStatus()) {
			writeStatus(!getStatus());
		}
	}

	size_t Sprayer::serialize(char* str, size_t n) const {
		char buf[4] = "ON\0";
		if (getStatus() == OFF) { buf[1] = 'F'; buf[2] = 'F'; } // OFF
		SprayerState state = getState();
		if (state != SprayerState::ProcessOn && state != SprayerState::ProcessScheduled) {
			return snprintf(str, n, buf);
		}
		else {
			int32_t until = state == SprayerState::ProcessOn ? offTime - millis() : onTime - millis();
			return snprintf(str, n, "%s %ld", buf, until);
		}
		return 0;
	}
}