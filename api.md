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


#### GET `/version`
Response: 200 OK, `text/plain`. Example: "v1.0.0 - built Dec 22 2021 20:39:35"


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
Request body is `application/json` and contains a single integer with the value of that setting.  
Response: 204 (No Content)


#### GET `/api/tillers/{id}`
`{id}` should be a number between 0 and 2.  
0 represents the left tiller, 1 represents the middle tiller, and 2 represents the right tiller.  
If `{id}` is omitted, all tillers are returned as a JSON array.
Response: 200 (OK), `application/json`
```json
{
  // an integer between 0 and 100.
    //   0 -> fully lowered
    // 100 -> fully raised
  "height": 20,

  // One of the following:
    // an integer between 0 and 100 -> tiller wants to achieve this height.
    // "STOP" -> tiller is stopped regardless of its height.
    // "UP" -> tiller is raising regardless of its height.
    // "DOWN" -> tiller is lowering regardless its of height.
    // "LOWERED" -> tiller wants to remain just below the surface.
    // "RAISED" -> tiller wants to remain just above the surface.
  "target": "LOWERED",

  // One of the following:
    // -1 -> lowering
    //  0 -> stopped
    //  1 -> raising
  "dh": 0
}
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
`{id}` (optional) should be a number between 0 and 7. If `{id}` is omitted, all sprayers are returned as a JSON array.
Sprayers 0-3 are on the left row, numbered front-to-back; and sprayers 4-7 are on the right row, numbered front-to-back.
Response: 200 (OK), `application/json`
```json
{
  "status": "ON" // required. Either "ON" or "OFF"
}
```


#### PUT `/api/sprayers/{id}`
`{id}` (optional) should be a number between 0 and 7, or either `left` or `right`. If `{id}` is omitted, all sprayers are set together.
Sprayers 0-3 are on the left row, numbered front-to-back; and sprayers 4-7 are on the right row, numbered front-to-back.
Expects: `application/json`:
```json
{
  // required. Either "ON" or "OFF"
  "status": "ON",
  // optional: time delay in milliseconds. Default: 0
  "delay": 500
}
```
Response: 204 (No Content)


#### GET `/api/hitch`
Gives the status of the trailer's 3-point hitch. The hitch should be raised for transportation
and lowered when moving through the field.

Response: 200 (OK), `application/json`
```json
{
  // an integer between 0 and 100.
    //   0 -> fully lowered
    // 100 -> fully raised
  "height": 20,

  // One of the following:
    // an integer between 0 and 100 -> hitch wants to achieve this height.
    // "STOP" -> hitch is stopped regardless of its height.
  "target": "STOP",

  // One of the following:
    // -1 -> lowering
    //  0 -> stopped
    //  1 -> raising
  "dh": 0
}
```

#### PUT `/api/hitch`
Sets the status of the trailer's 3-point hitch. The hitch should be raised for transportation
and lowered when moving through the field.

Expects: `application/json` object:
```json
{
  // required. Should be one of the following:
    // an integer between 0 and 100 -> hitch wants to achieve this height.
    // "STOP" -> hitch should stop regardless of height.
    // "UP" -> target the current value of config setting "HitchRaisedHeight"
    // "DOWN" -> target the current value of config setting "HitchLoweredHeight"
  "targetHeight": "STOP"
}
```

#### GET `/api/heightSensors`
The BOT has 3 LIDAR height sensors on the mounting bar, aligned with each of the 3 multivators. These sensors measure
the distance to the ground, and can be used to lower the tillers into the ground on uneven or sloping surfaces.

Response: 200 OK, `application/json`:
```json
{
  "hwError": "string", // if missing/falsey, there is no error. If present/truthy, sensors may be an empty array
  "sensors": [
    {
      "paired": true, // true if controller is connected to sensor. This should occur within 100's of ms of a sensor power cycle.
      "serial": 58124, // 16-bit unique ID of sensor. Assigned by manufacturer and burned into ROM
      "height": 38 // height above the ground in cm
    },
    {
      "paired": false
      // unpaired sensors will not have serial or height properties
    },
    {
      "paired": false
      // unpaired sensors will not have serial or height properties
    }
  ]
}
```

#### POST `/api/weeds/{weedStr}`
`{weedStr}` is a 5-character hex bitfield. It contains the set of weeds found in each of the five rows, from left to right:
- The first character marks weeds found under the leftmost tiller
- The second character marks weeds found under the left sprayers
- The third character marks weeds found under the middle tiller
- The fourth character marks weeds found under the right sprayers
- The fifth character marks weeds found under the rightmost tiller

The BOT has four tanks, so it works on up to four different weed types. Each weed type is a bit in the hex digit:
- If the `0x01` bit is set, the weed will be sprayed from tank 1. (Sprayers 0 and 4)
- If the `0x02` bit is set, the weed will be sprayed from tank 2. (Sprayers 1 and 5)
- If the `0x04` bit is set, the weed will be sprayed from tank 3. (Sprayers 2 and 6)
- If the `0x08` bit is set, the weed will be sprayed from tank 4. (Sprayers 3 and 7)

Alternatively, one of the tanks may contain fertilizer instead of herbicide. Then you can set the corresponding bit
to apply fertilizer to a portion of the crop.

The controller will use its configured delay settings to calculate when the weed(s) are under the trailer, and turn on the
sprayers/tillers to eliminate them.

Response: 204 (No Content)