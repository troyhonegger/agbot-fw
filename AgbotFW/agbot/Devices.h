/*
 * Devices.h
 * Declares objects for controlling high-level device functionality.
 * Created: 2/25/2020 2:52:06 PM
 *  Author: troy.honegger
 */ 


#pragma once

#include "Common.h"
#include "Config.h"
#include "Hitch.h"
#include "Tiller.h"
#include "Sprayer.h"
#include "Throttle.h"
#include "LidarLiteV3.h"
#include "Estop.h"

extern Estop estop;
extern Config config;
extern Hitch hitch;
extern Tiller tillers[Tiller::COUNT];
extern Sprayer sprayers[Sprayer::COUNT];
extern Throttle throttle;
extern LidarLiteBank heightSensors;

