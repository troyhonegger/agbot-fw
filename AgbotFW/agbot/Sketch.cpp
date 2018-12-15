﻿#include <Arduino.h>

#include "SerialApi.h"
#include "Config.h"
#include "Estop.h"
#include "Sprayer.h"
#include "Tiller.h"

void displayMenu(void);

void setup() {
	initConfig();
	initTillers();
	initSprayers();
	//initSerialApi();
	setTotalDelay(1250);
	setPrecision(500);
	setTillerAccuracy(10);
	pinMode(8, INPUT_PULLUP);
	Serial.begin(9600);
	displayMenu();
}

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

static bool sprayersInDiagMode = false;
static bool sprayersInProcessMode = false;
static bool tillersInDiagMode = false;
static bool tillersInProcessMode = false;

void loop() {
	if (Serial.available()) {
		switch (Serial.read()) {
			case -1: // No character available
				break;
			case '0': // Sprayers enter diag mode
				sprayersInDiagMode = true;
				sprayersInProcessMode = false;
				sprayersEnterDiagMode();
				Serial.print(F("0. Sprayers are now in diag mode\r\n"));
				break;
			case '1': // Tillers enter diag mode
				tillersInDiagMode = true;
				tillersInProcessMode = false;
				tillersEnterDiagMode();
				Serial.print(F("1. Tillers are now in diag mode\r\n"));
				break;
			case '2': // Sprayers enter process mode
				sprayersInDiagMode = false;
				sprayersInProcessMode = true;
				sprayersEnterProcessMode();
				Serial.print(F("2. Sprayers are now in process mode\r\n"));
				break;
			case '3': // Tillers enter process mode
				tillersInDiagMode = false;
				tillersInProcessMode = true;
				tillersEnterProcessMode();
				Serial.print(F("3. Tillers are now in process mode\r\n"));
				break;
			case '4': // Schedule sprayer spray
				if (sprayersInProcessMode) {
					scheduleSpray(255);
					Serial.print(F("4. Spray scheduled\r\n"));
				}
				else Serial.print(F("Invalid command - sprayers are not in process mode\r\n"));
				break;
			case '5': // Schedule tiller lower
				if (tillersInProcessMode) {
					scheduleTillerLower(7);
					Serial.print(F("5. Tiller lower scheduled\r\n"));
				}
				else Serial.print(F("Invalid command - tillers are not in process mode\r\n"));
				break;
			case '6': // Sprayer diag on
				if (sprayersInDiagMode) {
					diagSetSprayer(255, SPRAYER_ON);
					Serial.print(F("6. Sprayers on\r\n"));
				}
				else Serial.print(F("Invalid command - sprayers are not in diag mode\r\n"));
				break;
			case '7': // Sprayer diag off
				if (sprayersInDiagMode) {
					diagSetSprayer(255, SPRAYER_OFF);
					Serial.print(F("7. Sprayers off\r\n"));
				}
				else Serial.print(F("Invalid command - sprayers are not in diag mode\r\n"));
				break;
			case '8': // Tiller diag raise
				if (tillersInDiagMode) {
					diagRaiseTiller(255);
					Serial.print(F("8. Tillers raising\r\n"));
				}
				else Serial.print(F("Invalid command - tillers are not in diag mode\r\n"));
				break;
			case '9': // Tiller diag lower
				if (tillersInDiagMode) {
					diagLowerTiller(255);
					Serial.print(F("9. Tillers lowering\r\n"));
				}
				else Serial.print(F("Invalid command - tillers are not in diag mode\r\n"));
				break;
			case 'a': // Tiller diag stop
			case 'A':
				if (tillersInDiagMode) {
					diagStopTiller(255);
					Serial.print(F("a. Tillers stopped\r\n"));
				}
				else Serial.print(F("Invalid command - tillers are not in diag mode\r\n"));
				break;
			case 'b': // Tiller diag target 50%
			case 'B':
				if (tillersInDiagMode) {
					diagSetTiller(7, 50);
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
	if (digitalRead(8) == LOW && tillersInProcessMode)
		scheduleTillerLower(7);
	updateTillers();
	updateSprayers();
}