/*
 * HttpApi.cpp
 *
 * Created: 2/25/2020 3:38:32 PM
 *  Author: troy.honegger
 */ 

#include "Common.hpp"
#include "Devices.hpp"
#include "HttpApi.hpp"
#include "HttpApi_Parsing.hpp"
#include "Log.hpp"

#define PSTR_AND_LENGTH(s) PSTR(s), sizeof(s) - 1

#define SET_STATIC_CONTENT(resp, s) do { resp.content = PSTR(s); resp.contentLength = sizeof(s) - 1; resp.isContentInProgmem = true; } while (0)

#define MIN(a,b) (((a) < (b)) ? (a) : (b))

static const char CONTENT_TYPE__APPLICATION_JSON[] PROGMEM = "Content-Type: application/json\r\n";
static const char CONTENT_TYPE__TEXT_PLAIN[] PROGMEM = "Content-Type: text/plain\r\n";

//TODO: want to replace atoi() calls with strtol() or similar

// TODO: this approach assumes each response is sent before the next message comes in
static char responseHeaders[256] = {0};
static char responseBody[256] = {0};

// helper handlers called by httpHandler and its children
static HttpHandler webpageHandler;
static HttpHandler versionHandler;
static HttpHandler apiHandler;
static HttpHandler configHandler;
static HttpHandler gpsHandler;
static HttpHandler hitchHandler;
static HttpHandler tillerHandler;
static HttpHandler sprayerHandler;
static HttpHandler weedHandler;
static HttpHandler heightSensorsHandler;

static HttpHandler notImplementedHandler;
static HttpHandler notFoundHandler;
static HttpHandler methodNotAllowedHandler;

static void handleParseError(HttpRequest const& request, HttpResponse& response, ParseStatus error);

void httpHandler(HttpRequest const& request, HttpResponse& response) {
	*responseHeaders = '\0';
	*responseBody = '\0';
	response.headers = responseHeaders;
	response.headersLength = 0;

	if (!strncmp_P(request.uri, PSTR_AND_LENGTH("/api"))) {
		apiHandler(request, response);
	}
	else if (!strncmp_P(request.uri, PSTR_AND_LENGTH("/version"))) {
		versionHandler(request, response);
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

static void versionHandler(HttpRequest const& request, HttpResponse& response) {
	if (request.method == HttpMethod::GET) {
		response.version = HttpVersion::Http_11;
		response.responseCode = 200;
		memccpy_P(responseHeaders + response.headersLength, CONTENT_TYPE__TEXT_PLAIN,
					'\0', sizeof(responseHeaders) - response.headersLength);
		response.headersLength = MIN(sizeof(responseHeaders), response.headersLength + sizeof(CONTENT_TYPE__TEXT_PLAIN) - 1);

		response.content =         PSTR(AGBOTFW_VERSION " - built " __DATE__ " " __TIME__);
		response.contentLength = sizeof(AGBOTFW_VERSION " - built " __DATE__ " " __TIME__) - 1;
		response.isContentInProgmem = true;
	}
	else {
		methodNotAllowedHandler(request, response);
	}
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
	else if (!strncmp_P(request.uri, PSTR_AND_LENGTH("/api/heightSensors"))) {
		heightSensorsHandler(request, response);
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
		response.version = HttpVersion::Http_11;
		const char* settingStr = request.uri + sizeof("/api/config") - 1;
		if ((!*settingStr || *settingStr == '?') && request.method == HttpMethod::GET) {
			response.responseCode = 200;
			memccpy_P(responseHeaders + response.headersLength, CONTENT_TYPE__APPLICATION_JSON,
						'\0', sizeof(responseHeaders) - response.headersLength);
			response.headersLength = MIN(sizeof(responseHeaders), response.headersLength + sizeof(CONTENT_TYPE__APPLICATION_JSON) - 1);
			const char *format = PSTR("{\n"
				"\t\"HitchAccuracy\": %u,\n"
				"\t\"HitchLoweredHeight\": %u,\n"
				"\t\"HitchRaisedHeight\": %u,\n"
				"\t\"Precision\": %u,\n"
				"\t\"ResponseDelay\": %u,\n"
				"\t\"TillerAccuracy\": %u,\n"
				"\t\"TillerLoweredHeight\": %u,\n"
				"\t\"TillerLowerTime\": %u,\n"
				"\t\"TillerRaisedHeight\": %u,\n"
				"\t\"TillerRaiseTime\": %u\n"
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
				response.responseCode = 200;
				memccpy_P(responseHeaders + response.headersLength, CONTENT_TYPE__APPLICATION_JSON,
							'\0', sizeof(responseHeaders) - response.headersLength);
				response.headersLength = MIN(sizeof(responseHeaders), response.headersLength + sizeof(CONTENT_TYPE__APPLICATION_JSON) - 1);
				response.contentLength = snprintf_P(responseBody, sizeof(responseBody) - 1, PSTR("%u"), config.get(setting));
				response.content = responseBody;
			}
			else { // request.method == HttpMethod::PUT
				char *endPtr;
				long lval = strtol(request.content, &endPtr, 10);
				if (!*request.content || *endPtr || lval < (long) minValue || lval > (long) maxValue) {
					response.responseCode = 400;
					response.contentLength = snprintf_P(responseBody, sizeof(responseBody) - 1, PSTR("%s must be an integer between %u and %u, not \"%s\""), settingStr, minValue, maxValue, request.content);
					response.content = responseBody;
				}
				else {
					response.responseCode = 204;
					config.set(setting, (uint16_t) lval);
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
	response.version = HttpVersion::Http_11;
	char* idStr = request.uri + sizeof("/api/tillers") - 1;
	switch (request.method) {
		case HttpMethod::GET: {
			if (*idStr && *++idStr) {
				int id = atoi(idStr);
				if (id < 0 || id >= Tiller::COUNT) {
					response.responseCode = 400;
					response.contentLength = snprintf_P(responseBody, sizeof(responseBody) - 1, PSTR("id must be between 0 and %d - was '%s'"), Tiller::COUNT - 1, idStr);
					response.content = responseBody;
				}
				else {
					memccpy_P(responseHeaders + response.headersLength, CONTENT_TYPE__APPLICATION_JSON,
								'\0', sizeof(responseHeaders) - response.headersLength);
					response.headersLength = MIN(sizeof(responseHeaders), response.headersLength + sizeof(CONTENT_TYPE__APPLICATION_JSON) - 1);
					response.contentLength = tillers[id].serialize(responseBody, sizeof(responseBody));
					response.content = responseBody;
				}
			}
			else {
				memccpy_P(responseHeaders + response.headersLength, CONTENT_TYPE__APPLICATION_JSON,
							'\0', sizeof(responseHeaders) - response.headersLength);
				response.headersLength = MIN(sizeof(responseHeaders), response.headersLength + sizeof(CONTENT_TYPE__APPLICATION_JSON) - 1);


				size_t len = 1;
				*responseBody = '['; // opening array element
				for (int i = 0; i < Tiller::COUNT; i++) {
					if (len < sizeof(responseBody) - 1) {
						len += tillers[i].serialize(responseBody + len, sizeof(responseBody) - len);

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
			int id = -1;
			if (*idStr && *++idStr) {
				id = atoi(idStr);
				if (id < 0 || id >= Tiller::COUNT) {
					response.responseCode = 400;
					response.contentLength = snprintf_P(responseBody, sizeof(responseBody) - 1, PSTR("id must be between 0 and %d - was '%s'"), Tiller::COUNT - 1, idStr);
					response.content = responseBody;
					return;
				}
			}

			PutTiller tillerCommand;
			ParseStatus result = parsePutTillerCmd(request.content, request.contentLength, tillerCommand);
			if (result == ParseStatus::SUCCESS) {
				// actually execute the command
				if (id >= 0) {
					tillers[id].setHeight(tillerCommand.targetHeight, tillerCommand.delay);
				}
				else {
					// do this for all tillers
					for (id = 0; id < Tiller::COUNT; id++) {
						tillers[id].setHeight(tillerCommand.targetHeight, tillerCommand.delay);
					}
				}
				// send a 204 No Content to indicate success
				response.responseCode = 204;
				response.contentLength = 0;
				response.content = nullptr;
			}
			else {
				handleParseError(request, response, result);
			}
		} break;
		default: methodNotAllowedHandler(request, response);
	}
}

static void sprayerHandler(HttpRequest const& request, HttpResponse& response) {
	response.version = HttpVersion::Http_11;
	char *idStr = request.uri + sizeof("/api/sprayers") - 1;
	switch (request.method) {
		case HttpMethod::GET: {
			if (*idStr && *++idStr) {
				int id = atoi(idStr);
				if (id < 0 || id >= Sprayer::COUNT) {
					response.responseCode = 400;
					response.contentLength = snprintf_P(responseBody, sizeof(responseBody) - 1, PSTR("id must be between 0 and %d - was '%s'"), Sprayer::COUNT - 1, idStr);
					response.content = responseBody;
					return;
				}
				response.responseCode = 200;
				memccpy_P(responseHeaders + response.headersLength, CONTENT_TYPE__APPLICATION_JSON,
							'\0', sizeof(responseHeaders) - response.headersLength);
				response.headersLength = MIN(sizeof(responseHeaders), response.headersLength + sizeof(CONTENT_TYPE__APPLICATION_JSON) - 1);
				response.contentLength = sprayers[id].serialize(responseBody, sizeof(responseBody));
				response.content = responseBody;
			}
			else {
				response.responseCode = 200;
				memccpy_P(responseHeaders + response.headersLength, CONTENT_TYPE__APPLICATION_JSON,
							'\0', sizeof(responseHeaders) - response.headersLength);
				response.headersLength = MIN(sizeof(responseHeaders), response.headersLength + sizeof(CONTENT_TYPE__APPLICATION_JSON) - 1);

				size_t len = 1;
				*responseBody = '['; // opening array element
				for (int i = 0; i < Sprayer::COUNT; i++) {
					if (len < sizeof(responseBody) - 1) {
						len += sprayers[i].serialize(responseBody + len, sizeof(responseBody) - len);

						if (len < sizeof(responseBody) - 1 && i + 1 < Sprayer::COUNT) {
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
			// first parse the {id} URL param:
				// 0:7 means the ID was given as a number; i.e. set that particular ID
				// -1 means no ID given (i.e. set all sprayers)
				// -2 means {id} == "left" (i.e. sprayers 0-3)
				// -3 means {id} == "right" (i.e. sprayers 4-7)
			int id = -1;
			if (*idStr && *++idStr) {
				if (!strncmp_P(idStr, PSTR_AND_LENGTH("left"))) {
					id = -2;
				}
				else if (!strncmp_P(idStr, PSTR_AND_LENGTH("right"))) {
					id = -3;
				}
				else {
					id = atoi(idStr);
					if (id < 0 || id >= Sprayer::COUNT) {
						response.responseCode = 400;
						response.contentLength = snprintf_P(responseBody, sizeof(responseBody) - 1, PSTR("id must be \"left\", \"right\", or between 0 and %d - was '%s'"), Sprayer::COUNT - 1, idStr);
						response.content = responseBody;
						return;
					}
				}
			}
			PutSprayer sprayerCommand;
			ParseStatus result = parsePutSprayerCmd(request.content, request.contentLength, sprayerCommand);
			if (result == ParseStatus::SUCCESS) {
				if (id >= 0) {
					sprayers[id].setStatus(sprayerCommand.status, sprayerCommand.delay);
				}
				else {
					uint8_t end = (id == -2 ? Sprayer::COUNT >> 1 : Sprayer::COUNT);
					for (uint8_t i = (id == -3 ? Sprayer::COUNT >> 1 : 0); i < end; i++) {
						sprayers[i].setStatus(sprayerCommand.status, sprayerCommand.delay);
					}
				}
				response.responseCode = 204;
				response.contentLength = 0;
				response.content = nullptr;
			}
			else {
				handleParseError(request, response, result);
			}
		} break;
		default: methodNotAllowedHandler(request, response);
	}
}

static inline uint8_t parseHex(char nibble) {
	if ('0' <= nibble && nibble <= '9') {
		return nibble - '0';
	}
	else if ('A' <= nibble && nibble <= 'F') {
		return nibble - 'A' + 10;
	}
	else if ('a' <= nibble && nibble <= 'f') {
		return nibble - 'a' + 10;
	}
	return 255;
}
static void weedHandler(HttpRequest const& request, HttpResponse& response) {
	if (request.method != HttpMethod::POST) {
		methodNotAllowedHandler(request, response);
		return;
	}
	// TODO consider returning "409 Conflict" if the hitch is up. Would need to document this decision
	response.version = HttpVersion::Http_11;
	char* cmdStr = request.uri + sizeof("/api/weeds") - 1;
	bool cmdValid = *cmdStr == '/' && strlen(++cmdStr) == 5;
	if (cmdValid) {
		for (int i = 0; i < 5; i++) {
			if (parseHex(cmdStr[i]) > 0x0F) {
				cmdValid = false;
				break;
			}
		}
	}

	if (!cmdValid) {
		response.responseCode = 400;
		response.contentLength = snprintf_P(responseBody, sizeof(responseBody) - 1, PSTR("Expected 5-character hex string in URL, not '%s'"), cmdStr);
		response.content = responseBody;
		return;
	}
	else {
		for (int i = 0; i < Tiller::COUNT; i++) {
			if (parseHex(cmdStr[i << 1])) {
				// tiller i is at index 2*i in the command string. If it is not '0', we should lower the tiller to kill whatever's in the row
				tillers[i].killWeed();
			}
		}
		uint8_t sprayerCmd = parseHex(cmdStr[1]) | (parseHex(cmdStr[3]) << 4);
		for (int i = 0; i < Sprayer::COUNT; i++) {
			if (sprayerCmd & (1 << i)) {
				sprayers[i].killWeed();
			}
		}
		response.responseCode = 204;
		response.contentLength = 0;
		response.content = nullptr;
	}
}

static void heightSensorsHandler(HttpRequest const& request, HttpResponse& response) {
	if (request.method != HttpMethod::GET) {
		methodNotAllowedHandler(request, response);
	}
	else {
		response.version = HttpVersion::Http_11;
		response.responseCode = 200;
		memccpy_P(responseHeaders + response.headersLength, CONTENT_TYPE__APPLICATION_JSON,
					'\0', sizeof(responseHeaders) - response.headersLength);
		response.headersLength = MIN(sizeof(responseHeaders), response.headersLength + sizeof(CONTENT_TYPE__APPLICATION_JSON) - 1);

		response.headers = responseHeaders;
		response.contentLength = heightSensors.serialize(responseBody, sizeof(responseBody));
		response.content = responseBody;
	}
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

static void handleParseError(HttpRequest const& request, HttpResponse& response, ParseStatus error) {
	response.version = HttpVersion::Http_11;
	response.responseCode = 400;
	switch (error) {
		case ParseStatus::SYNTAX_ERROR:
			SET_STATIC_CONTENT(response, "Malformed JSON in request body");
			break;
		case ParseStatus::BUFFER_OVERFLOW:
			SET_STATIC_CONTENT(response, "Too many JSON tokens");
			break;
		case ParseStatus::SEMANTIC_ERROR:
			SET_STATIC_CONTENT(response, "Invalid JSON request");
			break;
		default:
			assert(0);
	}
}
