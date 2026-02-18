/*
 * config.h
 * dmc-lite source code for user configuration
 *
 * Note that all signals are 3.3V TTL
 *
 */

#ifdef ARDUINO_ARCH_MBED_GIGA

// Motor outputs 1-8 (pins 22-37, alternating step/dir)
#define PIN_STEP1 22
#define PIN_DIR1  23
#define PIN_STEP2 24
#define PIN_DIR2  25
#define PIN_STEP3 26
#define PIN_DIR3  27
#define PIN_STEP4 28
#define PIN_DIR4  29
#define PIN_STEP5 30
#define PIN_DIR5  31
#define PIN_STEP6 32
#define PIN_DIR6  33
#define PIN_STEP7 34
#define PIN_DIR7  35
#define PIN_STEP8 36
#define PIN_DIR8  37

// Motor outputs 9-16 (pins 38-53, alternating step/dir)
#define PIN_STEP9  38
#define PIN_DIR9   39
#define PIN_STEP10 40
#define PIN_DIR10  41
#define PIN_STEP11 42
#define PIN_DIR11  43
#define PIN_STEP12 44
#define PIN_DIR12  45
#define PIN_STEP13 46
#define PIN_DIR13  47
#define PIN_STEP14 48
#define PIN_DIR14  49
#define PIN_STEP15 50
#define PIN_DIR15  51
#define PIN_STEP16 52
#define PIN_DIR16  53

// Motor outputs 17-24 (pins 54-69, alternating step/dir)
#define PIN_STEP17 54
#define PIN_DIR17  55
#define PIN_STEP18 56
#define PIN_DIR18  57
#define PIN_STEP19 58
#define PIN_DIR19  59
#define PIN_STEP20 60
#define PIN_DIR20  61
#define PIN_STEP21 62
#define PIN_DIR21  63
#define PIN_STEP22 64
#define PIN_DIR22  65
#define PIN_STEP23 66
#define PIN_DIR23  67
#define PIN_STEP24 68
#define PIN_DIR24  69

// Motor outputs 25-32 (pins 70-85, alternating step/dir)
#define PIN_STEP25 70
#define PIN_DIR25  71
#define PIN_STEP26 72
#define PIN_DIR26  73
#define PIN_STEP27 74
#define PIN_DIR27  75
#define PIN_STEP28 76
#define PIN_DIR28  77
#define PIN_STEP29 78
#define PIN_DIR29  79
#define PIN_STEP30 80
#define PIN_DIR30  81
#define PIN_STEP31 82
#define PIN_DIR31  83
#define PIN_STEP32 84
#define PIN_DIR32  85

// camera signals should be run through a relay
#define PIN_CAM_METER D52
#define PIN_CAM_SHUTTER D53

#elif defined(ARDUINO_ARCH_MBED_PORTENTA)


#define LAST_ARDUINO_PIN_NUMBER LEDB + 1

#define PIN_STEP1 LAST_ARDUINO_PIN_NUMBER + PC_3 // SPI1_COPI
#define PIN_STEP2 LAST_ARDUINO_PIN_NUMBER + PI_1 // SPI1_CK
#define PIN_STEP3 LAST_ARDUINO_PIN_NUMBER + PG_9 // UART2_RX
#define PIN_STEP4 LAST_ARDUINO_PIN_NUMBER + PI_7 // CAMERA_D3P (CAM D7)
#define PIN_STEP5 LAST_ARDUINO_PIN_NUMBER + PI_4 // CAMERA_D2P (CAM D5)
#define PIN_STEP6 LAST_ARDUINO_PIN_NUMBER + PH_12 // CAMERA_D1P (CAM D3)
#define PIN_STEP7 LAST_ARDUINO_PIN_NUMBER + PH_10 // CAMERA_D0P (CAM D1)
#define PIN_STEP8 LAST_ARDUINO_PIN_NUMBER + PI_5  // CAMERA_CKP (CAM VS)

#define PIN_DIR1  LAST_ARDUINO_PIN_NUMBER + PC_2 // SPI1_CIPO
#define PIN_DIR2  LAST_ARDUINO_PIN_NUMBER + PI_0 // SPI1_CS
#define PIN_DIR3  LAST_ARDUINO_PIN_NUMBER + PG_14 // UART2_TX
#define PIN_DIR4  LAST_ARDUINO_PIN_NUMBER + PI_6  // CAMERA_D3N (CAM D6)
#define PIN_DIR5  LAST_ARDUINO_PIN_NUMBER + PH_14 // CAMERA_D2N (CAM D4)
#define PIN_DIR6  LAST_ARDUINO_PIN_NUMBER + PH_11 // CAMERA_D1N (CAM D2)
#define PIN_DIR7  LAST_ARDUINO_PIN_NUMBER + PH_9  // CAMERA_D0N (CAM D0)
#define PIN_DIR8  LAST_ARDUINO_PIN_NUMBER + PA_6 // CAMERA_CKN (CAM CLK)

// camera signals should be run through a relay
#define PIN_CAM_METER LAST_ARDUINO_PIN_NUMBER + PC_13   // GPIO 0
#define PIN_CAM_SHUTTER LAST_ARDUINO_PIN_NUMBER + PC_15 // GPIO 1

#define LOGIC_OUT_0 LAST_ARDUINO_PIN_NUMBER + PD_4       // GPIO 2
#define LOGIC_OUT_1 LAST_ARDUINO_PIN_NUMBER + PD_5       // GPIO 3

#else

#error "Board not supported for dmc-lite sketch"

#endif

