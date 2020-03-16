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
static HttpHandler configHandler;
static HttpHandler gpsHandler;
static HttpHandler hitchHandler;
static HttpHandler tillerHandler;
static HttpHandler sprayerHandler;
static HttpHandler weedHandler;

static HttpHandler notImplementedHandler;
static HttpHandler notFoundHandler;
static HttpHandler methodNotAllowedHandler;
static HttpHandler malformedJsonHandler;

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


static void configHandler(HttpRequest const& request, HttpResponse& response) {
	if (request.method != HttpMethod::GET && request.method != HttpMethod::PUT) {
		methodNotAllowedHandler(request, response);
	}
	else {
		const char* settingStr = request.uri + sizeof("/api/config") - 1;
		if ((!*settingStr || *settingStr == '?') && request.method == HttpMethod::GET) {
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
				config.get(Setting::HitchAccuracy),
				config.get(Setting::HitchLoweredHeight),
				config.get(Setting::HitchRaisedHeight),
				config.get(Setting::Precision),
				config.get(Setting::ResponseDelay),
				config.get(Setting::TillerAccuracy),
				config.get(Setting::TillerLoweredHeight),
				config.get(Setting::TillerLowerTime),
				config.get(Setting::TillerRaisedHeight),
				config.get(Setting::TillerRaiseTime));
			response.content = responseBody;
		}
		else if (*settingStr == '/') {
			settingStr++;
			// ignore the query string if necessary to make sure the URI path is null-terminated
			char *query = strchr(settingStr, '?');
			if (query) {
				*query = '\0';
			}
			Setting setting;

			uint16_t minValue;
			uint16_t maxValue;
			if (stringToSetting(settingStr, setting)) {
				minValue = minSettingValue(setting);
				maxValue = maxSettingValue(setting);
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
				response.contentLength = snprintf_P(responseBody, sizeof(responseBody) - 1, PSTR("%d"), config.get(setting));
				response.content = responseBody;
			}
			else { // request.method == HttpMethod::PUT
				long lval = atol(request.content);
				if (lval < minValue || lval > maxValue) {
					response.responseCode = 400;
					response.contentLength = snprintf_P(responseBody, sizeof(responseBody) - 1, PSTR("%s must be between %d and %d"), settingStr + 1, minValue, maxValue);
					response.content = responseBody;
				}
				else {
					response.responseCode = 204;
				}
			}
		}
		else {
			notFoundHandler(request, response);
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
	switch (request.method) {
		case HttpMethod::GET: {
			char* idStr = request.uri + sizeof("/api/tillers");
			if (*idStr && *++idStr) {
				int id = atoi(idStr);
				if (id < 0 || id >= Tiller::COUNT) {
					response.responseCode = 400;
					response.contentLength = snprintf_P(responseBody, sizeof(responseBody) - 1, PSTR("id must be between 0 and %d - was '%s'"), Tiller::COUNT, idStr);
					response.content = responseBody;
				}
				else {
					strcpy_P(responseHeaders, PSTR("Content-Type: application/json"));
					response.headers[0].key = responseHeaders;
					response.headers[0].keyLen = 12;
					response.headers[0].value = &responseHeaders[14];
					response.headers[0].valueLen = 16;
					response.contentLength = tillers[id].serialize(responseBody, sizeof(responseBody) - 1);
					response.content = responseBody;
				}
			}
			else {
				strcpy_P(responseHeaders, PSTR("Content-Type: application/json"));
				response.headers[0].key = responseHeaders;
				response.headers[0].keyLen = 12;
				response.headers[0].value = &responseHeaders[14];
				response.headers[0].valueLen = 16;

				size_t len = 1;
				*responseBody = '['; // opening array element
				for (int i = 0; i < Tiller::COUNT; i++) {
					if (len < sizeof(responseBody) - 1) {
						len += tillers[i].serialize(responseBody + len, sizeof(responseBody) - 1 - len);

						if (len < sizeof(responseBody) - 1 && i + 1 < Tiller::COUNT) {
							responseBody[len++] = ',';
						}
					}
				}
				if (len < sizeof(responseBody) - 1) {
					responseBody[len++] = ']';
				}
				responseBody[len] = '\0';

				response.contentLength = len;
				response.content = responseBody;
			}
		} break;
		case HttpMethod::PUT: {
			notImplementedHandler(request, response); //TODO
		} break;
		default: methodNotAllowedHandler(request, response);
	}
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

static void malformedJsonHandler(HttpRequest const& request, HttpResponse& response) {
	response.version = HttpVersion::Http_11;
	response.responseCode = 400;
	SET_STATIC_CONTENT(response, "Malformed JSON in request body");
}
