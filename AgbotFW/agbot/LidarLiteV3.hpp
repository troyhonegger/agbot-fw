/*
 * LidarLiteV3.hpp
 *
 * For state machine documentation see LidarLiteV3_State_Machine.drawio
 *
 * Manages the Lidar Lite laser distance sensors, used for height sensors on the BOT.
 * The LidarLite's are controlled via the I2C bus. Control logic is split across 2 classes:
 *
 * class LidarLiteSensor controls an individual sensor once it is paired (associated with
 *   a specific I2C address and location on the BOT). LidarLiteSensor has a state machine that
 *   polls the sensor regularly for readings. The poll delay is currently set at 10ms
 *   for an update frequency of 100Hz. (See READ_DELAY in LidarLiteV3.cpp).
 * class LidarLiteBank encapsulates all the sensors, and handles pairing (associating sensors
 *   with their physical locations on the BOT, and assigning their I2C addresses). It
 *   uses the sensors' enable hardlines to select one sensor at a time, pair it, and pass it
 *   off to the corresponding LidarLiteSensor. This also allows the sensors to be power-cycled
 *   at any time: LidarLiteBank dynamically reconnects to them once they come back up.
 *
 * Call LidarLiteBank::begin() on startup. This automatically calls begin() on all LidarLiteSensor's.
 * Then call LidarLiteBank::update() to run the control logic. If a sensor is paired (i.e.
 * lidarLiteBank[i].isPaired() returns true), call lidarLiteBank[i].getHeight()
 * to grab the latest measurement.
 *
 * The Lidar Lite hardware specification itself is very well documented in the datasheet:
 * https://static.garmin.com/pumac/LIDAR_Lite_v3_Operation_Manual_and_Technical_Specifications.pdf
 *
 * WARNING: The I2C control logic is blocking I/O; this is an inherent limitation of the Wire library.
 * So, the runtime of update() is roughly proportional to the number of bytes sent over I2C.
 * Therefore, for optimal responsiveness, this implementation tries to send as little as possible over I2C,
 * and breaks large read/write operations into several cycles.
 *
 * During pairing, up to 12 bytes may be sent in one cycle, to pair a single sensor. Once paired,
 * a sensor sends at most a 5-byte message in any given cycle. Therefore, up to 22 bytes may be sent
 * by the LidarLite controller in one cycle. (12 bytes to pair one sensor, and 5 bytes each if the other
 * two perform a read at the same time).
 *
 * The LidarLite sensors support both I2C fast mode (400kHz) and standard mode (100kHz). If possible,
 * I recommend using fast mode. To set fast mode, just call Wire.setClock(400000L). The worst-case
 * runtime of LidarLiteBank::update() should be around 2ms with standard mode, or 0.5ms with fast mode.
 *
 * Created: 3/14/2020 5:05:16 PM
 *  Author: troy.honegger
 */

#pragma once

#include "Common.hpp"

class LidarLiteSensor {
	public:
	static const uint8_t DEFAULT_ADDRESS = 0x62; // defined by hardware. Initial I2C address of a LidarLite upon power up
	static const uint8_t START_ADDRESS = 0x70; // arbitrary. Sensors are numbered 0x70, 0x72, 0x74. Entire range must be less than 0x7F, and must not conflict with DEFAULT_ADDRESS

	private:
	Timer timer;
	uint16_t serial;
	uint16_t height;
	uint8_t id;
	uint8_t state;
	uint8_t numReadings;
	bool paired;

	// state machine functions
	void enterState_Unpaired(void);
	bool enterState_Configuring(void); // returns true for success; false for error
	void enterState_WaitForRead(void);
	bool enterState_Read(void); // returns true for success; false for error

	// disallow copy constructor
	void operator =(LidarLiteSensor const&) {}
	LidarLiteSensor(LidarLiteSensor const& other) { }

	public:
	LidarLiteSensor() : serial(0), height(0), id(0), state(0), numReadings(0), paired(false) { }

	// Returns true if the sensor is connected and configured. This should happen within hundreds of milliseconds
	// after either the sensor or the controller is power cycled.
	inline bool isPaired(void) const { return paired; }
	// (If paired) returns the unique serial number of the sensor, assigned by the manufacturer.
	inline uint16_t getSerial(void) const { return serial; }
	// (If paired) returns the last retrieved height in centimeters.
	inline uint16_t getHeight(void) const { return height; }

	// Initializes the class. Call before calling any other member functions.
	void begin(uint8_t id);
	// Runs the state machine. Call every iteration of the main controller loop.
	void update(void);

	// Writes the information pertaining to this height sensor to the given string. Writes at most n characters, including the null
	// terminator, and returns the number of characters in the serialized string, excluding the null terminator. If the string length
	// exceeds n, returns the number of characters that would have been in the string, were there enough space.
	size_t serialize(char* str, size_t n) const;

	friend class LidarLiteBank;
};

class LidarLiteBank {
	public:
	static const uint8_t NUM_SENSORS = 3; // number of LidarLite sensors on the machine.
	static const uint8_t ENABLE_HARDLINE_START_PIN = 22; // enable pins are 22, 23, 24

	private:
	LidarLiteSensor sensors[NUM_SENSORS];

	// state machine variables
	Timer timer;
	uint8_t numPaired;
	uint8_t currentSensor;
	uint8_t state;
	bool success;
	bool addrConflict;

	// state machine functions
	void enterState_Waiting(void);
	void enterState_SensorPowerCycle(void);
	void enterState_ConflictCheck(void);
	void enterState_AddressConflict(void);
	void enterState_NodeStartup(void);
	void enterState_NodePair(void);
	void enterState_NodePairDone(void);

	// disallow copy constructor
	void operator =(LidarLiteBank const&) {}
	LidarLiteBank(LidarLiteBank const& other) { }

	public:
	LidarLiteBank() : numPaired(0) { }
		
	// returns the i'th LidarLiteSensor. Defined so a LidarLiteBank acts like an array; i.e. you can say
	//   LidarLiteBank sensors;
	//   sensors[1].getHeight();
	LidarLiteSensor& operator[](int i) { return sensors[i]; }

	// Initializes the class. Call before calling any other member functions.
	void begin(void);
	// Runs the state machine. Call every iteration of the main controller loop.
	void update(void);
	
	// Writes the information pertaining to the height sensors to the given string. Writes at most n characters, including the null
	// terminator, and returns the number of characters in the serialized string, excluding the null terminator. If the string length
	// exceeds n, returns the number of characters that would have been in the string, were there enough space.
	size_t serialize(char* str, size_t n) const;
};

