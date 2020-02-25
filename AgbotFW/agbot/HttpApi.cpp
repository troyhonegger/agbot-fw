/*
 * HttpApi.cpp
 *
 * Created: 2/25/2020 3:38:32 PM
 *  Author: troy.honegger
 */ 

#include "Devices.hpp"
#include "HttpApi.hpp"

void apiHandler(HttpRequest const& request, HttpResponse& response) {
	// TODO implement API
	response.version = HttpVersion::Http_11;
	response.responseCode = 501;
	response.content = "HTTP API Not implemented";
	response.contentLength = 24;
}