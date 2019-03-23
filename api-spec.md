# API Specification

### Scope/Purpose
This API is hosted on the microcontroller that runs the multivator. The speed-control system is a separate API.

### General Functionality Notes
* The API will use the TCP protocol.
* The API will be able to communicate with multiple clients concurrently (minimum 2-4 for current use case).
* If these clients send conflicting messages (for instance, one switches to process mode and the other to diagnostics mode), the controller should simply process messages on a first-come, first-serve basis
* The API will be hosted at the static IP address 10.0.0.2, port 8010.
* Messages must end with either a Windows CRLF (`\r\n`) or a Unix LF (`\n`).
* Messages will contiain ASCII-encoded data only - no raw binary data or extended character sets
* Messages must be 63 bytes or less in length.
* The controller should process and respond to messages strictly sequentially, in the order in which they are received.
* The controller should respond to each message it receives. If a message requires no response, the controller should still send a blank line to indicate that the message was processed correctly.

### Command Types (to the API from the client computer)
* `Estop` - immediately engages the machine's e-stop and cuts power to all peripherals, no questions asked. Use with care.
* `KeepAlive` - used to detect if the API is still connected to the client.
  * If a client has not sent a message in 1,000 ms, it should send a KeepAlive to the controller.
  * If the controller goes 2,000 ms without receiving at least one message from at least one client, it will engage the e-stop.
  * Sidenote: if the client does not receive a response to this (or any) message, it should also engage the e-stop.
* `SetMode {mode}`
  * `{mode}` should be either `"Processing"` or `"Diagnostics"`
  * Processing mode: used to send procesing commands that tell the controller where the weeds are, allowing it to coordinate the multivator's actions accordingly.
  * Diagnostics mode: used to more directly control the multivator, generally for troubleshooting or other non-production purposes.
* `GetState {query}`
  * `{query}` must be one of `Mode`, `Configuration[setting]`, `Tiller[ID]`, `Sprayer[ID]`, or `Hitch`, with square brackets in each case separating component from identifier.
  * For `GetState Mode`, no arguments are necessary
  * For `GetState Configuration[setting]`, `setting` should be one of the allowed configuration settings (see "Configuration Settings" below for details)
  * For `GetState Tiller[ID]`, `ID` should be a decimal integer between 0 and 2 (0 is the far left tiller, 1 is the middle tiller, 2 is the right tiller)
  * For `GetState Sprayer[ID]`, `ID` should be a decimal integer between 0 and 7 (0 is the front left sprayer, 3 is the back left sprayer, 4 is the front right sprayer, 7 is the back right sprayer)
  * For `GetState Hitch`, no arguments are necessary
  * See the "Responses" section below for the controller's responses to each of these requests.
* `SetConfig {property}={value}`
  * There are certain configurable settings the controller can remember across sessions - this command sets them.
  * See "Configuration Settings" below for a list of all possible options for `{property}`, with their corresponding ranges for `{value}`. In each case, `{value}` must be a decimal integer.
* `DiagSet {element}={value}`: Sends a diagnostics command (this message is invalid in processing mode).
  * `{element}` must be one of `"Sprayer[XX]"`, `"Tiller[X]"`, or `"Hitch"`
  * For `Hitch`, `{value}` should be either `"STOP"` or a decimal integer between 0 and 100 specifying the target height of the hitch
  * For `Tiller[X]`:
    * `X` should be a bitfield (3 bits, so between 0 and 7) describing which tillers to set. To set tillers 0 and 2, you would set the 0th and 2nd bits for `X=0b101=5`
    * `{value}` should be either `"STOP"` or a decimal integer between 0 and 100 specifying the target height of the hitch
  * For `Sprayer[XX]`:
    * `XX` should be a hexadecimal bitfield (8 bits, so between 0 and FF) describing which sprayers to set. To set sprayers 7, 3, 2, and 1, you would set the 7th, 3rd, 2nd, and 1st bits for `XX=0b10001110=8E`
    * `{value}` should be either `"ON"` or `"OFF"`
* `Process #XXXXX`: Sends a processing command (this message is invalid in diagnostics mode)
  * `#XXXXX` is a 5-digit sequence of hexadecimal digits
  * The first (most significant) character represents tiller 0 (left of left row)
  * The second character represents the left row of sprayers
  * The third character represents tiller 1 (in between the two rows)
  * The fourth character represents the right row of sprayers
  * The last (least significant) character represents tiller 2 (right of right row)
  * Each hex digit corresponds to plant identification as follows:
    * Bit 0 (least-significant bit) signifies foxtail
    * Bit 1 signifies cocklebur
    * Bit 2 signifies ragweed
    * Bit 3 (most-significant bit) signifies corn that needs fertilizer (ignored for in between rows)
* `ProcessRaiseHitch` - used at the end of a row; stops the tillers and sprayers and raises hitch. Invalid in diagnostics mode.
* `ProcessLowerHitch` - used at the beginning of a row; lowers the hitch and then enables the tillers and sprayers. Invalid in diagnostics mode.

### Responses (from the API to the client computer, as a result of a command)
* The API will never speak unless spoken to.
* If a command is invalid, the API will return an error message, as helpful as reasonably possible in 63 characters, describing the issue.
* Most commands will not result in a response. In such cases, the lack of an error message (i.e. the receipt of an empty line) means that the command was successful.
* The following commands will lead to responses as follows:
* `GetState Mode` - response is either `"Processing"` or `"Diagnostics"`
* `GetState Configuration[setting]` - response is just a decimal number between 0 and 65535 with the specified setting's value. See "Configuration Settings" below for more information
* `GetState Sprayer[ID]` - response looks like `<state> [until]`.
  * `<state>` (required): either `"ON"` or `"OFF"`
  * `[until]` (optional): the amount of time the sprayer is scheduled to remain in that state. If unspecified, the sprayer will remain in its current state indefinitely.
  * Examples: `"ON 1200"`; `"OFF"`
* `GetState Hitch` - response is a [JSON](www.json.org) object
  * Schema is as follows:
    * `height` (required): the actual, physical height of the hitch; an integer from 0 to 100
    * `target` (required): the target height of the hitch; an integer from 0 to 100 or the string `"STOP"`
    * `dh` (required): short for "delta height"; -1 means lowering, 0 means stopped, and 1 means raising
  * Examples:
    * `{"target":80,"height":50,"dh":1}`
	* `{"height":20,"target":"STOP","dh":0}`
* `GetState Tiller[ID]` - response is a [JSON](www.json.org) object
  * Schema is as follows:
    * `height` (required): the actual, physical height of the tiller; an integer from 0 to 100
    * `target` (required): the target height of the tiller; an integer from 0 to 100 or the string `"STOP"`
    * `dh` (required): short for "delta height"; -1 means lowering, 0 means stopped, and 1 means raising
    * `until` (optional): the number of milliseconds until the `target` property changes automatically; if this property is absent, the tiller will remain in its current state indefinitely.
  * Examples:
    * `{"height":50,"target":"STOP","dh":0}`
    * `{"height":100,"dh":-1,"target":0,"until":972}`

### Configuration Settings
Setting Name | Description | Range
-------------|-------------|------
Precision | The length of time, in milliseconds, the tiller will be lowered or the sprayer will be on to eliminate a single weed. Lower values mean more efficiency but tighter timing. | 0-65535
KeepAliveTimeout | The length of time, in milliseconds, the controller will wait without receiving a message before engaging the estop. Should be set to 2000 or a similar small default. | 0-65535
ResponseDelay | The length of time, in milliseconds, between when the Arduino learns of the weed and when the weed actually passes beneath the tillers/sprayers. The exact value will be a function of vehicle speed. | 0-65535
TillerAccuracy | The percentage by which the tiller's actual height and target height may differ. For example, if set to 5, and the tiller’s target height is 30, the controller will target a height between 25 and 35. | 0-100
TillerRaiseTime | The amount of time, in milliseconds, for a tiller to raise from 0 to 100. | 0-65535
TillerLowerTime | The amount of time, in milliseconds, for a tiller to lower from 100 to 0. | 0-65535
TillerLoweredHeight | The height of the tiller when it is considered "lowered" in the processing state. | 0-100
TillerRaisedHeight | The height of the tiller when it is considered "raised" in the processing state. | 0-100
HitchLoweredHeight | The height of the hitch when lowered for processing. | 0-100
HitchRaisedHeight | The height of the hitch when raised at the end of a row. | 0-100