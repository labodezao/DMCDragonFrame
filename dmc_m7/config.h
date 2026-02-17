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
// ... define additional limit switches as needed

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
// ... define additional limit switches as needed

#else

#error "Board not supported for dmc-lite sketch"

#endif
