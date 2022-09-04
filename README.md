# agBot Firmware

Hi! You're looking at some of the source code for Purdue's agBOT challenge entry. All this code runs on an Arduino Mega board on the trailer. It receives commands with weed positions from the computer at the front of the bot, and kills weeds (via till or spray) with a targeted burst at exactly the right time. It also controls most of the electrical components on the trailer, including the LIDAR height sensors, GPS receiver, 3-point hitch actuators, and throttle.

See below for developer documentation, including install instructions to run & modify this code.

## Background: the agBot challenge

The [agBot weed and feed competition](www.agbot.ag) is a competition to autonomously identify and eliminate weeds in a cornfield. This code was part of Purdue's winning entry in 2019, and runs as part of a larger system including (1) a self-driving UTV; (2) a computer that uses image recognition to identify weeds as it drives; and (3) a trailer with tillers and sprayers for killing the weeds.

## API Spec
The current version of the code is controlled over an HTTP RESTful API. See [the API spec](./api.md) for details.

HTTP-over-Arduino requires a lot of custom code (to parse HTTP messages), but most of that code is already in place, and should be reusable for future features. The advantage to this approach is that other computers can access the controls quite easily via standard HTTP libraries (`curl` in Bash; `requests` in Python, etc), rather than mucking around with TCP. In theory, it should even be possible to build a web interface that leverages this API to easily control the bot manually from a web browser. (This has not yet been done).

## Developer Documentation

To get started running this code, first clone this GitHub repository. Then, you'll need to setup your environment:
### Environment Setup
This project uses Microchip Studio, so the setup is a bit more complex than what you may be used to from the Arduino IDE. The advantage to this is that Microchip Studio adds some power features, and allows you to split code across multiple files.

To set up, first [download and install Microchip Studio](https://www.microchip.com/en-us/tools-resources/develop/microchip-studio). Unfortunately this is Windows-only; I'm not aware of any Mac- or Linux-friendly workarounds.

Open Microchip Studio and go to "File -> Open -> Project/Solution". In the dialog that opens, find your cloned copy of the repository, and open "AgbotFW.atsln". You should be able to build the code by selecting "Build -> Build Solution."

To push code to an Arduino, you'll need a command-line tool called `avrdude.exe`. There are probably many ways to get this tool, but the way I recommend is to just [download the Arduino IDE](https://www.arduino.cc/en/software), as it has avrdude bundled along with it. On my laptop, it's in `C:\Program Files (x86)\arduino-1.8.16\hardware\tools\avr\bin\avrdude.exe`.

Once you have avrdude, you need a way to run it from Microchip Studio. To do this, open the AgbotFW solution in Microchip Studio, and go to "Tools -> External Tools." Choose "Add" to define your own external tool. Then enter the following information:
- Name: a name for the action, for example "Deploy to Arduino Mega"
- Command: `C:\path\to\ArduinoIDE\hardware\tools\avrdude.exe`
- Arguments:  `-C"C:\path\to\ArduinoIDE\hardware\tools\avr/etc/avrdude.conf" -v -patmega2560 -cwiring -P<COM_PORT> -b115200 -D -Uflash:w:"$(ProjectDir)Debug\$(TargetName).hex":i`. `-P<COM_PORT>` should be replaced with the COM port your Arduino is connected to; for example, `-PCOM3`.
  - To find the COM port, first plug in the Arduino, and then open Device Manager, and expand "Ports (COM & LPT)". See [this page](https://support.arduino.cc/hc/en-us/articles/360016420140-COM-port-number-changes-when-connecting-board-on-different-ports-or-in-bootloader-mode) for troubleshooting help.
- Initial Directory: leave blank
- Check the "Use Output Window" checkbox

Select "OK", then you should see an extra option appear under the "Tools" menu with the name of your newly defined tool. Click it to deploy code to the Arduino!

### Project Overview:
You can navigate through the folders using the Solution Explorer. The code is divided into projects:
* `agbot` - contains most of the brains of the bot. Start in `Sketch.cpp` to get a walkthrough; it's the main file, and it contains the `setup` and `loop` functions that you may recognize from other Arduino sketches. Modules are defined for different components (tillers, sprayers, hitch, API, etc), and `Sketch.cpp` just calls them all in sequence, over and over again.
  * `BenchTests.cpp` has something approaching unit tests, though they're very incomplete.
  * `Common` defines an assert library, and timers.
  * `Config` defines constants for things like timing information. These can be configured over the API, but remain saved via EEPROM when the Arduino reboots.
  * `Devices.h` just declares all the modules at once as logical devices.
  * `Estop` defines logic to throw a relay disconnecting power to all devices. Use with care. This should really only be done if the API requests it, but the API endpoint is currently not implemented.
  * `Hitch` controls raising and lowering the 3-point hitch, as well as the clutch.
  * `Http` contains a lot of string parsing to implement the HTTP protocol. Hopefully all this "just works" and you don't need to touch any of it.
  * `HttpApi` leverages `Http` to define endpoints that are called when specific URL's are called.
  * `HttpApi_Parsing` parses and validates JSON messages for `HttpApi`.
  * `jsmn` is a widely-used JSON parsing library for embedded C. See [the Github repo](https://github.com/zserge/jsmn).
  * `LidarLiteV3` contains code to connect to the LIDAR height sensors.
  * `Log` contains logging macros `LOG_ERROR`, `LOG_WARNING`, `LOG_INFO`, `LOG_DEBUG`, and `LOG_VERBOSE`. You can view the logs by connecting to the Arduino over serial.
    * By default, all messages are logged. You can configure this under "Project -> agbot Properties" from the toolbar; go to Toolchain, select "AVR/GNU C++ Compiler -> Symbols", and replace `LOGGING_VERBOSE` with `LOGGING_INFO`, to only log `INFO` messages and above.
  * `Sketch.cpp` is the main file, containing the `setup` and `loop` functions. It essentially just calls all the other modules in sequence.
  * `Sprayer` controls the 8 sprayers.
  * `Throttle` controls the throttle actuator.
  * `Tillers` controls the 3 tillers.
* `ArduinoCore` contains Arduino-written libraries and functions like `digitalRead()`, `digitalWrite()`, etc. Please don't modify anything in here.
* `Sparkfun_Ublox_Arduino_Library` is a clone of [https://github.com/sparkfun/SparkFun_Ublox_Arduino_Library](https://github.com/sparkfun/SparkFun_Ublox_Arduino_Library). This can be used for talking to the GPS, but that hasn't been implemented yet.
* `Ethernet` is forked from [the official Arduino Ethernet library](https://github.com/arduino-libraries/Ethernet). I've added a small, custom modification that lives at [https://github.com/troyhonegger/Ethernet](https://github.com/troyhonegger/Ethernet), and significantly speeds up the HTTP server. You shouldn't need to modify anything here.

### Current Gaps:
* **Height sensor calibration:** test to see if needed first. The sensors are working fine but I haven't tested their accuracy.
* **Tiller/Height sensor integration:** if a tiller's `targetHeight` is `RAISED` or `LOWERED`, each tiller should look at its corresponding height sensor, and adjust its height based on the reading, so it remains just above (or just below) the ground.
* **E-stop API:** The e-stop itself exists, and the API endpoint is spec'ed out, but it hasn't been implemented.
* **Throttle electromechanical integration:** Some logic is in-place, but it has never been tested with the physical throttle actuator. There are no limit switches on that actuator, so need to make sure it never over-extends or over-retracts.
* **GPS integration:** we have a Sparkfun GPS unit, and a library to connect to it. We just need to integrate with that library, and create an API endpoint so the server can pull GPS information from the Arduino over HTTP. That way, the server can know where it is.
* **Single-page HTML controller:** this would be a web interface that uses the already-defined HTTP API to control the Arduino, and present a simple diagnostics interface. The code (HTML/Javascript/CSS) for this webpage may be too much to store on the Arduino, so it may have to reside on the server. This is a bit of a stretch goal.
* Search for `TODO` comments in the code to find other known gaps. A lot of them are related to hardware-level tweaking or configuration - pin mappings, active high vs active low, IP address settings, etc.
