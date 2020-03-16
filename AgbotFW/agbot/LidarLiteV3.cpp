/*
 * LidarLiteV3.cpp
 *
 * Created: 3/14/2020 5:34:32 PM
 *  Author: troy.honegger
 */ 

#include "Common.hpp"

#include <EEPROM.h>
#include <Wire.h>

#include "LidarLiteV3.hpp"

#define LIDAR_EEPROM_OFFSET(id)		(256 + (id) * sizeof(LidarLiteV3::serial))
#define LIDAR_I2C_ID(id)			(LidarLiteV3::START_ADDRESS + (id))

// LIDAR registers
#define ACQ_COMMAND		0x00
#define STATUS			0x01
#define FULL_DELAY_HIGH	0x0f
#define FULL_DELAY_LOW	0x10
#define UNIT_ID_HIGH	0x16
#define UNIT_ID_LOW		0x17
#define I2C_ID_HIGH		0x18
#define I2C_ID_LOW		0x19
#define I2C_SEC_ADDR	0x1a
#define I2C_CONFIG		0x1e

// LIDAR register value definitions
#define ACQ_COMMAND__MEASURE_NO_CORRECTION		0x03
#define ACQ_COMMAND__MEASURE_WITH_CORRECTION	0x04
#define I2C_CONFIG__DISABLE_DEFAULT_ADDRESS		0x08

// State machine states
#define LIDAR_STATE__UNINITIALIZED			0
#define LIDAR_STATE__SET_ADDRESS			1
#define LIDAR_STATE__SET_ADDRESS_FAIL		2
#define LIDAR_STATE__CONFIGURE				3
#define LIDAR_STATE__READY					4 /* SM is considered "initialized" if state >= READY */
#define LIDAR_STATE__BUSY					5
#define LIDAR_STATE__MEASURED				6

static inline int readRegister(uint8_t i2cAddress, uint8_t regAddress) {
	uint8_t byteCount =
		Wire.requestFrom(i2cAddress,
							1, // read size
							regAddress,
							1, // register size
							true // sendStop
						);
	if (byteCount) {
		return Wire.read();
	}
	else {
		return -1;
	}
}

static inline void writeRegister(uint8_t i2cAddress, uint8_t regAddress, uint8_t value) {
	Wire.beginTransmission(i2cAddress);
	Wire.write(regAddress);
	Wire.write(value);
	Wire.endTransmission();
}

void LidarLiteV3::begin(uint8_t id) {
	LidarLiteV3::id = id;
	EEPROM.get(LIDAR_EEPROM_OFFSET(id), serial);
	height = 0;
	state = LIDAR_STATE__UNINITIALIZED;
}

bool LidarLiteV3::isInitialized(void) { return state >= LIDAR_STATE__READY; }

void LidarLiteV3::update(void) {
	int readValue;
	bool stop;
	do {
		stop = true;
		switch (state) {
			case LIDAR_STATE__UNINITIALIZED:
			// Unlock the correct chip by writing its unique serial number to I2C_ID_HIGH and I2C_ID_LOW
			writeRegister(DEFAULT_ADDRESS, I2C_ID_HIGH, (serial >> 8) & 0x00FF);
			writeRegister(DEFAULT_ADDRESS, I2C_ID_LOW, serial & 0x00FF);
			// Write new I2C address of sensor to I2C_SEC_ADDR. There may be multiple slaves at the default
			// address, but the write will only succeed on the one whose serial number matches I2C_ID
			writeRegister(DEFAULT_ADDRESS, I2C_SEC_ADDR, LIDAR_I2C_ID(id));
			state = LIDAR_STATE__SET_ADDRESS;
			break;

			case LIDAR_STATE__SET_ADDRESS:
			readValue = readRegister(LIDAR_I2C_ID(id), STATUS);
			if (readValue >= 0) {
				// successfully set I2C address. We can now proceed with the rest of the configuration
				state = LIDAR_STATE__CONFIGURE;
			}
			else {
				// that address wasn't on the network
				state = LIDAR_STATE__SET_ADDRESS_FAIL;
			}
			break;

			case LIDAR_STATE__SET_ADDRESS_FAIL:
			readValue = readRegister(START_ADDRESS, UNIT_ID_HIGH);
			if (readValue >= 0) {
				uint16_t newSerial = readValue << 8;
				readValue = readRegister(START_ADDRESS, UNIT_ID_LOW);
				if (readValue >= 0) {
					newSerial |= readValue & 0x00FF;
					EEPROM.update(LIDAR_EEPROM_OFFSET(id),		(uint8_t) (newSerial & 0xFF));
					EEPROM.update(LIDAR_EEPROM_OFFSET(id) + 1,	(uint8_t) (newSerial >> 8));
					// try again from ground zero
					serial = newSerial;
					state = LIDAR_STATE__UNINITIALIZED;
				}
				else {
					// no one is listening at START_ADDRESS. Keep trying
					state = LIDAR_STATE__SET_ADDRESS_FAIL;
				}
			}
			else {
				// no one is listening at START_ADDRESS. Keep trying
				state = LIDAR_STATE__SET_ADDRESS_FAIL;
			}
			break;

			case LIDAR_STATE__CONFIGURE:
			writeRegister(LIDAR_I2C_ID(id), I2C_CONFIG, I2C_CONFIG__DISABLE_DEFAULT_ADDRESS);
			// TODO any other settings should be set here.
			// Continuous measurements would be cool, but I think there's a race condition
			// if the data updates in between reading the most and least significant bytes.
			state = LIDAR_STATE__READY;
			break;

			case LIDAR_STATE__READY:
			if ((numReadings & 0x1F)) { // if count is not divisible by 32
				writeRegister(LIDAR_I2C_ID(id), ACQ_COMMAND, ACQ_COMMAND__MEASURE_NO_CORRECTION);
			}
			else {
				writeRegister(LIDAR_I2C_ID(id), ACQ_COMMAND, ACQ_COMMAND__MEASURE_WITH_CORRECTION);
			}
			state = LIDAR_STATE__BUSY;
			break;

			case LIDAR_STATE__BUSY:
			readValue = readRegister(LIDAR_I2C_ID(id), STATUS);
			if (!(readValue & 1)) { // bit 0 of status register is 1 if busy, 0 if complete
				state = LIDAR_STATE__MEASURED;
				stop = false;
			}
			break;

			case LIDAR_STATE__MEASURED:
			readValue = readRegister(LIDAR_I2C_ID(id), FULL_DELAY_HIGH);
			height = readValue << 8;
			readValue = readRegister(LIDAR_I2C_ID(id), FULL_DELAY_LOW);
			height |= (readValue & 0xFF);
			numReadings++;
			state = LIDAR_STATE__READY;
			break;
		}
	} while (!stop);
}


void LidarLiteBank::begin(void) {
	//TODO
}

void LidarLiteBank::update(void) {
	//TODO
}