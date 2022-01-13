/*
 * Devices.hpp
 * Declares objects for controlling high-level device functionality.
 * Created: 2/25/2020 2:52:06 PM
 *  Author: troy.honegger
 */ 


#pragma once

#include "Common.hpp"
#include "Config.hpp"
#include "Hitch.hpp"
#include "Tiller.hpp"
#include "Sprayer.hpp"
#include "Throttle.hpp"
#include "LidarLiteV3.hpp"

extern Config config;
extern Hitch hitch;
extern Tiller tillers[Tiller::COUNT];
extern Sprayer sprayers[Sprayer::COUNT];
extern Throttle throttle;
extern LidarLiteBank heightSensors;

