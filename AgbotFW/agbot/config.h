/*
 * Config.h
 * There are a handful of settings that need to be stored in EEPROM so that they can be configured at runtime and
 * persisted across application instances. This header defines functions for reading and writing those values. In general,
 * the "getter" functions can be called anytime - it is expected that the implementation will buffer the settings,
 * so there is no data read overhead. The "setter" functions, however, do have overhead and should not be called except
 * upon request from the client computer.
 * Created: 11/21/2018 8:09:00 PM
 *  Author: troy.honegger
 */

#ifndef CONFIG_H_
#define CONFIG_H_

// Initializes the configuration settings by loading them from EEPROM into RAM. This should be called on startup -
// if it isn't, the settings may be initialized to random values.
void initConfig(void);

// This setting represents the length, in milliseconds, of the window for weed elimination - smaller values mean more precision.
// For example, if the precision is 500, the tillers will be lowered and the sprayers will be activated for 500 milliseconds
// around the weed. The time interval is centered around total_delay, so if total_delay is 1250, the window will begin 1000ms
// after receiving the message and end 1500ms after receiving the message.
unsigned long getPrecision(void);
void setPrecision(unsigned long);

// This setting represents the total length of time between when the Arduino is notified of the weed and when the weed is eliminated.
unsigned long getTotalDelay(void);
void setTotalDelay(unsigned long);

// This setting represents how closely the tillers should be kept to their target value. A value of 10 allows a +/-10% variation,
// a value of 5 allows a +/-5% variation, etc. This must be a number between 0 and 100.
uint8_t getTillerAccuracy(void);
void setTillerAccuracy(uint8_t);

// This setting represents the amount of time, in milliseconds, the tiller takes to raise.
unsigned long getTillerRaiseTime(void);
void setTillerRaiseTime(unsigned long);

// This setting represents the amount of time, in milliseconds, the tiller takes to lower.
unsigned long getTillerLowerTime(void);
void setTillerLowerTime(unsigned long);

#endif /* CONFIG_H_ */