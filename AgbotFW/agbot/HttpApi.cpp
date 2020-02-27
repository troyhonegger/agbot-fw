/*
 * HttpApi.cpp
 *
 * Created: 2/25/2020 3:38:32 PM
 *  Author: troy.honegger
 */ 

#include "Common.hpp"
#include "Devices.hpp"
#include "HttpApi.hpp"

#define PSTR_AND_LENGTH(s) PSTR(s), sizeof(s) - 1

#define SET_STATIC_CONTENT(resp, s) do { resp.content = PSTR(s); resp.contentLength = sizeof(s) - 1; resp.isContentInProgmem = true; } while (0)

// TODO: this approach assumes each response is sent before the next message comes in
static char responseHeaders[256] = {0};
static char responseBody[256] = {0};

// helper handlers called by httpHandler and its children
static HttpHandler webpageHandler;
static HttpHandler apiHandler;
static HttpHandler modeHandler;
static HttpHandler configHandler;
static HttpHandler gpsHandler;
static HttpHandler hitchHandler;
static HttpHandler tillerHandler;
static HttpHandler sprayerHandler;
static HttpHandler weedHandler;

static HttpHandler notImplementedHandler;
static HttpHandler notFoundHandler;
static HttpHandler methodNotAllowedHandler;

void httpHandler(HttpRequest const& request, HttpResponse& response) {
	*responseHeaders = '\0';
	*responseBody = '\0';

	if (!strncmp_P(request.uri, PSTR_AND_LENGTH("/api"))) {
		apiHandler(request, response);
	}
	else if (!strncmp_P(request.uri, PSTR_AND_LENGTH("/"))) {
		webpageHandler(request, response);
	}
	else {
		notFoundHandler(request, response);
	}
}

static void webpageHandler(HttpRequest const& request, HttpResponse& response) {
	notImplementedHandler(request, response);
}

static void apiHandler(HttpRequest const& request, HttpResponse& response) {
	if (!strncmp_P(request.uri, PSTR_AND_LENGTH("/api/weeds"))) {
		weedHandler(request, response);
	}
	else if (!strncmp_P(request.uri, PSTR_AND_LENGTH("/api/mode"))) {
		modeHandler(request, response);
	}
	else if (!strncmp_P(request.uri, PSTR_AND_LENGTH("/api/config"))) {
		configHandler(request, response);
	}
	else if (!strncmp_P(request.uri, PSTR_AND_LENGTH("/api/gps"))) {
		gpsHandler(request, response);
	}
	else if (!strncmp_P(request.uri, PSTR_AND_LENGTH("/api/tillers"))) {
		tillerHandler(request, response);
	}
	else if (!strncmp_P(request.uri, PSTR_AND_LENGTH("/api/sprayers"))) {
		sprayerHandler(request, response);
	}
	else if (!strncmp_P(request.uri, PSTR_AND_LENGTH("/api/hitch"))) {
		hitchHandler(request, response);
	}
	else {
		notFoundHandler(request, response);
	}
}

static void modeHandler(HttpRequest const& request, HttpResponse& response) {
	switch (request.method) {
		case HttpMethod::GET:
			response.version = HttpVersion::Http_11;
			response.responseCode = 200;
			strcpy_P(responseHeaders, PSTR("Content-Type: application/json"));
			response.headers[0].key = responseHeaders;
			response.headers[0].keyLen = 12;
			response.headers[0].value = &responseHeaders[14];
			response.headers[0].valueLen = 16;
			if (agbot::currentMode == agbot::MachineMode::Diag) {
				SET_STATIC_CONTENT(response, "{\"mode\": \"Diagnostics\"}");
			}
			else {
				SET_STATIC_CONTENT(response, "{\"mode\": \"Run\"}");
			}
		break;
		case HttpMethod::PUT: {
			response.version = HttpVersion::Http_11;
			response.responseCode = 204;

			agbot::MachineMode mode;
			if (request.contentLength && strstr_P(request.content, PSTR("Run"))) {
				mode = agbot::MachineMode::Run;
			}
			else if (request.contentLength && strstr_P(request.content, PSTR("Diagnostics"))) {
				mode = agbot::MachineMode::Diag;
			}
			else {
				response.responseCode = 400;
				SET_STATIC_CONTENT(response, "Invalid request - unknown machine mode.");
				return;
			}
			agbot::currentMode = mode;
			for (uint8_t i = 0; i < agbot::Sprayer::COUNT; i++) {
				agbot::sprayers[i].setMode(mode);
			}
		} break;
		default:
			methodNotAllowedHandler(request, response);
	}
}

static void configHandler(HttpRequest const& request, HttpResponse& response) {
	if (request.method != HttpMethod::GET && request.method != HttpMethod::PUT) {
		methodNotAllowedHandler(request, response);
	}
	else {
		const char* settingStr = request.uri + sizeof("/api/config") - 1;
		if (!*settingStr && request.method == HttpMethod::GET) {
			response.responseCode = 200;
			const char *format = PSTR("{\n"
				"\t\"HitchAccuracy\": %d,\n"
				"\t\"HitchLoweredHeight\": %d,\n"
				"\t\"HitchRaisedHeight\": %d,\n"
				"\t\"Precision\": %d,\n"
				"\t\"ResponseDelay\": %d\n"
				"\t\"TillerAccuracy\": %d\n"
				"\t\"TillerLoweredHeight\": %d\n"
				"\t\"TillerLowerTime\": %d\n"
				"\t\"TillerRaisedHeight\": %d\n"
				"\t\"TillerRaiseTime\": %d\n"
			"}");
			response.contentLength = snprintf_P(responseBody, sizeof(responseBody) - 1, format,
				agbot::config.get(agbot::Setting::HitchAccuracy),
				agbot::config.get(agbot::Setting::HitchLoweredHeight),
				agbot::config.get(agbot::Setting::HitchRaisedHeight),
				agbot::config.get(agbot::Setting::Precision),
				agbot::config.get(agbot::Setting::ResponseDelay),
				agbot::config.get(agbot::Setting::TillerAccuracy),
				agbot::config.get(agbot::Setting::TillerLoweredHeight),
				agbot::config.get(agbot::Setting::TillerLowerTime),
				agbot::config.get(agbot::Setting::TillerRaisedHeight),
				agbot::config.get(agbot::Setting::TillerRaiseTime));
			response.content = responseBody;
		}
		else {
			agbot::Setting setting;
			uint16_t minValue = 0;
			uint16_t maxValue = 0xFFFF;
			if (!strncmp_P(settingStr, PSTR_AND_LENGTH("/HitchAccuracy"))) {
				setting = agbot::Setting::HitchAccuracy;
				minValue = 0;
				maxValue = 100;
			}
			else if (!strncmp_P(settingStr, PSTR_AND_LENGTH("/HitchLoweredHeight"))) {
				setting = agbot::Setting::HitchLoweredHeight;
				minValue = 0;
				maxValue = 100;
			}
			else if (!strncmp_P(settingStr, PSTR_AND_LENGTH("/HitchRaisedHeight"))) {
				setting = agbot::Setting::HitchRaisedHeight;
				minValue = 0;
				maxValue = 100;
			}
			else if (!strncmp_P(settingStr, PSTR_AND_LENGTH("/Precision"))) {
				setting = agbot::Setting::Precision;
				minValue = 0;
				maxValue = 100;
			}
			else if (!strncmp_P(settingStr, PSTR_AND_LENGTH("/ResponseDelay"))) {
				setting = agbot::Setting::ResponseDelay;
				minValue = 0;
				maxValue = 0xFFFF;
			}
			else if (!strncmp_P(settingStr, PSTR_AND_LENGTH("/TillerAccuracy"))) {
				setting = agbot::Setting::TillerAccuracy;
				minValue = 0;
				maxValue = 100;
			}
			else if (!strncmp_P(settingStr, PSTR_AND_LENGTH("/TillerLoweredHeight"))) {
				setting = agbot::Setting::TillerLoweredHeight;
				minValue = 0;
				maxValue = 100;
			}
			else if (!strncmp_P(settingStr, PSTR_AND_LENGTH("/TillerLowerTime"))) {
				setting = agbot::Setting::TillerLowerTime;
				minValue = 0;
				maxValue = 0xFFFF;
			}
			else if (!strncmp_P(settingStr, PSTR_AND_LENGTH("/TillerRaisedHeight"))) {
				setting = agbot::Setting::TillerRaisedHeight;
				minValue = 0;
				maxValue = 100;
			}
			else if (!strncmp_P(settingStr, PSTR_AND_LENGTH("/TillerRaiseTime"))) {
				setting = agbot::Setting::TillerRaiseTime;
				minValue = 0;
				maxValue = 0xFFFF;
			}
			else {
				response.responseCode = 400;
				SET_STATIC_CONTENT(response, "Unknown configuration setting.");
				return;
			}
			if (request.method == HttpMethod::GET) {
				strcpy_P(responseHeaders, PSTR("Content-Type: application/json"));
				response.headers[0].key = responseHeaders;
				response.headers[0].keyLen = 12;
				response.headers[0].value = &responseHeaders[14];
				response.headers[0].valueLen = 16;
				response.contentLength = snprintf_P(responseBody, sizeof(responseBody) - 1, PSTR("%d"), agbot::config.get(setting));
				response.content = responseBody;
			}
			else { // request.method == HttpMethod::PUT
				long lval = atol(request.content);
				if (lval < minValue || lval > maxValue) {
					response.responseCode = 400;
					response.contentLength = snprintf_P(responseBody, sizeof(responseBody) - 1, PSTR("%s must be between %d and %d"), settingStr, minValue, maxValue);
					response.content = responseBody;
				}
				else {
					response.responseCode = 204;
				}
			}
		}
	}
}

static void gpsHandler(HttpRequest const& request, HttpResponse& response) {
	notImplementedHandler(request, response);
}

static void hitchHandler(HttpRequest const& request, HttpResponse& response) {
	notImplementedHandler(request, response);
}

static void tillerHandler(HttpRequest const& request, HttpResponse& response) {
	notImplementedHandler(request, response);
}

static void sprayerHandler(HttpRequest const& request, HttpResponse& response) {
	notImplementedHandler(request, response);
}

static void weedHandler(HttpRequest const& request, HttpResponse& response) {
	notImplementedHandler(request, response);
}


static void notImplementedHandler(HttpRequest const& request, HttpResponse& response) {
	response.version = HttpVersion::Http_11;
	response.responseCode = 501;
	SET_STATIC_CONTENT(response, "Endpoint not implemented");
}

static void notFoundHandler(HttpRequest const& request, HttpResponse& response) {
	response.version = HttpVersion::Http_11;
	response.responseCode = 404;
	SET_STATIC_CONTENT(response, "Requested resource not found.");
}

static void methodNotAllowedHandler(HttpRequest const& request, HttpResponse& response) {
	response.version = HttpVersion::Http_11;
	response.responseCode = 405;
	SET_STATIC_CONTENT(response, "Method not allowed");
}
