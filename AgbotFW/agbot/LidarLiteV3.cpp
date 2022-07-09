/*
 * LidarLiteV3.cpp
 *
 * Created: 3/14/2020 5:34:32 PM
 *  Author: troy.honegger
 */ 

#include "Common.h"
#include "Log.h"

#include <Wire.h>

#include "LidarLiteV3.h"

#define LIDAR_I2C_ID(id)			(LidarLiteSensor::START_ADDRESS + ((id) << 1))

// LIDAR registers
#define ACQ_COMMAND			0x00
#define STATUS				0x01
#define ACQ_CONFIG			0x04
#define FULL_DELAY_HIGH		0x0f
#define FULL_DELAY_LOW		0x10
#define OUTER_LOOP_COUNT	0x11
#define UNIT_ID_HIGH		0x16
#define UNIT_ID_LOW			0x17
#define I2C_ID_HIGH			0x18
#define I2C_ID_LOW			0x19
#define I2C_SEC_ADDR		0x1a
#define I2C_CONFIG			0x1e

// LIDAR register value definitions
#define ACQ_COMMAND__MEASURE_NO_CORRECTION		0x03
#define ACQ_COMMAND__MEASURE_WITH_CORRECTION	0x04
#define I2C_CONFIG__DISABLE_DEFAULT_ADDRESS		0x08
#define I2C_CONFIG__ENABLE_NONDEFAULT_ADDRESS	0x10
#define ACQ_CONFIG__VALUES						0x28 /* sensor configuration - each bit has meaning, see datasheet for details. */
#define OUTER_LOOP_COUNT__CONTINUOUS			0xFF

#define ADJACENT_REGISTER						0x80 /* Per the datasheet, set this bit to read/write two adjacent registers at once*/

//#define LIDAR_LITE_BENCH_TESTS /* Verbose logging of LidarLite state transitions is off by default. Uncomment to enable (for HW debugging only) */
#ifdef LIDAR_LITE_BENCH_TESTS
#define LOG_STATE_TRANSITION LOG_VERBOSE
#else
#define LOG_STATE_TRANSITION
#endif

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

static int32_t readAdjacentRegisters(uint8_t i2cAddress, uint8_t regAddress) {
	uint8_t byteCount =
		Wire.requestFrom(i2cAddress,
							2, // read size
							regAddress | ADJACENT_REGISTER,
							1, // register size,
							true // sendStop
						);
	if (byteCount == 2) {
		uint16_t result = Wire.read() & 0xff;
		result <<= 8;
		result |= Wire.read() & 0xff;
		return result;
	}
	else {
		return -1;
	}
}

static inline bool writeRegister(uint8_t i2cAddress, uint8_t regAddress, uint8_t value) {
	Wire.beginTransmission(i2cAddress);
	Wire.write(regAddress);
	Wire.write(value);
	// endTransmission() returns 0 for success or nonzero for error (NACK, etc)
	return !Wire.endTransmission();
}

static inline bool writeAdjacentRegisters(uint8_t i2cAddress, uint8_t regAddress, uint16_t value) {
	Wire.beginTransmission(i2cAddress);
	Wire.write(regAddress | ADJACENT_REGISTER);
	Wire.write(value >> 8);
	Wire.write(value & 0xff);
	// endTransmission() returns 0 for success or nonzero for error (NACK, etc)
	return !Wire.endTransmission();
}

#define LIDAR_STATE__UNPAIRED		0
#define LIDAR_STATE__CONFIGURING	1
#define LIDAR_STATE__WAITFORREAD	2
#define LIDAR_STATE__READ			3

#define READ_DELAY					10 /* ms. Should correspond to the value of the hardware's MEASURE_DELAY register */

void LidarLiteSensor::begin(uint8_t id) {
	LidarLiteSensor::id = id;
	height = 0;
	enterState_Unpaired();
}

void LidarLiteSensor::update(void) {
	switch (state) {
		case LIDAR_STATE__UNPAIRED:
			if (paired) {
				bool success = enterState_Configuring();
				if (success) {
					enterState_WaitForRead();
					LOG_STATE_TRANSITION("LidarLite: Unpaired->Configuring->WaitForRead (success)");
				}
				else {
					enterState_Unpaired();
					LOG_STATE_TRANSITION("LidarLite: Unpaired->Configuring->Unpaired (failure)");
				}
			}
		break;
		case LIDAR_STATE__WAITFORREAD:
			if (timer.isUp()) {
				bool success = enterState_Read();
				if (success) {
					enterState_WaitForRead();
					LOG_STATE_TRANSITION("LidarLite: Read Success %d", height);
				}
				else {
					enterState_Unpaired();
					LOG_STATE_TRANSITION("LidarLite: Read->Unpaired");
				}
			}
		break;
		default:
			assert(0);
		break;
	}
}

void LidarLiteSensor::enterState_Unpaired(void) {
	// mark as unpaired
	paired = false;

	state = LIDAR_STATE__UNPAIRED;
}

bool LidarLiteSensor::enterState_Configuring(void) {
	bool ret;
	
	// send sensor configuration
	ret = writeRegister(LIDAR_I2C_ID(id), ACQ_CONFIG, ACQ_CONFIG__VALUES);
	if (ret) {
		ret = writeRegister(LIDAR_I2C_ID(id), OUTER_LOOP_COUNT, OUTER_LOOP_COUNT__CONTINUOUS);
	}
	if (ret) {
		ret = writeRegister(LIDAR_I2C_ID(id), ACQ_COMMAND, ACQ_COMMAND__MEASURE_WITH_CORRECTION);
	}
	state = LIDAR_STATE__CONFIGURING;
	return ret;
}

void LidarLiteSensor::enterState_WaitForRead(void) {
	timer.restart(READ_DELAY);

	state = LIDAR_STATE__WAITFORREAD;
}

bool LidarLiteSensor::enterState_Read(void) {
	int32_t result = readAdjacentRegisters(LIDAR_I2C_ID(id), FULL_DELAY_HIGH);
	if (result < 0) {
		return false;
	}
	else {
		//TODO-NOWNOW test accuracy. may have to apply a calibration. Per datasheet response is nonlinear below 1m
		height = static_cast<uint16_t>(result);
		return true;
	}
}

size_t LidarLiteSensor::serialize(char* str, size_t n) const {
	if (paired) {
		return snprintf_P(str, n, PSTR("{\"paired\":true,\"serial\":%d,\"height\":%d}"), getSerial(), getHeight());
	}
	else {
		return snprintf_P(str, n, PSTR("{\"paired\":false}"));
	}
}

void LidarLiteBank::begin(void) {
	enterState_Waiting();
	for (uint8_t i = 0; i < NUM_SENSORS; i++) {
		sensors[i].begin(i);
	}
}

#define LIDARBANK_STATE__WAITING			0
#define LIDARBANK_STATE__SENSOR_POWER_CYCLE	1
#define LIDARBANK_STATE__CONFLICT_CHECK		2
#define LIDARBANK_STATE__ADDRESS_CONFLICT	3
#define LIDARBANK_STATE__NODE_STARTUP		4
#define LIDARBANK_STATE__NODE_PAIR			5
#define LIDARBANK_STATE__NODE_PAIR_DONE		6

#define CONFLICT_CHECK_DELAY					50 /* ms */
#define STARTUP_DELAY							50 /* ms */
#define ADDRESS_CONFLICT_RETRY_DELAY			1000 /* ms */


void LidarLiteBank::update(void) {	
	uint8_t newNumPaired = 0;
	switch (state) {
		case LIDARBANK_STATE__WAITING:
			for (int i = 0; i < NUM_SENSORS; i++) {
				if (sensors[i].paired) {
					newNumPaired++;
				}
			}
			numPaired = newNumPaired;
			if (numPaired != NUM_SENSORS) {
				enterState_SensorPowerCycle();
				LOG_STATE_TRANSITION("LidarLiteBank: Waiting->SensorPowerCycle");
			}
		break;
		case LIDARBANK_STATE__SENSOR_POWER_CYCLE:
			for (int i = 0; i < NUM_SENSORS; i++) {
				if (sensors[i].paired) {
					newNumPaired++;
				}
			}
			if (newNumPaired != numPaired) {
				numPaired = newNumPaired;
				enterState_SensorPowerCycle();
				LOG_STATE_TRANSITION("LidarLiteBank: SensorPowerCycle->SensorPowerCycle");
			}
			else if (timer.isUp()) {
				enterState_ConflictCheck();
				LOG_STATE_TRANSITION("LidarLiteBank: SensorPowerCycle->ConflictCheck");
				if (success) {
					enterState_AddressConflict();
					LOG_STATE_TRANSITION("LidarLiteBank: ConflictCheck->AddressConflict");
				}
				else {
					enterState_NodeStartup();
					LOG_STATE_TRANSITION("LidarLiteBank: ConflictCheck->NodeStartup (currentSensor = %d)", currentSensor);
				}
			}
		break;
		case LIDARBANK_STATE__ADDRESS_CONFLICT:
			if (timer.isUp()) {
				enterState_Waiting();
				LOG_STATE_TRANSITION("LidarLiteBank: AddressConflict->Waiting");
			}
		break;
		case LIDARBANK_STATE__NODE_STARTUP:
			if (timer.isUp()) {
				enterState_NodePair();
				LOG_STATE_TRANSITION("LidarLiteBank: NodeStartup->NodePair");
			}
		break;
		case LIDARBANK_STATE__NODE_PAIR:
			if (success) {
				enterState_NodePairDone();
				enterState_Waiting();
				LOG_STATE_TRANSITION("LidarLiteBank: NodePair->NodePairDone->Waiting (success)");
			}
			else {
				enterState_Waiting();
				LOG_STATE_TRANSITION("LidarLiteBank: NodePair->Waiting (failure)");
			}
		break;
		default:
			assert(0);
		break;
	}

	for (int i = 0; i < NUM_SENSORS; i++) {
		sensors[i].update();
	}
}

void LidarLiteBank::enterState_Waiting(void) {
	state = LIDARBANK_STATE__WAITING;
}

void LidarLiteBank::enterState_SensorPowerCycle(void) {
	// disable all unpaired nodes
	for (int i = 0; i < NUM_SENSORS; i++) {
		if (!sensors[i].paired) {
			pinMode(ENABLE_HARDLINE_START_PIN + i, OUTPUT);
			digitalWrite(ENABLE_HARDLINE_START_PIN + i, LOW);
		}
	}

	// set conflict check timer
	timer.restart(CONFLICT_CHECK_DELAY);

	state = LIDARBANK_STATE__SENSOR_POWER_CYCLE;
}

void LidarLiteBank::enterState_ConflictCheck(void) {
	// read STATUS register of slave 0x62
	int status = readRegister(LidarLiteSensor::DEFAULT_ADDRESS, STATUS);

	// set status (true/ACK for >= 0, false/NACK otherwise)
	success = status >= 0;

	state = LIDARBANK_STATE__CONFLICT_CHECK;
}

void LidarLiteBank::enterState_AddressConflict(void) {
	// log error
	LOG_ERROR("LidarLiteV3 I2C address conflict. This probably means a sensors' enable line is floating.");

	// set addrConflict flag
	addrConflict = true;

	// set retry timer
	timer.restart(ADDRESS_CONFLICT_RETRY_DELAY);

	// disable all nodes
	for (int i = 0; i < NUM_SENSORS; i++) {
		pinMode(ENABLE_HARDLINE_START_PIN + i, OUTPUT);
		digitalWrite(ENABLE_HARDLINE_START_PIN + i, LOW);
	}

	state = LIDARBANK_STATE__ADDRESS_CONFLICT;
}

void LidarLiteBank::enterState_NodeStartup(void) {
	// set currentSensor to an unpaired sensor
	do {
		currentSensor++;
		if (currentSensor >= NUM_SENSORS) {
			currentSensor = 0;
		}
	} while (sensors[currentSensor].paired);

	// enable currentSensor
	pinMode(ENABLE_HARDLINE_START_PIN + currentSensor, INPUT); // setting pin as high-impedance allows the sensor's internal pullup to drive it high

	// set startup timer
	timer.restart(STARTUP_DELAY);

	// unset addrConflict
	addrConflict = false;

	state = LIDARBANK_STATE__NODE_STARTUP;
}

void LidarLiteBank::enterState_NodePair(void) {
	// read serial number of slave 0x62 (DEFAULT_ADDRESS)
	int32_t serial = readAdjacentRegisters(LidarLiteSensor::DEFAULT_ADDRESS, UNIT_ID_HIGH);
	success = serial >= 0;

	// IF the sensor responded, write its serial number back to it, to unlock the I2C_SEC_ADDR register
	if (success) {
		success = writeAdjacentRegisters(LidarLiteSensor::DEFAULT_ADDRESS, I2C_ID_HIGH, static_cast<uint16_t>(serial));
	}
	// IF the sensor responded, set its secondary I2C address
	if (success) {
		success = writeRegister(LidarLiteSensor::DEFAULT_ADDRESS, I2C_SEC_ADDR, LIDAR_I2C_ID(currentSensor));

		// record serial number
		sensors[currentSensor].serial = static_cast<uint16_t>(serial);
	}

	state = LIDARBANK_STATE__NODE_PAIR;
}

void LidarLiteBank::enterState_NodePairDone(void) {
	// enable new address using 0x62 address
	writeRegister(LidarLiteSensor::DEFAULT_ADDRESS, I2C_CONFIG, I2C_CONFIG__ENABLE_NONDEFAULT_ADDRESS);
	// disable 0x62 address using new address
	if (writeRegister(LIDAR_I2C_ID(currentSensor), I2C_CONFIG,
			I2C_CONFIG__ENABLE_NONDEFAULT_ADDRESS | I2C_CONFIG__DISABLE_DEFAULT_ADDRESS)) {
		// IF ack received, mark sensor as paired
		sensors[currentSensor].paired = true;
	}

	state = LIDARBANK_STATE__NODE_PAIR_DONE;
}

size_t LidarLiteBank::serialize(char* str, size_t n) const {
	if (addrConflict) {
		return snprintf_P(str, n, PSTR("{\"hwError\":\"I2C address conflict - sensors cannot be identified. "
			"Check sensor wiring, particularly the enable lines.\",\"sensors\":[]}"));
	}

	size_t len = snprintf_P(str, n, PSTR("{\"sensors\": ["));
	for (int i = 0; i < NUM_SENSORS; i++) {
		len += sensors[i].serialize(str + len, len < n ? n - len : 0);
		if (len < n) {
			str[len] = (i + 1 == NUM_SENSORS) ? ']' : ',';
		}
		len++;
	}

	// add closing brace
	if (len < n) {
		str[len] = '}';
	}
	len++;

	// add null terminator
	if (len < n) {
		str[len] = '\0';
	}
	else if (n) {
		str[n - 1] = '\0';
	}
	return len;
}
