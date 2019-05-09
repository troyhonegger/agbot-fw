/*
 * Throttle.h
 * Encapsulates the data necessary to interact with the pony motor throttle. At every iteration of the main loop, 
 *
 * Created: 5/7/2019 4:50:13 PM
 *  Author: troy.honegger
 */ 

#pragma once

#include "Common.hpp"

namespace agbot {
	class Throttle {
		public:
			static const uint8_t XTD_LEN = 100; // TODO: find through experimentation
			static const uint8_t RET_LEN = 0; // TODO: find through experimentation
			static const uint8_t XTD_PIN = 48;
			static const uint8_t RET_PIN = 49;
			static const uint8_t SENSOR_PIN = PIN_A14;
			static const uint8_t ACCURACY = 5; // TODO: re-hardcode if needed (no time to add config option)
		private:
			static const uint8_t ON_VOLTAGE = LOW; // TODO: change if active high
			static const uint8_t OFF_VOLTAGE = !ON_VOLTAGE;

			bool throttledUp;
			mutable volatile uint8_t actuatorLength;
			int8_t dl;

			void updateActuatorLength() const;
			int8_t getDesiredDL() const;
		public:
			Throttle() : throttledUp(false), actuatorLength(0), dl(0) { }

			void begin();

			void up();
			void down();

			// The ONLY function that should perform I/O.
			void update();
	};
}