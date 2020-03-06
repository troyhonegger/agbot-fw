# HTTP API Spec

TODO - need to flesh this out from old API, fill in blanks, and replace references to old api-spec file with this new HTTP API.  

This API is hosted on the Arduino that powers the multivator.
All high-level communication with the Arduino
(with the exception of updating firmware) should take place over this API.
This API does not specify how other components of the system
(cameras, front-end communication, etc) interact with each other.

## General Functionality and Architecture
 - The API will run on top of the HTTP protocol.
 - The API will be hosted on a static IP address on port 80.
 - The API will _not_ support persistent connections (at least in the first iteration). Clients should expect each response to contain a `"Connection: Close"` header.
   - This may be changed in future iterations as an optimization.
 - In general, the Arduino is a _very_ computationally limited platform, particularly when it comes to RAM. Care should be take to avoid sending large requests.

## Endpoints

#### GET `/`
Returns `text/html` webpage


#### GET `/api/gps`
TODO - flesh out. Will probably return `application/json`


#### GET `/api/config/{setting}`
Provides the value of the configuration setting `setting`. If `{setting}` is omitted, returns all configuration settings.  
Response: 200 (OK), `application/json`
```json
GET /api/config
=> {
  "HitchAccuracy": 5,
  "HitchLoweredHeight": 0,
  "HitchRaisedHeight": 100,
  ...
}
GET /api/config/HitchAccuracy
=> 5
```

#### PUT `/api/config/{setting}`
`{setting}` should contain the name of the setting to configure.  
Request body should be `application/json` and contain a single integer with the value of that setting.  
Response: 204 (No Content)


#### GET `/api/tillers/{id}`
`{id}` should be a number between 0 and 2.  
0 represents the left tiller, 1 represents the middle tiller, and 2 represents the right tiller.  
If `{id}` is omitted, all tillers should be returned as a JSON array.  
Response: 200 (OK), `application/json`
```json
{
  "actualHeight": "20",
  "targetHeight": "LOWERED",
  "dh": 0
}

"dh" is one of the following:
 -1: lowering
  0: stopped
  1: raising
"actualHeight" is an integer between 0 and 100
  0: fully lowered
  100: fully raised
"targetHeight" is one of the following:
  an integer between 0 (fully lowered) and 100 (fully raised)
  "STOP": Tiller is stopped regardless of its height
  "UP": Tiller is raising regardless of its height
  "DOWN": Tiller is lowering regardless of its height
  "LOWERED": Tiller wants to remain just below the surface.
  "RAISING": Tiller wants to remain just above the surface.
```


#### PUT `/api/tillers/{id}`
`{id}` should be a number between 0 and 2.  
0 represents the left tiller, 1 represents the middle tiller, and 2 represents the right tiller.  
If omitted, all tillers will be set together.  
Expects: `application/json` object:
```json
{
  // required. See documentation for GET request
  "targetHeight": "STOP",
  // optional - time delay in milliseconds. Default: 0
  "delay": 500
}
```
Response: 204 (No Content)


#### GET `/api/sprayers/{id}`
`{id}` should be a number between 0 and 7, or either `"left"` or `"right"`.  
TODO - document mapping of IDs to physical sprayers.  
If `{id}` is omitted, all sprayers should be returned as a JSON array.  
Response: 200 (OK), `application/json`
```json
{
  "status": "ON" // required. Either "ON" or "OFF"
}
```


#### PUT `/api/sprayers/{id}`
`{id}` should be a number between 0 and 7, or either `"left"` or `"right"`.  
TODO - document mapping of IDs to physical sprayers.
If `{id}` is omitted, all sprayers should be set together.
Expects: `application/json`:
```json
{
  // required. Either "ON" or "OFF"
  "status": "ON",
  // optional: time delay in milliseconds. Default: 0
  "delay": 500
}
```


#### GET `/api/hitch`
Returns `application/json`

#### PUT `/api/hitch`
Valid in either diagnostics or run mode. TODO - define schema


#### POST `/api/weeds/{weedStr}`
`{weedStr}` is a 5-character hex bitfield.
Returns 400 if in diagnostics mode.

