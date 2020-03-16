/*
 * Log.h
 *
 * Created: 2/28/2020 3:59:51 PM
 *  Author: troy.honegger
 */ 


#pragma once

#include "Common.hpp"

#define LOG_LEVEL_VERBOSE		1
#define LOG_LEVEL_DEBUG			2
#define LOG_LEVEL_INFO			3
#define LOG_LEVEL_WARNING		4
#define LOG_LEVEL_ERROR			5
#define LOG_LEVEL_OFF			6

#if defined(LOGGING_VERBOSE)
#define LOG_LEVEL			LOG_LEVEL_VERBOSE
#elif defined(LOGGING_DEBUG)
#define LOG_LEVEL			LOG_LEVEL_DEBUG
#elif defined(LOGGING_INFO)
#define LOG_LEVEL			LOG_LEVEL_INFO
#elif defined(LOGGING_WARNING)
#define LOG_LEVEL			LOG_LEVEL_WARNING
#elif defined(LOGGING_ERROR)
#define LOG_LEVEL			LOG_LEVEL_ERROR
#elif defined(LOGGING_OFF)
#define LOG_LEVEL			LOG_LEVEL_OFF
#elif defined(DEBUG)
#define LOG_LEVEL			LOG_LEVEL_INFO
#else
#define LOG_LEVEL			LOG_LEVEL_OFF
#endif

class LogClass {
	static Print* stream;
	static FILE* file;
	static int streamPutc(char, FILE*);
	void printPrefix(uint8_t level);

	// disable copying
	LogClass(LogClass const&);
	void operator=(LogClass const&);
public:
	LogClass() {}

	void begin(Print* stream);
	void write_P(uint8_t level, const char* format, ...);
	void writeDetails_P(const char* format, ...);
};

extern LogClass Log;

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(format, ...)			Log.write_P(LOG_LEVEL_ERROR, PSTR(format), __VA_ARGS__)
#else
#define LOG_ERROR(format, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARNING
#define LOG_WARNING(format, ...)		Log.write_P(LOG_LEVEL_WARNING, PSTR(format), __VA_ARGS__)
#else
#define LOG_WARNING(format, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(format, ...)			Log.write_P(LOG_LEVEL_INFO, PSTR(format), ## __VA_ARGS__)
#else
#define LOG_INFO(format, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(format, ...)			Log.write_P(LOG_LEVEL_DEBUG, PSTR(format), ## __VA_ARGS__)
#else
#define LOG_DEBUG(format, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_VERBOSE
#define LOG_VERBOSE(format, ...)		Log.write_P(LOG_LEVEL_VERBOSE, PSTR(format), ## __VA_ARGS__)
#else
#define LOG_VERBOSE(format, ...)
#endif
