#include <Arduino.h>

#include "Common.hpp"
#include "Devices.hpp"
#include "Http.hpp"
#include "HttpApi.hpp"
#include "Log.hpp"

#include <string.h>
#include <SparkFun_Ublox_Arduino_Library.h>

Estop estop;
Config config;
Hitch hitch;
Tiller tillers[Tiller::COUNT];
Sprayer sprayers[Sprayer::COUNT];
Throttle throttle;
LidarLiteBank heightSensors;

EthernetServer ethernetSrvr(80);
HttpServer server(ethernetSrvr, 4, httpHandler);

#ifndef BENCH_TESTS

void setup() {
#if LOG_LEVEL != LOG_LEVEL_OFF
	Serial.begin(115200);
	Log.begin(&Serial);
#endif

	LOG_DEBUG("Beginning setup...");

	config.begin();
	hitch.begin(&config);
	for (uint8_t i = 0; i < Tiller::COUNT; i++) {
		tillers[i].begin(i, &config);
	}
	for (uint8_t i = 0; i < Sprayer::COUNT; i++) {
		sprayers[i].begin(i, &config);
	}
	throttle.begin();

	uint8_t mac[6] = { 0xA8, 0x61, 0x0A, 0xAE, 0x11, 0xF6 };
	uint8_t controllerIP[4] = {172, 21, 2, 1};//TODO - should be { 192, 168, 4, 2 };
	Ethernet.begin(mac, controllerIP);
	// adjust these two settings to taste
	Ethernet.setRetransmissionCount(3);
	Ethernet.setRetransmissionTimeout(150);
	server.begin();

	Wire.begin();
	Wire.setClock(400000L);
	heightSensors.begin();

	estop.begin();

	LOG_INFO("Setup complete.");
}


//#define TIMING_ANALYSIS
#ifdef TIMING_ANALYSIS
static uint32_t starttime;
static uint32_t lastprint;
static uint32_t ncycles;
// counts measures response times on a log scale.
static uint32_t counts[33];
#endif

void loop() {
#ifdef TIMING_ANALYSIS
	if (!starttime) { // should run on first loop only
		starttime = micros();
		if (!lastprint) {
			lastprint = starttime;
		}
	}
#endif

	server.serve();
	
	estop.update();

	hitch.getActualHeight();
	if (hitch.needsUpdate()) { hitch.update(); }
	for (uint8_t i = 0; i < Tiller::COUNT; i++) { tillers[i].update(); }
	for (uint8_t i = 0; i < Sprayer::COUNT; i++) { sprayers[i].update(); }

	// throttle up if the hitch or any tillers are moving (but NOT if the sprayers or the
	// clutch are active - they shouldn't suck too much power)
	int8_t throttleUp = hitch.getDH();
	for (uint8_t i = 0; i < Tiller::COUNT; i++) { throttleUp |= tillers[i].getDH(); }
	if (throttleUp) { throttle.up(); }
	else { throttle.down(); }
	throttle.update();

	heightSensors.update();

#ifdef TIMING_ANALYSIS
	{
		ncycles++;
		uint32_t time = micros() - starttime;
		starttime += time;
		uint8_t i = 0;
		while (time) { // compute ceil(log2(time)) for logging
			time >>= 1;
			i++;
		}
		counts[i]++;
		if (starttime - lastprint >= 1000000UL) {
			// get median cycle time by computing a cumulative sum until n = ncycles/2
			uint32_t n;
			for (i = 0, n = 0; n < (ncycles >> 1); i++) {
				n += counts[i];
			}
			// get max cycle time by scanning counts from the back
			uint8_t maxi;
			for (maxi = 32; maxi && !counts[maxi]; maxi--);
			LOG_INFO("Cycles: %ldHz; max 2^%dus, median 2^%dus", ncycles, maxi, i);

			memset(counts, 0, sizeof(counts));
			lastprint = starttime;
			ncycles = 0;
		}
	}
#endif
}

#endif // BENCH_TESTS
