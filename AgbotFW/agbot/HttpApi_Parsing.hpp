/*
 * HttpApi_Parsing.h
 *
 * Created: 12/17/2021 10:36:41 PM
 *  Author: troy.honegger
 */ 

#pragma once

#include <Arduino.h>

enum ParseStatus : uint8_t {
	SUCCESS,
	SYNTAX_ERROR,
	BUFFER_OVERFLOW,
	SEMANTIC_ERROR
};

struct PutTiller {
	uint8_t targetHeight;
	uint32_t delay;
};

ParseStatus parsePutTillerCmd(char* jsonString, size_t n, struct PutTiller& result);

struct PutSprayer {
	bool status;
	uint32_t delay;
};

ParseStatus parsePutSprayerCmd(char* jsonString, size_t n, struct PutSprayer& result);


#define HITCH_CMD__DOWN	248
#define HITCH_CMD__UP	249
#define HITCH_CMD__STOP	250

struct PutHitch {
	uint8_t targetHeight;
};

ParseStatus parsePutHitchCmd(char* jsonString, size_t n, struct PutHitch& result);
