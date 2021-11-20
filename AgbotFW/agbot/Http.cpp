#include "Http.hpp"

#include "Common.hpp"
#include "Log.hpp"

typedef long ssize_t; // for some reason Arduino doesn't have this defined by default?

#define HTTPCLIENT_STATUS_DISCONNECTED	(0)
#define HTTPCLIENT_STATUS_READING		(32)
#define HTTPCLIENT_STATUS_RCVD			(64)

// Reading states MUST be between 33 and 63
#define HTTPCLIENT_READING_METHOD		(33)
#define HTTPCLIENT_READING_VERSION		(34)
#define HTTPCLIENT_READING_URI			(35)
#define HTTPCLIENT_READING_HEADER_KEY	(36)
#define HTTPCLIENT_READING_HEADER_START	(37)
#define HTTPCLIENT_READING_HEADER_VALUE	(38)
#define HTTPCLIENT_READING_BODY			(39)

// Received states MUST be between 65 and 95
#define HTTPCLIENT_RCVD_REQUEST			(65)
#define HTTPCLIENT_RCVD_BAD_METHOD		(66)
#define HTTPCLIENT_RCVD_BAD_VERSION		(67)
#define HTTPCLIENT_RCVD_URI_TOO_LONG	(68)
#define HTTPCLIENT_RCVD_HDRS_TOO_LONG	(69)
#define HTTPCLIENT_RCVD_BODY_TOO_LONG	(70)


#define HTTP_METHOD_MAX_LEN				(7)
#define HTTP_VERSION_MAX_LEN			(8)

// TODO duplication of identical definition in EthernetApi.cpp
#define MAKE_CONST_STR_WITH_LEN(token) static const char token##_STR[] PROGMEM = #token; static const uint8_t token##_STR_LEN = strlen_P(token##_STR)

static const char HTTP_10_STR[] PROGMEM = "HTTP/1.0";
static const uint8_t HTTP_10_STR_LEN = 8;

static const char HTTP_11_STR[] PROGMEM = "HTTP/1.1";
static const uint8_t HTTP_11_STR_LEN = 8;

static const char CRLF_STR[] PROGMEM = "\r\n";
static const uint8_t CRLF_STR_LEN = 2;

static const char CONTENT_LENGTH_STR[] PROGMEM = "Content-Length";
static const uint8_t CONTENT_LENGTH_STR_LEN = 14;

MAKE_CONST_STR_WITH_LEN(OPTIONS);
MAKE_CONST_STR_WITH_LEN(GET);
MAKE_CONST_STR_WITH_LEN(HEAD);
MAKE_CONST_STR_WITH_LEN(POST);
MAKE_CONST_STR_WITH_LEN(PUT);
MAKE_CONST_STR_WITH_LEN(DELETE);
MAKE_CONST_STR_WITH_LEN(PATCH);

static size_t writePgm(Print& print, PGM_P buf, size_t bufLen) {
	size_t n = 0;
	for (unsigned char c; bufLen; bufLen--) {
		c = pgm_read_byte(buf++);
		if (print.write(c)) {
			n++;
		}
		else {
			break;
		}
	}
	return n;
}

static void resetConnection(HttpConnection&);

HttpServer::HttpServer(EthernetServer& server, uint8_t maxConnections, HttpHandler* handler)
		: maxConnections(maxConnections), server(server), handler(handler) {
	// limit maxConnections to HTTP_MAX_CONNECTIONS
	if (maxConnections > HTTP_MAX_CONNECTIONS) {
		HttpServer::maxConnections = HTTP_MAX_CONNECTIONS;
	}
	for (int i = 0; i < maxConnections; i++) {
		resetConnection(connections[i]);
	}
}

// ASSUMES Ethernet has already been initialized by the application level
void HttpServer::begin(void) {
	server.begin();
}

// reads up to bufferLen - 1 bytes into buf, either from connection.leftovers (if present)
// or the Ethernet client (otherwise). Always null-terminates the input.
static size_t read(EthernetClient& client, HttpConnection& connection, char* buf, size_t bufferLen) {
	size_t numRead = 0;
	if (connection.leftovers) {
		numRead = connection.leftoversLength;
		if (numRead > bufferLen - 1) {
			numRead = bufferLen - 1;
		}
		memcpy(buf, connection.leftovers, numRead);
		buf[numRead] = '\0';
		if (numRead == connection.leftoversLength) {
			connection.leftovers = nullptr;
			connection.leftoversLength = 0;
		}
		else {
			connection.leftovers += numRead;
			connection.leftoversLength -= numRead;
		}
	}
	else {
		numRead = client.read((uint8_t*) buf, bufferLen - 1);
		buf[numRead] = '\0';
	}
	return numRead;
}

// ASSUMES: extra points inside the string last returned by read(), for this connection
static void unread(HttpConnection& connection, char *extra, size_t extraLen) {
	if (connection.leftovers) {
		// extra points to a copy of memory stored immediately before leftovers.
		connection.leftovers -= extraLen;
		connection.leftoversLength += extraLen;
	}
	else {
		connection.leftovers = extra;
		connection.leftoversLength = extraLen;
	}
}

// character-by-character copy. Useful for shifting things in memory when src and dest may overlap.
static void copyInPlace(char* dest, char *src, size_t length) {
	for (; length; length--) {
		*dest++ = *src++;
	}
}

static bool parseHttpMethod(const char* str, HttpMethod& method) {
	if (!strncmp_P(str, GET_STR, GET_STR_LEN)) {
		method = HttpMethod::GET;
		return true;
	}
	else if (!strncmp_P(str, POST_STR, POST_STR_LEN)) {
		method = HttpMethod::POST;
		return true;
	}
	else if (!strncmp_P(str, PUT_STR, PUT_STR_LEN)) {
		method = HttpMethod::PUT;
		return true;
	}
	else if (!strncmp_P(str, DELETE_STR, DELETE_STR_LEN)) {
		method = HttpMethod::DELETE;
		return true;
	}
	else if (!strncmp_P(str, HEAD_STR, HEAD_STR_LEN)) {
		method = HttpMethod::HEAD;
		return true;
	}
	else if (!strncmp_P(str, PATCH_STR, PATCH_STR_LEN)) {
		method = HttpMethod::PATCH;
		return true;
	}
	else if (!strncmp_P(str, OPTIONS_STR, OPTIONS_STR_LEN)) {
		method = HttpMethod::OPTIONS;
		return true;
	}
	else {
		return false;
	}
}

static bool parseHttpVersion(const char* str, HttpVersion& version) {
	if (!strncmp_P(str, HTTP_10_STR, HTTP_10_STR_LEN)) {
		version = HttpVersion::Http_10;
		return true;
	}
	else if (!strncmp_P(str, HTTP_11_STR, HTTP_11_STR_LEN)) {
		version = HttpVersion::Http_11;
		return true;
	}
	else {
		return false;
	}
}

static char* strnchr(char* str, char c, size_t n) {
	if (!str) {
		return nullptr;
	}
	for (; n > 0 && *str; n--) {
		if (*str == c) {
			return str;
		}
		str++;
	}
	return nullptr;
}

static char* strchrs_2(char* str, char c1, char c2) {
	if (!str) {
		return nullptr;
	}
	while (*str) {
		if (*str == c1 || *str == c2) {
			return str;
		}
		str++;
	}
	return nullptr;
}

// returns: TRUE to stop parsing and wait for more data to come in; FALSE to try serving the state machine again
static bool parseClient_ReadingMethod(EthernetClient& client, HttpConnection& connection) {
	// NOTE: the literal text of the HTTP method is not stored; only an HttpMethod enum. For parsing,
	// the method is temporarily stored in the requestHeaders string, to save space.

	// no data to read - try again later
	if (!connection.leftovers && !client.available()) {
		return true;
	}
	ssize_t bufferLen = HTTP_METHOD_MAX_LEN + 2 - connection.readPosition; // space for the worst-case string ("OPTIONS ") incl zero byte
	if (bufferLen <= 1) { // need bufferLen > 1 so there is room for more data + null byte
		connection.state = HTTPCLIENT_RCVD_BAD_METHOD;
		return false;
	}
	size_t amountRead = read(client, connection, connection.requestHeaders + connection.readPosition, bufferLen);
	// search for a whitespace character signifying end of this field
	char* endPosn = strchrs_2(connection.requestHeaders + connection.readPosition, ' ', '\t');
	if (endPosn) {
		// Store anything after the field delimiter as leftovers to be parsed later
		if (endPosn + 1 < connection.requestHeaders + connection.readPosition + amountRead) {
			unread(connection, endPosn + 1, connection.requestHeaders + connection.readPosition + amountRead - (endPosn + 1));
		}
		*endPosn = '\0';
		if (parseHttpMethod(connection.requestHeaders, connection.request.method)) {
			connection.state = HTTPCLIENT_READING_URI;
		}
		else {
			connection.state = HTTPCLIENT_RCVD_BAD_METHOD;
		}
		memset(connection.requestHeaders, 0, endPosn - connection.requestHeaders);
		connection.readPosition = 0;
		connection.parsePosition = 0;
	}
	else {
		connection.readPosition += amountRead;
	}
	return false;
}

// returns: TRUE to stop parsing and wait for more data to come in; FALSE to try serving the state machine again
static bool parseClient_ReadingUri(EthernetClient& client, HttpConnection& connection) {
	if (!connection.leftovers && !client.available()) {
		return true; // wait for more data to come in
	}
	ssize_t bufferLen = sizeof(connection.requestUri) - connection.readPosition;
	if (bufferLen <= 1) { // need bufferLen > 1 so there is room for more data + null byte
		connection.state = HTTPCLIENT_RCVD_URI_TOO_LONG;
		return false;
	}
	size_t amountRead = read(client, connection, connection.requestUri + connection.readPosition, bufferLen);

	// check for leading whitespace and trim it if necessary
	if (connection.readPosition == 0) {
		unsigned int i;
		for (i = 0; i < amountRead; i++) {
			if (connection.requestUri[i] != ' ' && connection.requestUri[i] != '\t') {
				break;
			}
		}
		if (i != 0) {
			copyInPlace(connection.requestUri, connection.requestUri + i, amountRead - i + /*zero byte*/ 1);
			amountRead -= i;
		}
	}
	// search for a whitespace character signifying end of this field
	char* endPosn = strchrs_2(connection.requestUri + connection.readPosition, ' ', '\t');
	if (endPosn) {
		// Store anything after the field delimiter as leftovers to be parsed later
		if (endPosn + 1 < connection.requestUri + connection.readPosition + amountRead) {
			unread(connection, endPosn + 1, connection.requestUri + connection.readPosition + amountRead - (endPosn + 1));
		}
		*endPosn = 0;
		connection.state = HTTPCLIENT_READING_VERSION;
		connection.readPosition = 0;
		connection.parsePosition = 0;
	}
	else {
		connection.readPosition += amountRead;
	}
	return false;
}

// returns: TRUE to stop parsing and wait for more data to come in; FALSE to try serving the state machine again
static bool parseClient_ReadingVersion(EthernetClient& client, HttpConnection& connection) {
	// NOTE: the literal text of the HTTP version is not stored; only an HttpVersion enum. For parsing,
	// the version is temporarily stored in the requestBody string, to save space.
	if (!connection.leftovers && !client.available()) {
		return true; // wait for more data to come in
	}
	ssize_t bufferLen = HTTP_VERSION_MAX_LEN + CRLF_STR_LEN + 1 - connection.readPosition; // space for the worst-case string ("HTTP/1.1\r\n") incl zero byte
	if (bufferLen <= 1) { // need bufferLen > 1 so there is room for more data + null byte
		connection.state = HTTPCLIENT_RCVD_BAD_VERSION;
		return false;
	}
	size_t amountRead = read(client, connection, connection.requestBody + connection.readPosition, bufferLen);

	// check for leading whitespace and trim it if necessary
	if (connection.readPosition == 0) {
		unsigned int i;
		for (i = 0; i < amountRead; i++) {
			if (connection.requestBody[i] != ' ' && connection.requestBody[i] != '\t') {
				break;
			}
		}
		if (i != 0) {
			copyInPlace(connection.requestBody, connection.requestBody + i, amountRead - i + /*also copy zero byte*/ 1);
			amountRead -= i;
		}
	}
	// search for a whitespace character signifying end of text
	char* endPosn = strstr_P(connection.requestBody + connection.readPosition, CRLF_STR);
	if (endPosn) {
		// Store anything after the field delimiter as leftovers to be parsed later
		if (endPosn + CRLF_STR_LEN < connection.requestBody + connection.readPosition + amountRead) {
			unread(connection, endPosn + CRLF_STR_LEN, connection.requestBody + connection.readPosition + amountRead - (endPosn + CRLF_STR_LEN));
		}
		memset(endPosn, 0, CRLF_STR_LEN);
		if (parseHttpVersion(connection.requestBody, connection.request.version)) {
			connection.state = HTTPCLIENT_READING_HEADER_START;
		}
		else {
			connection.state = HTTPCLIENT_RCVD_BAD_VERSION;
		}
		memset(connection.requestBody, 0, endPosn - connection.requestBody);
		connection.readPosition = 0;
		connection.parsePosition = 0;
	}
	else {
		connection.readPosition += amountRead;
	}
	return false;
}

// returns: TRUE to stop parsing and wait for more data to come in; FALSE to try serving the state machine again
static bool parseClient_ReadingHeaderStart(EthernetClient& client, HttpConnection& connection) {
	if (connection.readPosition - connection.parsePosition < 2) {
		if (!connection.leftovers && !client.available()) {
			return true;
		}
		// we have to read ahead more before we can tell whether this is an actual header or a blank line
		ssize_t bufferLen = sizeof(connection.requestHeaders) - connection.readPosition;
		if (bufferLen <= 1) {
			connection.state = HTTPCLIENT_RCVD_HDRS_TOO_LONG;
		}
		else {
			connection.readPosition += read(client, connection, connection.requestHeaders + connection.readPosition, bufferLen);
		}
	}
	else {
		if (!strncmp_P(connection.requestHeaders + connection.parsePosition, CRLF_STR, CRLF_STR_LEN)) {
			// end of header section

			// Store anything after the field delimiter as leftovers to be parsed later
			if (connection.parsePosition + CRLF_STR_LEN < connection.readPosition) {
				unread(connection, connection.requestHeaders + connection.parsePosition + CRLF_STR_LEN,
						connection.requestHeaders + connection.readPosition - (connection.requestHeaders + connection.parsePosition + CRLF_STR_LEN));
			}

			connection.readPosition = 0;
			connection.parsePosition = 0;
			connection.state = HTTPCLIENT_READING_BODY;
		}
		else if (connection.request.numHeaders == HTTP_HEADER_CNT) {
			connection.state = HTTPCLIENT_RCVD_HDRS_TOO_LONG;
		}
		else {
			// we're looking at the start of a valid header.
			connection.request.headers[connection.request.numHeaders].key = connection.requestHeaders + connection.parsePosition;
			connection.request.numHeaders++;
			connection.state = HTTPCLIENT_READING_HEADER_KEY;
		}
	}
	return false;
}

// returns: TRUE to stop parsing and wait for more data to come in; FALSE to try serving the state machine again
static bool parseClient_ReadingHeaderKey(EthernetClient& client, HttpConnection& connection) {
	if (connection.parsePosition == connection.readPosition) {
		if (!connection.leftovers && !client.available()) {
			return true; // need to wait for more data to come through
		}
		else {
			ssize_t bufferLen = sizeof(connection.requestHeaders) - connection.readPosition;
			if (bufferLen <= 1) {
				connection.state = HTTPCLIENT_RCVD_HDRS_TOO_LONG;
			}
			else {
				connection.readPosition += read(client, connection, connection.requestHeaders + connection.readPosition, bufferLen);
			}
		}
	}
	else {
		char* endPosn = strchr(connection.requestHeaders + connection.parsePosition, ':');
		if (endPosn) {
			connection.request.headers[connection.request.numHeaders - 1].keyLen = endPosn - connection.request.headers[connection.request.numHeaders - 1].key;
			connection.parsePosition = endPosn - connection.requestHeaders + 1;
			connection.state = HTTPCLIENT_READING_HEADER_VALUE;
		}
		else {
			connection.parsePosition = connection.readPosition;
		}
	}
	return false;
}

// returns: TRUE to stop parsing and wait for more data to come in; FALSE to try serving the state machine again
static bool parseClient_ReadingHeaderValue(EthernetClient& client, HttpConnection& connection) {
	if (connection.parsePosition == connection.readPosition) {
		if (!connection.leftovers && !client.available()) {
			return true; // need to wait for more data
		}
		else {
			ssize_t bufferLen = sizeof(connection.requestHeaders) - connection.readPosition;
			if (bufferLen <= 1) {
				connection.state = HTTPCLIENT_RCVD_HDRS_TOO_LONG;
			}
			else {
				connection.readPosition += read(client, connection, connection.requestHeaders + connection.readPosition, bufferLen);
			}
		}
	}
	else {
		HttpHeader* currentHeader = &connection.request.headers[connection.request.numHeaders - 1];
		if (!currentHeader->value) {
			unsigned int i;
			// haven't found the start of the literal value yet. Scan ahead until you find a non-whitespace character
			for (i = connection.parsePosition; i < connection.readPosition; i++) {
				if (connection.requestHeaders[i] != ' ' && connection.requestHeaders[i] != '\t') {
					currentHeader->value = connection.requestHeaders + i;
					break;
				}
			}
			connection.parsePosition = i;
		}
		else {
			char* endPosn = strstr_P(connection.requestHeaders + connection.parsePosition, CRLF_STR);
			if (endPosn) {
				currentHeader->valueLen = endPosn - currentHeader->value;
				if (!strncmp_P(currentHeader->key, CONTENT_LENGTH_STR, currentHeader->keyLen)) {
					char tmp = currentHeader->value[currentHeader->valueLen];
					currentHeader->value[currentHeader->valueLen] = '\0';
					//TODO: this does bad things if the content-length is not an integer
					connection.request.contentLength = atoi(currentHeader->value);
					currentHeader->value[currentHeader->valueLen] = tmp;
				}
				connection.parsePosition = endPosn + CRLF_STR_LEN - connection.requestHeaders;
				connection.state = HTTPCLIENT_READING_HEADER_START;
			}
			else {
				connection.parsePosition = connection.readPosition;
			}
		}
	}
	return false;
}

// returns: TRUE to stop parsing and wait for more data to come in; FALSE to try serving the state machine again
static bool parseClient_ReadingBody(EthernetClient& client, HttpConnection& connection) {
	if (connection.readPosition >= connection.request.contentLength) {
		connection.state = HTTPCLIENT_RCVD_REQUEST;
		return false;
	}
	else if (!connection.leftovers && !client.available()) {
		return true;
	}
	else if (connection.request.contentLength + 1 > sizeof(connection.requestBody)) {
		connection.state = HTTPCLIENT_RCVD_BODY_TOO_LONG;
		return false;
	}
	else {
		ssize_t bufferLen = connection.request.contentLength + 1 - connection.readPosition;
		connection.readPosition += read(client, connection, connection.requestBody + connection.readPosition, bufferLen);
	}
	return false;
}

//TODO: because of the way that we search for \r\n, if \r and \n come in in separate packets, we won't see it.
static void parseRequest(EthernetClient& client, HttpConnection& connection) {
	bool triggerReturn = false;
	while (!triggerReturn) {
		if (!(connection.state & HTTPCLIENT_STATUS_READING)) {
			return;
		}
		switch (connection.state) {
			case HTTPCLIENT_READING_METHOD:
				triggerReturn = parseClient_ReadingMethod(client, connection);
				break;

			case HTTPCLIENT_READING_VERSION:
				triggerReturn = parseClient_ReadingVersion(client, connection);
				break;

			case HTTPCLIENT_READING_URI:
				triggerReturn = parseClient_ReadingUri(client, connection);
				break;

			case HTTPCLIENT_READING_HEADER_START:
				triggerReturn = parseClient_ReadingHeaderStart(client, connection);
				break;

			case HTTPCLIENT_READING_HEADER_KEY:
				triggerReturn = parseClient_ReadingHeaderKey(client, connection);
				break;

			case HTTPCLIENT_READING_HEADER_VALUE:
				triggerReturn = parseClient_ReadingHeaderValue(client, connection);
				break;

			case HTTPCLIENT_READING_BODY:
				triggerReturn = parseClient_ReadingBody(client, connection);
				break;
		}
	}
}

// TODO performance tuning - It's probably faster to write in bulk rather than in pieces, but it looks like print(F("..."))
// prints characters one at a time after reading them from flash.
// Furthermore, it looks like EthernetClient's write() function has a busy loop while it waits for the W5100's buffer to
// clear out. Might need to modify Ethernet library to allow asynchronous write (i.e. application code polls for when the
// hardware is ready). Would need to test whether the busy loop is actually running - I think the chip's buffer is at least 2K
static void writeResponse(EthernetClient& client, HttpResponse& response) {
	switch (response.version) {
		case HttpVersion::Http_10:
			client.print(F("HTTP/1.0 "));
			break;
		default:
			client.print(F("HTTP/1.1 "));
	}

	switch (response.responseCode) {
		case 100:
			client.print(F("100 Continue"));
			break;
		case 101:
			client.print(F("101 Switching Protocols"));
			break;
		case 102:
			client.print(F("102 Processing"));
			break;
		case 103:
			client.print(F("103 Early Hints"));
			break;
		case 200:
			client.print(F("200 OK"));
			break;
		case 201:
			client.print(F("201 Created"));
			break;
		case 202:
			client.print(F("202 Accepted"));
			break;
		case 203:
			client.print(F("203 Non-Authoritative Information"));
			break;
		case 204:
			client.print(F("204 No Content"));
			break;
		case 205:
			client.print(F("205 Reset Content"));
			break;
		case 206:
			client.print(F("206 Partial Content"));
			break;
		case 207:
			client.print(F("207 Multi-Status"));
			break;
		case 208:
			client.print(F("208 Already Reported"));
			break;
		case 226:
			client.print(F("204 IM Used"));
			break;
		case 300:
			client.print(F("300 Multiple Choices"));
			break;
		case 301:
			client.print(F("301 Moved Permanently"));
			break;
		case 302:
			client.print(F("302 Found"));
			break;
		case 303:
			client.print(F("303 See Other"));
			break;
		case 304:
			client.print(F("304 Not Modified"));
			break;
		case 305:
			client.print(F("305 Use Proxy"));
			break;
		case 306:
			client.print(F("306 Switch Proxy"));
			break;
		case 307:
			client.print(F("307 Temporary Redirect"));
			break;
		case 308:
			client.print(F("308 Permanent Redirect"));
			break;
		case 400:
			client.print(F("400 Bad Request"));
			break;
		case 401:
			client.print(F("401 Unauthorized"));
			break;
		case 402:
			client.print(F("402 Payment Required"));
			break;
		case 403:
			client.print(F("403 Forbidden"));
			break;
		case 404:
			client.print(F("404 Not Found"));
			break;
		case 405:
			client.print(F("405 Method Not Allowed"));
			break;
		case 406:
			client.print(F("406 Not Acceptable"));
			break;
		case 407:
			client.print(F("407 Proxy Authentication Required"));
			break;
		case 408:
			client.print(F("408 Request Timeout"));
			break;
		case 409:
			client.print(F("409 Conflict"));
			break;
		case 410:
			client.print(F("410 Gone"));
			break;
		case 411:
			client.print(F("411 Length Required"));
			break;
		case 412:
			client.print(F("412 Precondition Failed"));
			break;
		case 413:
			client.print(F("413 Payload Too Large"));
			break;
		case 414:
			client.print(F("414 URI Too Long"));
			break;
		case 415:
			client.print(F("415 Unsupported Media Type"));
			break;
		case 416:
			client.print(F("416 Range Not Satisfiable"));
			break;
		case 417:
			client.print(F("417 Expectation Failed"));
			break;
		case 418:
			client.print(F("418 I'm a teapot"));
			break;
		case 421:
			client.print(F("421 Misdirected Request"));
			break;
		case 422:
			client.print(F("422 Unprocessable Entity"));
			break;
		case 423:
			client.print(F("423 Locked"));
			break;
		case 424:
			client.print(F("424 Failed Dependency"));
			break;
		case 425:
			client.print(F("425 Too Early"));
			break;
		case 426:
			client.print(F("426 Upgrade Required"));
			break;
		case 428:
			client.print(F("428 Precondition Required"));
			break;
		case 429:
			client.print(F("429 Too Many Requests"));
			break;
		case 431:
			client.print(F("431 Request Header Fields Too Large"));
			break;
		case 451:
			client.print(F("451 Unavailable For Legal Reasons"));
			break;
		case 500:
			client.print(F("500 Internal Server Error"));
			break;
		case 501:
			client.print(F("501 Not Implemented"));
			break;
		case 502:
			client.print(F("502 Bad Gateway"));
			break;
		case 503:
			client.print(F("503 Service Unavailable"));
			break;
		case 504:
			client.print(F("504 Gateway Timeout"));
			break;
		case 505:
			client.print(F("505 HTTP Version Not Supported"));
			break;
		case 506:
			client.print(F("506 Variant Also Negotiates"));
			break;
		case 507:
			client.print(F("507 Insufficient Storage"));
			break;
		case 508:
			client.print(F("508 Loop Detected"));
			break;
		case 510:
			client.print(F("510 Not Extended"));
			break;
		case 511:
			client.print(F("511 Network Authentication Required"));
			break;
		default:
			client.print(response.responseCode);
			client.print(F(" (unknown status)"));
	}
	client.print(F("\r\nConnection: Close\r\n"));
	for (int i = 0; response.headers[i].key; i++) {
		client.print(response.headers[i].key);
		client.print(F(": "));
		client.print(response.headers[i].value);
		client.print(F("\r\n"));
	}
	if (response.contentLength) {
		client.print(F("Content-Length: "));
		client.print(response.contentLength);
		client.print(F("\r\n"));
	}
	client.print(F("\r\n"));
	if (response.content && response.contentLength) {
		if (response.isContentInProgmem) {
			writePgm(client, reinterpret_cast<PGM_P>(response.content), response.contentLength);
		}
		else {
			client.write(response.content, response.contentLength);
		}
	}

}

//TODO if we ever support keep-alive, remove the hardcoded "Connection: Close" from each of these responses
static void handleRequest(EthernetClient& client, HttpConnection& connection, HttpHandler* handler) {
	HttpResponse response {};
	switch (connection.state) {
		case HTTPCLIENT_RCVD_REQUEST:
			handler(connection.request, response);
			writeResponse(client, response);
			break;
		case HTTPCLIENT_RCVD_BAD_METHOD:
			client.print(F("HTTP/1.1 405 Method Not Allowed\r\nConnection: Close\r\nContent-Length: 23\r\n\r\nERROR - unknown method."));
			break;
		case HTTPCLIENT_RCVD_BAD_VERSION:
			client.print(F("HTTP/1.1 505 HTTP Version Not Supported\r\nConnection: Close\r\nContent-Length: 29\r\n\r\nERROR - unknown HTTP version."));
			break;
		case HTTPCLIENT_RCVD_URI_TOO_LONG:
			client.print(F("HTTP/1.1 414 URI Too Long\r\nConnection: Close\r\nContent-Length: 29\r\n\r\nERROR - Request URI too long."));
			break;
		case HTTPCLIENT_RCVD_HDRS_TOO_LONG:
			client.print(F("HTTP/1.1 431 Request Header Fields Too Large\r\nConnection: Close\r\nContent-Length: 40\r\n\r\nERROR - Request Header Fields Too Large."));
			break;
		case HTTPCLIENT_RCVD_BODY_TOO_LONG:
			client.print(F("HTTP/1.1 413 Request Entity Too Large\r\nConnection: Close\r\nContent-Length: 33\r\n\r\nERROR - Request Entity Too Large."));
			break;
		default:
			// can't hurt
			client.print(F("HTTP/1.1 500 Internal Server Error\r\nConnection: Close\r\nContent-Length: 53\r\n\r\nUnknown connection state. Please talk to a developer."));
			break;
	}
}

static void resetConnection(HttpConnection& connection) {
	connection.leftovers = nullptr;
	connection.leftoversLength = 0;
	connection.parsePosition = 0;
	connection.readPosition = 0;
	connection.request.content = connection.requestBody;
	connection.request.contentLength = 0;
	HttpHeader* header = &connection.request.headers[0];
	for (int i = 0; i < HTTP_HEADER_CNT; i++) {
		header->key = nullptr;
		header->keyLen = 0;
		header->value = nullptr;
		header->valueLen = 0;
		header++;
	}
	connection.request.numHeaders = 0;
	connection.request.method = HttpMethod::GET;
	connection.request.uri = connection.requestUri;
	connection.request.uriLength = 0;
	connection.request.version = HttpVersion::Http_10;
	memset(&connection.requestBody, 0, sizeof(connection.requestBody));
	memset(&connection.requestHeaders, 0, sizeof(connection.requestHeaders));
	memset(&connection.requestUri, 0, sizeof(connection.requestUri));
	connection.state = HTTPCLIENT_STATUS_DISCONNECTED;
}

void HttpServer::serve(void) {
	// if we have enough room for more clients, look for and add them
	if (numConnections < maxConnections) {
		EthernetClient newClient = server.accept();
		while (newClient) {
			// search for a slot to put the client
			for (uint8_t i = 0; i < maxConnections; i++) {
				if (!clients[i]) {
					clients[i] = newClient;
					connections[i].state = HTTPCLIENT_READING_METHOD;
					numConnections++;
					break;
				}
			}
			if (numConnections == maxConnections) {
				break;
			}
			// continue accepting clients until there are none or we're out of slots for them
			newClient = server.accept();
		}
	}

	for (int i = 0; i < maxConnections; i++) {
		if (!clients[i]) {
			continue;
		}
		if (!clients[i].connected()) {
			clients[i].stop();
			numConnections--;
			resetConnection(connections[i]);
			continue;
		}
		if (connections[i].state & HTTPCLIENT_STATUS_READING) {
			parseRequest(clients[i], connections[i]);
		}
		if (connections[i].state & HTTPCLIENT_STATUS_RCVD) {
#if LOG_LEVEL <= LOG_LEVEL_VERBOSE
			LOG_VERBOSE("Received HTTP Message: State=[%d], Method=[%d], URI=[%s], Version=[%d]",
							connections[i].state,
							static_cast<uint8_t>(connections[i].request.method),
							connections[i].request.uri,
							static_cast<uint8_t>(connections[i].request.version));
			Log.writeDetails_P(PSTR("Headers (%d)\n%s\n"), connections[i].request.numHeaders, connections[i].requestHeaders);
			Log.writeDetails_P(PSTR("Request Body (len=%d):\n%s\n"), connections[i].request.contentLength, connections[i].request.content);
#endif
			// respond to the request using the provided handler, or a default error behavior.
			handleRequest(clients[i], connections[i], handler);
			// reset the connection state and clear all request data
			resetConnection(connections[i]);
			// disconnect from the client
			clients[i].stop();
			numConnections--;
		}
	}
}
