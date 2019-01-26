# agBot Firmware

### Background: the agBot challenge

The [agBot weed and feed competition](www.agbot.ag) is an annual competition to autonomously identify and
eliminate weeds in a cornfield. The 2019 competition specifically involves an implement that can recognize and
differentiate between corn and three distinct weed types, and apply fertilizer to the corn while eliminating the
weeds. See [the competition site](http://www.agbot.ag/weed-feed-competition-2019-2/) for more information.

### That's where we come in

This repository is part of Purdue University's entry to the agBot weed and feed competition. Our
implementation consists a UTV with three primary components:
1. A navigation and speed control system (currently using a Trimble GPS unit) to steer the machine
2. A set of cameras, attached to the front of the UTV, connected to a Linux computer that performs image recognition
3. An implement on the rear of the UTV that uses sprayers and tillers to eliminate the weeds as per the Linux computer's instructions

This repository is the software component for part (3) of this system. The code runs on an Arduino that
decodes the messages from the computer and uses them to control the tillers and sprayers.
