#pragma once

#include <Arduino.h>

//#include "Ethernet.h"
// TODO: this is ugly, but it's the only way Atmel studio currently recognizes the header. How to add Ethernet.h to the include path?
#include "../ArduinoCore/include/libraries/Ethernet/src/Ethernet.h"

#define HTTP_HEADER_CNT			(8)

// Size of buffer allocated by HTTP server for storing HTTP headers.
// Any incoming request cannot overflow this buffer; if it does, a 431 is returned
#define HTTP_INCOMING_REQUEST_HEADERS_SIZE		(256)
// Size of buffer allocated by HTTP server for storing request URI.
// Any incoming request cannot overflow this buffer; if it does, a 414 is returned
#define HTTP_INCOMING_REQUEST_URI_SIZE			(40)
// Size of buffer allocated by HTTP server for storing request body.
// Any incoming request cannot overflow this buffer; if it does, a 413 is returned
#define HTTP_INCOMING_REQUEST_BODY_SIZE			(256)

// cannot be more than MAX_SOCK_NUM. Adjust as needed to save memory.
#define HTTP_MAX_CONNECTIONS		(4)

enum class HttpMethod : uint8_t {
	OPTIONS,
	GET,
	HEAD,
	POST,
	PUT,
	DELETE,
	PATCH,
};

enum class HttpVersion : uint8_t {
	Http_10,
	Http_11,
};

struct HttpHeader {
	char *key;
	char *value;
	size_t valueLen;
	size_t keyLen;
};

struct HttpRequest {
	HttpMethod method;
	HttpVersion version;
	uint8_t numHeaders;
	char *uri;
	size_t uriLength;
	HttpHeader headers[HTTP_HEADER_CNT];
	char* content;
	size_t contentLength;
};

struct HttpResponse {
	HttpHeader headers[HTTP_HEADER_CNT];
	char* content;
	size_t contentLength;
	uint16_t responseCode;
	HttpVersion version;
	bool isContentInProgmem;
};

struct HttpConnection {
	HttpRequest request;
	char requestHeaders[HTTP_INCOMING_REQUEST_HEADERS_SIZE];
	char requestUri[HTTP_INCOMING_REQUEST_URI_SIZE];
	char requestBody[HTTP_INCOMING_REQUEST_BODY_SIZE];
	char *leftovers;
	size_t readPosition;
	size_t parsePosition;
	uint16_t leftoversLength;
	uint8_t state;
};

typedef void HttpHandler(HttpRequest const &, HttpResponse &);

class HttpServer {
	uint8_t numConnections;
	uint8_t maxConnections;

	EthernetServer &server;

	EthernetClient clients[HTTP_MAX_CONNECTIONS];
	HttpConnection connections[HTTP_MAX_CONNECTIONS];

	HttpHandler* handler;
public:
	HttpServer(EthernetServer& server, uint8_t maxConnections, HttpHandler* handler);
	void begin(void);
	void serve(void);
};

// TODO: HttpClient??
