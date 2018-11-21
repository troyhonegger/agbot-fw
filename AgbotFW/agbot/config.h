#ifndef CONFIG_H
#define CONFIG_H

/* Initializes the configuration settings by loading them from EEPROM into RAM. This should be called on startup -
 * if it isn't, the settings may be initialized to random values. */
void init_config(void);

/* This setting represents the length, in milliseconds, of the window for weed elimination - smaller values mean more precision.
 * For example, if the precision is 500, the tillers will be lowered and the sprayers will be activated for 500 milliseconds
 * around the weed. The time interval is centered around total_delay, so if total_delay is 1250, the window will begin 1000ms
 * after receiving the message and end 1500ms after receiving the message. */
unsigned long get_precision(void);
void set_precision(unsigned long);

/* This setting represents the total length of time between when the Arduino is notified of the weed and when the weed is eliminated. */
unsigned long get_total_delay(void);
void set_total_delay(unsigned long);

/* This setting represents how closely the tillers should be kept to their target value. A value of 10 allows a +/-10% variation;
 * a value of 5 allows a +/-5% variation. This must be a number between 0 and 100. */
unsigned char get_tiller_accuracy(void);
void set_tiller_accuracy(unsigned char);

/* This setting represents the amount of time, in milliseconds, the tiller takes to raise. */
unsigned long get_tiller_raise_time(void);
void set_tiller_raise_time(unsigned long);

/* This setting represents the amount of time, in milliseconds, the tiller takes to lower. */
unsigned long get_tiller_lower_time(void);
void set_tiller_lower_time(unsigned long);

/* Since a sprayer takes (we assume) no time to turn on, this value is simply total_delay - precision / 2. It is included here so the
 * value can be buffered depending on the implementation. */
unsigned long get_sprayer_delay(void);

/* This value is simply total_delay - tiller_lower_time - precision / 2. It is included here so the value can be buffered depending on
 * the implementation. */
unsigned long get_tiller_lower_delay(void);

/* This value is the simple tiller_lower_time + precision. It is included here so the value can be buffered depending on the
 * implementation. */
unsigned long get_tiller_lowered_time(void);

#endif //CONFIG_H