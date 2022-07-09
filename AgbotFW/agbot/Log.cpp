/*
 * Log.cpp
 *
 * Created: 2/28/2020 4:26:21 PM
 *  Author: troy.honegger
 */ 

#include "Log.h"

#include "Common.h"

static const char VERBOSE_PREFIX_FORMAT[] PROGMEM =	"%09ld VERBOSE ";
static const char DEBUG_PREFIX_FORMAT[] PROGMEM =	"%09ld DEBUG   ";
static const char INFO_PREFIX_FORMAT[] PROGMEM =	"%09ld INFO    ";
static const char WARNING_PREFIX_FORMAT[] PROGMEM =	"%09ld WARNING ";
static const char ERROR_PREFIX_FORMAT[] PROGMEM =	"%09ld ERROR   ";
static const char OFF_PREFIX_FORMAT[] PROGMEM =		"%09ld OFF     ";

Print* LogClass::stream = nullptr;
FILE* LogClass::file = nullptr;
LogClass Log;

int LogClass::streamPutc(char c, FILE *f) {
	static char prev = '\0';
	int ret = 0;
	if (c == '\n' && prev != '\r') {
		ret += stream->write('\r');
	}
	ret += stream->write(c);
	prev = c;
	return ret;
}

void LogClass::begin(Print* stream) {
	LogClass::stream = stream;
	LogClass::file = fdevopen(LogClass::streamPutc, nullptr);
}

void LogClass::printPrefix(uint8_t level) {
	const char* prefixFormat;
	switch (level) {
		case LOG_LEVEL_VERBOSE:
			prefixFormat = VERBOSE_PREFIX_FORMAT;
			break;
		case LOG_LEVEL_DEBUG:
			prefixFormat = DEBUG_PREFIX_FORMAT;
			break;
		case LOG_LEVEL_INFO:
			prefixFormat = INFO_PREFIX_FORMAT;
			break;
		case LOG_LEVEL_WARNING:
			prefixFormat = WARNING_PREFIX_FORMAT;
			break;
		case LOG_LEVEL_ERROR:
			prefixFormat = ERROR_PREFIX_FORMAT;
			break;
		default: // LOG_LEVEL_OFF:
			prefixFormat = OFF_PREFIX_FORMAT;
			break;
	}
	fprintf_P(file, prefixFormat, millis());
}

void LogClass::write_P(uint8_t level, const char* format, ...) {
	printPrefix(level);

	va_list args;
	va_start(args, format);
	vfprintf_P(file, format, args);
	va_end(args);
	fprintf_P(file, PSTR("\n"));
}

void LogClass::writeDetails_P(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf_P(file, format, args);
	va_end(args);
}
