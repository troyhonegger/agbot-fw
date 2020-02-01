#include "Http.hpp"

typedef long ssize_t; // for some reason Arduino doesn't have this defined by default?

#define HTTPCLIENT_DISCONNECTED			(0)
#define HTTPCLIENT_READING_METHOD		(1)
#define HTTPCLIENT_READING_VERSION		(2)
#define HTTPCLIENT_READING_URI			(3)
#define HTTPCLIENT_READING_HEADER_KEY	(4)
#define HTTPCLIENT_START_OF_HEADER		(5)
#define HTTPCLIENT_READING_HEADER_VALUE	(6)
#define HTTPCLIENT_READING_BODY			(7)

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

HttpServer::HttpServer(EthernetServer& server, uint8_t maxConnections, HttpHandler handler)
		: server(server), maxConnections(maxConnections), handler(handler) {
	// limit maxConnections to HTTP_MAX_CONNECTIONS
	if (maxConnections > HTTP_MAX_CONNECTIONS) {
		HttpServer::maxConnections = HTTP_MAX_CONNECTIONS;
	}
}

// ASSUMES Ethernet has already been initialized by the application level
void HttpServer::begin(void) {
	server.begin();
}

static size_t read(EthernetClient& client, HttpConnection& connection, char* buf, size_t bufferLen) {
	size_t numRead = 0;
	if (connection.leftovers) {
		memcpy(buf, connection.leftovers, connection.leftoversLength);
		memset(connection.leftovers, 0, connection.leftoversLength);
		numRead = connection.leftoversLength;
		buf[numRead] = '\0';
		connection.leftovers = nullptr;
		connection.leftoversLength = 0;
	}
	else {
		numRead = client.read((uint8_t*) buf, bufferLen - 1);
		buf[numRead] = '\0';
	}
	return numRead;
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

//TODO: because of the way that we search for \r\n, if \r and \n come in in separate packets, we won't see it.
static void serveClient(EthernetClient& client, HttpConnection& connection) {
	size_t amountRead = 0;
	ssize_t bufferLen = 0;
	char* endPosn = nullptr;
	int i = 0;
	while (true) {
		switch (connection.state) {
			case HTTPCLIENT_DISCONNECTED:
				return;
			case HTTPCLIENT_READING_METHOD:
				// NOTE: the literal text of the HTTP method is not stored; only an HttpMethod enum. For parsing,
				// the method is temporarily stored in the requestHeaders string, to save space.
				if (!connection.leftovers && !client.available()) {
					return; // wait for more data to come in
				}
				bufferLen = HTTP_METHOD_MAX_LEN + 2 - connection.readPosition; // space for the worst-case string ("OPTIONS ") incl zero byte
				if (bufferLen <= 0) {
					// TODO: respond with 415 and disconnect
				}
				amountRead = read(client, connection, connection.requestHeaders + connection.readPosition, bufferLen);

				// search for a whitespace character signifying end of text
				endPosn = strchrs_2(connection.requestHeaders + connection.readPosition, ' ', '\t');
				if (endPosn) {
					// Store anything after the field delimiter as leftovers to be parsed later
					if (endPosn + 1 < connection.requestHeaders + connection.readPosition + amountRead) {
						connection.leftovers = endPosn + 1;
						connection.leftoversLength = connection.requestHeaders + connection.readPosition + amountRead - (endPosn + 1);
					}
					*endPosn = '\0';
					if (parseHttpMethod(connection.requestHeaders, connection.request.method)) {
						connection.state = HTTPCLIENT_READING_VERSION;
					}
					else {
						// TODO: respond with 415 and disconnect
						connection.state = HTTPCLIENT_DISCONNECTED;
					}
					memset(connection.requestHeaders, 0, endPosn - connection.requestHeaders);
					connection.readPosition = 0;
					connection.parsePosition = 0;
				}
				else {
					connection.readPosition += amountRead;
				}
				break;

			case HTTPCLIENT_READING_VERSION:
				// NOTE: the literal text of the HTTP version is not stored; only an HttpVersion enum. For parsing,
				// the version is temporarily stored in the requestBody string, to save space.
				if (!connection.leftovers && !client.available()) {
					return; // wait for more data to come in
				}
				bufferLen = HTTP_VERSION_MAX_LEN + 2 - connection.readPosition; // space for the worst-case string ("HTTP/1.1 ") incl zero byte
				if (bufferLen <= 0) {
					// TODO: respond with 505 and disconnect
				}
				amountRead = read(client, connection, connection.requestBody + connection.readPosition, bufferLen);

				// check for leading whitespace and trim it if necessary
				if (connection.readPosition == 0) {
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
				endPosn = strchrs_2(connection.requestHeaders + connection.readPosition, ' ', '\t');
				if (endPosn) {
					// Store anything after the field delimiter as leftovers to be parsed later
					if (endPosn + 1 < connection.requestBody + connection.readPosition + amountRead) {
						connection.leftovers = endPosn + 1;
						connection.leftoversLength = connection.requestBody + connection.readPosition + amountRead - connection.leftovers;
					}
					*endPosn = '\0';
					if (parseHttpVersion(connection.requestBody, connection.request.version)) {
						connection.state = HTTPCLIENT_READING_URI;
					}
					else {
						// TODO: respond with 505 and disconnect
						connection.state = HTTPCLIENT_DISCONNECTED;
					}
					memset(connection.requestBody, 0, endPosn - connection.requestBody);
					connection.readPosition = 0;
					connection.parsePosition = 0;
				}
				else {
					connection.readPosition += amountRead;
				}
				break;

			case HTTPCLIENT_READING_URI:
				if (!connection.leftovers && !client.available()) {
					return; // wait for more data to come in
				}
				bufferLen = sizeof(connection.requestUri) - connection.readPosition;
				if (bufferLen <= 1) {
					// TODO: respond with 414 and disconnect
				}
				amountRead = read(client, connection, connection.requestUri + connection.readPosition, bufferLen);

				// check for leading whitespace and trim it if necessary
				if (connection.readPosition == 0) {
					for (i = 0; i < amountRead; i++) {
						if (connection.requestUri[i] != ' ' && connection.requestUri[i] != '\t') {
							break;
						}
					}
					if (i != 0) {
						copyInPlace(connection.requestUri, connection.requestUri + i, amountRead - i + /*also copy zero byte*/ 1);
						amountRead -= i;
					}
				}
				// search for a CRLF signifying end of text
				endPosn = strstr_P(connection.requestUri + connection.readPosition, CRLF_STR);
				if (endPosn) {
					// Store anything after the field delimiter as leftovers to be parsed later
					if (endPosn + CRLF_STR_LEN < connection.requestUri + connection.readPosition + amountRead) {
						connection.leftovers = endPosn + CRLF_STR_LEN;
						connection.leftoversLength = connection.requestUri + connection.readPosition + amountRead - connection.leftovers;
					}
					memset(endPosn, 0, CRLF_STR_LEN);
					connection.state = HTTPCLIENT_START_OF_HEADER;
					connection.readPosition = 0;
					connection.parsePosition = 0;
				}
				else {
					connection.readPosition += amountRead;
				}
				break;

			case HTTPCLIENT_START_OF_HEADER:
				if (connection.readPosition - connection.parsePosition < 2) {
					if (!connection.leftovers && !client.available()) {
						return; // wait for data to be available
					}
					// we have to read ahead more before we can tell whether this is an actual header or a blank line
					bufferLen = sizeof(connection.requestHeaders) - connection.readPosition;
					if (bufferLen <= 1) {
						// TODO: respond with 431 and disconnect
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
							connection.leftovers = connection.requestHeaders + connection.parsePosition + CRLF_STR_LEN;
							connection.leftoversLength = connection.requestHeaders + connection.readPosition - connection.leftovers;
						}

						connection.readPosition = 0;
						connection.parsePosition = 0;
						connection.state = HTTPCLIENT_READING_BODY;
					}
					else if (connection.request.numHeaders == HTTP_HEADER_CNT) {
						// TODO: too many headers; respond with 431 and disconnect
					}
					else {
						// we're looking at the start of a valid header.
						connection.request.headers[connection.request.numHeaders].key = connection.requestHeaders + connection.parsePosition;
						connection.request.numHeaders++;
						connection.state = HTTPCLIENT_READING_HEADER_KEY;
					}
				}
				break;

			case HTTPCLIENT_READING_HEADER_KEY:
				if (connection.parsePosition == connection.readPosition) {
					if (!connection.leftovers && !client.available()) {
						return; // need to wait for more data to come through
					}
					else {
						bufferLen = sizeof(connection.requestHeaders) - connection.readPosition;
						if (bufferLen <= 1) {
							// TODO: respond with 431 and disconnect
						}
						else {
							connection.readPosition += read(client, connection, connection.requestHeaders, bufferLen);
						}
					}
				}
				else {
					endPosn = strchr(connection.requestHeaders + connection.parsePosition, ':');
					if (endPosn) {
						connection.request.headers[connection.request.numHeaders].keyLen = endPosn - connection.request.headers[connection.request.numHeaders].key;
						connection.parsePosition = endPosn - connection.requestHeaders + 1;
						connection.state = HTTPCLIENT_READING_HEADER_VALUE;
					}
					else {
						connection.parsePosition = connection.readPosition;
					}
				}
				break;

			case HTTPCLIENT_READING_HEADER_VALUE:
				if (connection.parsePosition == connection.readPosition) {
					if (!connection.leftovers && !client.available()) {
						return; // need to wait for more data
					}
					else {
						bufferLen = sizeof(connection.requestHeaders) - connection.readPosition;
						if (bufferLen <= 1) {
							// TODO send 431 and disconnect
						}
						else {
							connection.readPosition += read(client, connection, connection.requestHeaders, bufferLen);
						}
					}
				}
				else {
					HttpHeader* currentHeader = &connection.request.headers[connection.request.numHeaders - 1];
					if (currentHeader->value) {
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
						endPosn = strstr_P(connection.requestHeaders + connection.parsePosition, CRLF_STR);
						if (endPosn) {
							currentHeader->valueLen = endPosn - currentHeader->value;
							if (strncmp_P(currentHeader->key, CONTENT_LENGTH_STR, currentHeader->keyLen)) {
								char tmp = currentHeader->value[currentHeader->valueLen];
								currentHeader->value[currentHeader->valueLen] = '\0';
								//TODO: this does bad things if the content-length is not an integer
								connection.request.contentLength = atoi(currentHeader->value);
								currentHeader->value[currentHeader->valueLen] = tmp;
							}
							connection.parsePosition = endPosn + CRLF_STR_LEN - (connection.requestHeaders + connection.parsePosition);
							connection.state = HTTPCLIENT_START_OF_HEADER;
						}
						else {
							connection.parsePosition = connection.readPosition;
						}
					}
				}
				break;

			case HTTPCLIENT_READING_BODY:
				if (connection.readPosition >= connection.request.contentLength) {
					// TODO: done. Now what???
				}
				else if (!connection.leftovers && !client.available()) {
					return; // need to wait for more data to come through
				}
				else if (connection.request.contentLength + 1 > sizeof(connection.requestBody)) {
					// TODO: send 413 and disconnect
				}
				else {
					bufferLen = connection.request.contentLength + 1 - connection.readPosition;
					connection.readPosition += read(client, connection, connection.requestBody + connection.readPosition, bufferLen);
				}
				break;
		}
	}
}

void HttpServer::serve(void) {
	// if we have enough room for more clients, look for and add them
	if (numConnections < maxConnections) {
		EthernetClient newClient = server.accept();
		while (newClient) {
			for (uint8_t i = 0; i < maxConnections; i++) {
				if (!clients[i]) {
					clients[i] = newClient;
					connections[i].state = HTTPCLIENT_READING_METHOD;
					numConnections++;
					break;
				}
			}
			if (numConnections = maxConnections) {
				break;
			}
			newClient = server.accept();
		}
	}

	// TODO: of course, lots of blanks to fill in here
	for (int i = 0; i < maxConnections; i++) {
		if (!clients[i]) {
			continue;
		}
		if (!clients[i].connected()) {
			clients[i].stop();
			numConnections--;
			connections[i].state = HTTPCLIENT_DISCONNECTED;
		}
	}
}
