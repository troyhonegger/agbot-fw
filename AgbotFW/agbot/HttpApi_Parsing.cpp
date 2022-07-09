/*
 * HttpApi_Parsing.cpp
 *
 * Created: 12/17/2021 10:36:25 PM
 *  Author: troy.honegger
 */ 

#include "HttpApi_Parsing.h"

#define JSMN_HEADER
#define JSMN_PARENT_LINKS
#include "jsmn.h"

#define NUM_JSON_TOKENS 12
static jsmn_parser jsonParser;
static jsmntok_t jsonBuffer[NUM_JSON_TOKENS];

#include "Tiller.h"
#include "Hitch.h"

// TODO watch out - this leads to a separate PSTR allocation for each call. May be wasteful if you compare against the same string in multiple places.
#define TOKEN_IS(json, tok, s) ((tok).type == JSMN_STRING && (tok).end - (tok).start == sizeof(s) - 1 && !strncmp_P((json) + (tok).start, PSTR(s), sizeof(s) - 1))

ParseStatus parsePutTillerCmd(char* jsonString, size_t n, struct PutTiller& result) {
	jsmn_init(&jsonParser);
	int nTok = jsmn_parse(&jsonParser, jsonString, n, jsonBuffer, NUM_JSON_TOKENS);
	
	if (nTok < 0) {
		switch (nTok) {
			case JSMN_ERROR_NOMEM:
				return ParseStatus::BUFFER_OVERFLOW;
			default: // case JSMN_ERROR_INVAL: case JSMN_ERROR_PART:
				return ParseStatus::SYNTAX_ERROR;
		}
	}
	else if (jsonBuffer[0].type != JSMN_OBJECT) {
		return ParseStatus::SEMANTIC_ERROR;
	}
	
	bool hasTargetHeight = false;
	bool hasDelay = false;
	result.delay = 0;

	for (int i = 1; i < nTok - 1; i++) {
		jsmntok_t* tok = jsonBuffer + i;
		if (tok->parent > 0) {
			continue; // skip past any child objects, etc. We only care about direct children of the root
		}

		if (TOKEN_IS(jsonString, *tok, "targetHeight")) {
			if (hasTargetHeight) {
				return ParseStatus::SEMANTIC_ERROR;
			}
			tok++;
			i++;
			hasTargetHeight = true;
			if (tok->type == JSMN_PRIMITIVE && *(jsonString + tok->start) >= '0' && *(jsonString + tok->start) <= '9') {
				// jsonString is not null-terminated, but atoi() is safe here, as the string must be valid JSON ending with '}',
				// and atoi stops at the first non-numeric character
				int cmd = atoi(jsonString + tok->start);
				if (cmd < 0 || cmd > Tiller::MAX_HEIGHT) {
					return ParseStatus::SEMANTIC_ERROR;
				}
				result.targetHeight = static_cast<uint8_t>(cmd);
			}
			else if (TOKEN_IS(jsonString, *tok, "STOP")) {
				result.targetHeight = static_cast<uint8_t>(TillerCommand::STOP);
			}
			else if (TOKEN_IS(jsonString, *tok, "UP")) {
				result.targetHeight = static_cast<uint8_t>(TillerCommand::UP);
			}
			else if (TOKEN_IS(jsonString, *tok, "DOWN")) {
				result.targetHeight = static_cast<uint8_t>(TillerCommand::DOWN);
			}
			else if (TOKEN_IS(jsonString, *tok, "LOWERED")) {
				result.targetHeight = static_cast<uint8_t>(TillerCommand::LOWERED);
			}
			else if (TOKEN_IS(jsonString, *tok, "RAISED")) {
				result.targetHeight = static_cast<uint8_t>(TillerCommand::RAISED);
			}
			else {
				return ParseStatus::SEMANTIC_ERROR;
			}
		}
		else if (TOKEN_IS(jsonString, *tok, "delay")) {
			if (hasDelay) {
				return ParseStatus::SEMANTIC_ERROR;
			}
			tok++;
			i++;
			hasDelay = true;
			if (tok->type == JSMN_PRIMITIVE && *(jsonString + tok->start) >= '0' && *(jsonString + tok->start) <= '9') {
				// jsonString is not null-terminated, but atol() is safe here, as the string must be valid JSON ending with '}',
				// and atol stops at the first non-numeric character
				result.delay = atol(jsonString + tok->start);
			}
			else {
				return ParseStatus::SEMANTIC_ERROR;
			}
		}
	}

	return hasTargetHeight ? ParseStatus::SUCCESS : ParseStatus::SEMANTIC_ERROR;
}

ParseStatus parsePutSprayerCmd(char* jsonString, size_t n, struct PutSprayer& result) {
	jsmn_init(&jsonParser);
	int nTok = jsmn_parse(&jsonParser, jsonString, n, jsonBuffer, NUM_JSON_TOKENS);
	
	if (nTok < 0) {
		switch (nTok) {
			case JSMN_ERROR_NOMEM:
			return ParseStatus::BUFFER_OVERFLOW;
			default: // case JSMN_ERROR_INVAL: case JSMN_ERROR_PART
			return ParseStatus::SYNTAX_ERROR;
		}
	}
	else if (jsonBuffer[0].type != JSMN_OBJECT) {
		return ParseStatus::SEMANTIC_ERROR;
	}
	
	bool hasStatus = false;
	bool hasDelay = false;
	result.delay = 0;
	
	for (int i = 1; i < nTok - 1; i++) {
		jsmntok_t* tok = jsonBuffer + i;
		if (tok->parent > 0) {
			continue; // skip past any child objects, etc. We only care about direct children of the root
		}
		
		if (TOKEN_IS(jsonString, *tok, "status")) {
			if (hasStatus) {
				return ParseStatus::SEMANTIC_ERROR;
			}
			tok++;
			i++;
			hasStatus = true;
			if (TOKEN_IS(jsonString, *tok, "ON")) {
				result.status = true;
			}
			else if (TOKEN_IS(jsonString, *tok, "OFF")) {
				result.status = false;
			}
			else {
				return ParseStatus::SEMANTIC_ERROR;
			}
		}
		else if (TOKEN_IS(jsonString, *tok, "delay")) {
			if (hasDelay) {
				return ParseStatus::SEMANTIC_ERROR;
			}
			tok++;
			i++;
			hasDelay = true;
			if (tok->type == JSMN_PRIMITIVE && *(jsonString + tok->start) >= '0' && *(jsonString + tok->start) <= '9') {
				// jsonString is not null-terminated, but atol() is safe here, as the string must be valid JSON ending with '}',
				// and atol stops at the first non-numeric character
				result.delay = atol(jsonString + tok->start);
			}
			else {
				return ParseStatus::SEMANTIC_ERROR;
			}
		}
	}
	
	return hasStatus ? ParseStatus::SUCCESS : ParseStatus::SEMANTIC_ERROR;
}

ParseStatus parsePutHitchCmd(char* jsonString, size_t n, struct PutHitch& result) {
	jsmn_init(&jsonParser);
	int nTok = jsmn_parse(&jsonParser, jsonString, n, jsonBuffer, NUM_JSON_TOKENS);

	if (nTok < 0) {
		switch (nTok) {
			case JSMN_ERROR_NOMEM:
			return ParseStatus::BUFFER_OVERFLOW;
			default: // case JSMN_ERROR_INVAL: case JSMN_ERROR_PART:
			return ParseStatus::SYNTAX_ERROR;
		}
	}
	else if (jsonBuffer[0].type != JSMN_OBJECT) {
		return ParseStatus::SEMANTIC_ERROR;
	}

	bool hasTargetHeight = false;

	for (int i = 1; i < nTok - 1; i++) {
		jsmntok_t* tok = jsonBuffer + i;
		if (tok->parent > 0) {
			continue; // skip past any child objects, etc. We only care about direct children of the root
		}

		if (TOKEN_IS(jsonString, *tok, "targetHeight")) {
			if (hasTargetHeight) {
				return ParseStatus::SEMANTIC_ERROR;
			}
			tok++;
			i++;
			hasTargetHeight = true;
			if (tok->type == JSMN_PRIMITIVE && *(jsonString + tok->start) >= '0' && *(jsonString + tok->start) <= '9') {
				// jsonString is not null-terminated, but atoi() is safe here, as the string must be valid JSON ending with '}',
				// and atoi stops at the first non-numeric character
				int cmd = atoi(jsonString + tok->start);
				if (cmd < 0 || cmd > Hitch::MAX_HEIGHT) {
					return ParseStatus::SEMANTIC_ERROR;
				}
				result.targetHeight = static_cast<uint8_t>(cmd);
			}
			else if (TOKEN_IS(jsonString, *tok, "STOP")) {
				result.targetHeight = HITCH_CMD__STOP;
			}
			else if (TOKEN_IS(jsonString, *tok, "UP")) {
				result.targetHeight = HITCH_CMD__UP;
			}
			else if (TOKEN_IS(jsonString, *tok, "DOWN")) {
				result.targetHeight = HITCH_CMD__DOWN;
			}
			else {
				return ParseStatus::SEMANTIC_ERROR;
			}
		}
	}

	return hasTargetHeight ? ParseStatus::SUCCESS : ParseStatus::SEMANTIC_ERROR;
}
