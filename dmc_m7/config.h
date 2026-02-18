/*
 * config.h
 * dmc-lite source code for user configuration
 *
 * Note that all signals are 3.3V TTL
 *
 */

#ifdef ARDUINO_ARCH_MBED_GIGA

#define LOGIC_OUT_0 D40
#define LOGIC_OUT_1 D41

// set a pin for an e-stop switch. uses pull-up resistor, so switch needs to connect pin to ground.
//#define KILL_SWITCH_PIN  D48

// set a pin for a logic switch input. uses pull-up resistor, so switch needs to connect pin to ground.
//#define LOGIC_SWITCH_PIN  D49

// set a pin for fan control (PWM output for cooling stepper drivers)
//#define FAN_PWM_PIN  D50

// set pins for limit switches (optional hardware limit switches for motors 1-8)
// uses pull-up resistors, so switches need to connect pins to ground
//#define LIMIT_SWITCH_LOW_1  D51
//#define LIMIT_SWITCH_HIGH_1 D52
//#define LIMIT_SWITCH_LOW_2  D53
//#define LIMIT_SWITCH_HIGH_2 D54
//#define LIMIT_SWITCH_LOW_3  D55
//#define LIMIT_SWITCH_HIGH_3 D56
//#define LIMIT_SWITCH_LOW_4  D57
//#define LIMIT_SWITCH_HIGH_4 D58
//#define LIMIT_SWITCH_LOW_5  D59
//#define LIMIT_SWITCH_HIGH_5 D60
//#define LIMIT_SWITCH_LOW_6  D61
//#define LIMIT_SWITCH_HIGH_6 D62
//#define LIMIT_SWITCH_LOW_7  D63
//#define LIMIT_SWITCH_HIGH_7 D64
//#define LIMIT_SWITCH_LOW_8  D65
//#define LIMIT_SWITCH_HIGH_8 D66

#elif defined(ARDUINO_ARCH_MBED_PORTENTA)


#define LAST_ARDUINO_PIN_NUMBER LEDB + 1


#define LOGIC_OUT_0 LAST_ARDUINO_PIN_NUMBER + PD_4       // GPIO 2
#define LOGIC_OUT_1 LAST_ARDUINO_PIN_NUMBER + PD_5       // GPIO 3

// set a pin for an e-stop switch
//#define KILL_SWITCH_PIN  LAST_ARDUINO_PIN_NUMBER + PE_3  // GPIO 4

// set a pin for a logic switch input
//#define LOGIC_SWITCH_PIN  LAST_ARDUINO_PIN_NUMBER + PG_3  // GPIO 5

// set a pin for fan control (PWM output for cooling stepper drivers)
//#define FAN_PWM_PIN  LAST_ARDUINO_PIN_NUMBER + PG_4  // GPIO 6

// set pins for limit switches (optional hardware limit switches for motors 1-8)
// uses pull-up resistors, so switches need to connect pins to ground
//#define LIMIT_SWITCH_LOW_1  LAST_ARDUINO_PIN_NUMBER + PG_5  // GPIO 7
//#define LIMIT_SWITCH_HIGH_1 LAST_ARDUINO_PIN_NUMBER + PG_6  // GPIO 8
//#define LIMIT_SWITCH_LOW_2  LAST_ARDUINO_PIN_NUMBER + PG_7  // GPIO 9
//#define LIMIT_SWITCH_HIGH_2 LAST_ARDUINO_PIN_NUMBER + PG_8  // GPIO 10
//#define LIMIT_SWITCH_LOW_3  LAST_ARDUINO_PIN_NUMBER + PG_9  // GPIO 11
//#define LIMIT_SWITCH_HIGH_3 LAST_ARDUINO_PIN_NUMBER + PG_10 // GPIO 12
//#define LIMIT_SWITCH_LOW_4  LAST_ARDUINO_PIN_NUMBER + PG_11 // GPIO 13
//#define LIMIT_SWITCH_HIGH_4 LAST_ARDUINO_PIN_NUMBER + PG_12 // GPIO 14
//#define LIMIT_SWITCH_LOW_5  LAST_ARDUINO_PIN_NUMBER + PG_13 // GPIO 15
//#define LIMIT_SWITCH_HIGH_5 LAST_ARDUINO_PIN_NUMBER + PG_14 // GPIO 16
//#define LIMIT_SWITCH_LOW_6  LAST_ARDUINO_PIN_NUMBER + PG_15 // GPIO 17
//#define LIMIT_SWITCH_HIGH_6 LAST_ARDUINO_PIN_NUMBER + PH_0  // GPIO 18
//#define LIMIT_SWITCH_LOW_7  LAST_ARDUINO_PIN_NUMBER + PH_1  // GPIO 19
//#define LIMIT_SWITCH_HIGH_7 LAST_ARDUINO_PIN_NUMBER + PH_2  // GPIO 20
//#define LIMIT_SWITCH_LOW_8  LAST_ARDUINO_PIN_NUMBER + PH_3  // GPIO 21
//#define LIMIT_SWITCH_HIGH_8 LAST_ARDUINO_PIN_NUMBER + PH_4  // GPIO 22

#else

#error "Board not supported for dmc-lite sketch"

#endif
