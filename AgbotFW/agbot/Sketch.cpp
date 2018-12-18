#include <Arduino.h>

#include "SerialApi.h"
#include "Config.h"
#include "Estop.h"
#include "Sprayer.h"
#include "Tiller.h"

void setup() {
	initConfig();
	initTillers();
	initSprayers();
	initSerialApi();
	pinMode(8, INPUT_PULLUP);
}

void loop() {
	readSerial();
	if (serialMessageAvailable()) {
		Serial.println(getSerialMessage());
	}
}