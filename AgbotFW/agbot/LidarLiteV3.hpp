/*
 * LidarLiteV3.hpp
 *
 * Encapsulates a Lidar Lite laser distance sensor, used for height sensors on the BOT.
 * The LidarLite's are controlled via the I2C bus. Call update() repeatedly so the object
 * can poll the LidarLite for height readings, in a (more or less) non-blocking manner.
 * Call getHeight() to grab the latest measurement.
 *
 * Each call to update() should take roughly a millisecond at maximum.
 *
 * The Lidar Lite hardware specification itself is very well documented in the datasheet:
 * https://static.garmin.com/pumac/LIDAR_Lite_v3_Operation_Manual_and_Technical_Specifications.pdf
 *
 * A note on Auto ID: The Lidar Lite sensors themselves are equipped with mechanisms that allow
 * a form of AutoID. It cannot be fully automatic as I2C is a many-to-many bus - there is no
 * way to know how the sensors are ordered. That said, the process is still reasonably simple,
 * and works like this:
 *  - The first time a new sensor is added, power up the Arduino and plug in all sensors one
 *    at a time, LEFT TO RIGHT. The order is critically important. The Arduino will connect to each
 *    one in turn and store their unique serial numbers
 *  - If the sensors have already been configured, the Arduino will remember their serial numbers.
 *    The next time the system is power cycled, things should initialize without an issue.
 *  - If a Lidar Lite is unplugged, it will NOT (as of now) auto-reconnect when you plug it back in.
 *    You will have to power cycle the Arduino.
 *
 * Created: 3/14/2020 5:05:16 PM
 *  Author: troy.honegger
 */

#pragma once

#include "Common.hpp"

// TODO: would like to include a config setting for update speed in Hz. This may
// require some more architecture in place, to define target platform at compile time.
// This may also require some mechanism to "spread the load" across different sensors.
// For example, if the update frequency is 100Hz, we want each of the sensors to update
// themselves at slightly different times over any 10ms interval, to improve
// responsiveness elsewhere in the system

class LidarLiteV3 {
	static const uint8_t DEFAULT_ADDRESS = 0x62; // defined by hardware. Initial I2C address of a LidarLite upon power up
	static const uint8_t START_ADDRESS = 0x70; // arbitrary. Sensors are numbered 0x70, 0x71, 0x72. Entire range must be less than 0x7F, and must not conflict with DEFAULT_ADDRESS
	// hardware-specific responsibilities:
	// upon boot-up, get the serial number (from somewhere).
	// configure the sensor (set the I2C ID, set up for continuous measurements)
	// poll for changes periodically (ideally have a means to share time with the other sensors, to evenly distribute the load)
	uint16_t serial;
	uint16_t height;
	uint8_t id;
	uint8_t state;
	uint8_t numReadings;

	public:
	LidarLiteV3() : serial(0), height(0), id(0), state(0), numReadings(0) { }
	inline uint16_t getSerial(void) { return serial; }
	void begin(uint8_t id);
	bool isInitialized(void);
	void update(void);

	// If initialized, return the last retrieved height in centimeters. Otherwise, return 0.
	inline uint16_t getHeight(void) { return height; }
};

class LidarLiteBank {
	public:
	static const uint8_t NUM_SENSORS = 3;
	LidarLiteV3 sensors[NUM_SENSORS];

	private:
	uint8_t numInitialized;

	public:
	LidarLiteBank() : numInitialized(0) { }
	void begin(void);
	void update(void);
};

