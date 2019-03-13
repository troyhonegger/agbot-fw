/*
 * TillAndSprayDemo.cpp
 * This was created as a simple test of the Arduino's serial capabilities. To use it, be sure to call Serial.begin() and displayMenu()
 * in your setup() code and tillAndSprayDemoLoop() inside your loop() function. Then you can simply connect to the Arduino with PuTTY
 * or a similar serial terminal and follow the menu item commands to send simple commands to control the tillers and sprayers.
 * Created: 12/17/2018 9:54:17 PM
 *  Author: troy.honegger
 */ 

#include <Arduino.h>

#include "Common.hpp"
#include "Config.hpp"
#include "Sprayer.hpp"
#include "Tiller.hpp"

#ifdef DEMO_MODE

extern agbot::Config config;
extern agbot::Tiller tillers[];
extern agbot::Sprayer sprayers[];

static bool sprayersInDiagMode = false;
static bool sprayersInProcessMode = false;
static bool tillersInDiagMode = false;
static bool tillersInProcessMode = false;

void displayMenu(void) {
	Serial.print(F("Select an option:\r\n"
				   "\t0. Sprayers enter diag mode\r\n"
				   "\t1. Tillers enter diag mode\r\n"
				   "\t2. Sprayers enter process mode\r\n"
				   "\t3. Tillers enter process mode\r\n"
				   "\t4. Schedule sprayer spray (all sprayers)\r\n"
				   "\t5. Schedule tiller lower (all tillers)\r\n"
				   "\t6. Sprayer diag ON (all sprayers)\r\n"
				   "\t7. Sprayer diag OFF (all sprayers)\r\n"
				   "\t8. Tiller diag RAISE (all tillers)\r\n"
				   "\t9. Tiller diag LOWER (all tillers)\r\n"
				   "\ta. Tiller diag STOP (all tillers)\r\n"
				   "\tb. Tiller diag target 50% (all tillers)\r\n"
				   "\tc. Display time\r\n"
				   "\th. Display menu\r\n\r\n"));
}

void setup(void) {
	config.begin();
	config.set(agbot::Setting::Precision, 500);
	config.set(agbot::Setting::ResponseDelay, 1500);
	config.set(agbot::Setting::TillerLowerTime, 500);
	config.set(agbot::Setting::TillerRaiseTime, 0);
	config.set(agbot::Setting::TillerAccuracy, 5);
	config.set(agbot::Setting::TillerRaisedHeight, 100);
	config.set(agbot::Setting::TillerLoweredHeight, 0);
	config.set(agbot::Setting::KeepAliveTimeout, 65535);
	config.set(agbot::Setting::HitchAccuracy, 5);
	config.set(agbot::Setting::HitchRaisedHeight, 100);
	config.set(agbot::Setting::HitchLoweredHeight, 0);
	//estop.begin();
	//hitch.begin(&config);
	for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
		tillers[i].begin(i, &config);
	}
	for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) {
		sprayers[i].begin(i, &config);
	}
	Serial.begin(9600);
	displayMenu();
}

void loop(void) {
	if (Serial.available()) {
		switch (Serial.read()) {
			case -1: // No character available
				break;
			case '0': // Sprayers enter diag mode
				sprayersInDiagMode = true;
				sprayersInProcessMode = false;
				for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) {
					sprayers[i].setMode(agbot::MachineMode::Diag);
				}
				Serial.print(F("0. Sprayers are now in diag mode\r\n"));
				break;
			case '1': // Tillers enter diag mode
				tillersInDiagMode = true;
				tillersInProcessMode = false;
				for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
					tillers[i].setMode(agbot::MachineMode::Diag);
				}
				Serial.print(F("1. Tillers are now in diag mode\r\n"));
				break;
			case '2': // Sprayers enter process mode
				sprayersInDiagMode = false;
				sprayersInProcessMode = true;
				for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) {
					sprayers[i].setMode(agbot::MachineMode::Process);
				}
				Serial.print(F("2. Sprayers are now in process mode\r\n"));
				break;
			case '3': // Tillers enter process mode
				tillersInDiagMode = false;
				tillersInProcessMode = true;
				for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
					tillers[i].setMode(agbot::MachineMode::Process);
				}
				Serial.print(F("3. Tillers are now in process mode\r\n"));
				break;
			case '4': // Schedule sprayer spray
				if (sprayersInProcessMode) {
					for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) {
						sprayers[i].scheduleSpray();
					}
					Serial.print(F("4. Spray scheduled\r\n"));
				}
				else Serial.print(F("Invalid command - sprayers are not in process mode\r\n"));
				break;
			case '5': // Schedule tiller lower
				if (tillersInProcessMode) {
					for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
						tillers[i].scheduleLower();
					}
					Serial.print(F("5. Tiller lower scheduled\r\n"));
				}
				else Serial.print(F("Invalid command - tillers are not in process mode\r\n"));
				break;
			case '6': // Sprayer diag on
				if (sprayersInDiagMode) {
					for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) {
						sprayers[i].setStatus(agbot::Sprayer::ON);
					}
					Serial.print(F("6. Sprayers on\r\n"));
				}
				else Serial.print(F("Invalid command - sprayers are not in diag mode\r\n"));
				break;
			case '7': // Sprayer diag off
				if (sprayersInDiagMode) {
					for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) {
						sprayers[i].setStatus(agbot::Sprayer::OFF);
					}
					Serial.print(F("7. Sprayers off\r\n"));
				}
				else Serial.print(F("Invalid command - sprayers are not in diag mode\r\n"));
				break;
			case '8': // Tiller diag raise
				if (tillersInDiagMode) {
					for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
						tillers[i].setTargetHeight(100);
					}
					Serial.print(F("8. Tillers raising\r\n"));
				}
				else Serial.print(F("Invalid command - tillers are not in diag mode\r\n"));
				break;
			case '9': // Tiller diag lower
				if (tillersInDiagMode) {
					for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
						tillers[i].setTargetHeight(0);
					}
					Serial.print(F("9. Tillers lowering\r\n"));
				}
				else Serial.print(F("Invalid command - tillers are not in diag mode\r\n"));
				break;
			case 'a': // Tiller diag stop
			case 'A':
				if (tillersInDiagMode) {
					for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
						tillers[i].setTargetHeight(agbot::Tiller::STOP);
					}
					Serial.print(F("a. Tillers stopped\r\n"));
				}
				else Serial.print(F("Invalid command - tillers are not in diag mode\r\n"));
				break;
			case 'b': // Tiller diag target 50%
			case 'B':
				if (tillersInDiagMode) {
					for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
						tillers[i].setTargetHeight(50);
					}
					Serial.print(F("b. Tillers set to target 50% height\r\n"));
				}
				else Serial.print(F("Invalid command - tillers are not in diag mode\r\n"));
				break;
			case 'c': // display time
			case 'C':
				Serial.print(F("c. Time in milliseconds is: "));
				Serial.println(millis());
				break;
			case 'h':
				displayMenu();
				break;
			default:
				Serial.print(F("Unrecognized command: press 'h' to display menu.\r\n"));
				break;
		}
	}
	for (uint8_t i = 0; i < agbot::Tiller::COUNT; i++) {
		tillers[i].update();
	}
	for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) {
		sprayers[i].update();
	}
}

#endif // DEMO_MODE