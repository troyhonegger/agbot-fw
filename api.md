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

#### GET `/api/mode`
Returns `application/json` response that looks like `{"mode": "<mode>"}`, where `<mode>` is either `Diagnostics` or `Run`. The default is Run mode, and the mode can only be changed with a call to `PUT /api/mode` (see below)

#### PUT `/api/mode`
Expects `application/json` request body that looks like `{"mode": "<mode>"}`, where `<mode>` is either `Diagnostics` or `Run`.


#### GET `/api/gps`
TODO - flesh out. Will probably return `application/json`


#### GET `/api/config/{setting}`
Returns an integer (`application/json`). If `{setting}` is omitted, returns a JSON object containing all settings.

#### PUT `/api/config/{setting}`
Expects the request body to contain an integer.


#### GET `/api/tillers/{id}`
`{id}` is between 0 and 2. 0 represents the left tiller, 1 represents the middle tiller, and 2 represents the right tiller. Returns `application/json`

#### PUT `/api/tillers/{id}`
`{id}` is between 0 and 2. 0 represents the left tiller, 1 represents the middle tiller, and 2 represents the right tiller. If omitted, all tillers will be set together.
Returns 400 if in diagnostics mode.
TODO - define schema. Need to decide whether to allow targeting a given height or not.


#### GET `/api/sprayers/{id}`
`{id}` is between 0 and 7. Returns `application/json`

#### PUT `/api/sprayers/{id}`
`{id}` is between 0 and 7, or one of `"left"` or `"right"`. If omitted, all sprayers will be set at once.
Returns 400 if in diagnostics mode.
TODO - define schema.


#### GET `/api/hitch`
Returns `application/json`

#### PUT `/api/hitch`
Valid in either diagnostics or run mode. TODO - define schema


#### POST `/api/weeds/{weedStr}`
`{weedStr}` is a 5-character hex bitfield.
Returns 400 if in diagnostics mode.

