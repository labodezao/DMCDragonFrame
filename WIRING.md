# DMC-Lite Wiring Guide for Arduino Giga R1

## Overview

This guide provides complete pin wiring information for the dmc-lite firmware on Arduino Giga R1, including all 32 motor outputs and input configurations.

## Important Notes

- **All signals are 3.3V TTL logic**
- **If your stepper drivers require 5V signals**, use voltage level shifters
- **Double-check all connections** before powering on
- **Recommended: Add a kill switch** for emergency motor stop

## Table of Contents

1. [Motor Outputs (32 Motors)](#motor-outputs-32-motors)
2. [Input Pins](#input-pins)
3. [Camera Trigger Outputs](#camera-trigger-outputs)
4. [Logic Outputs](#logic-outputs)
5. [Optional Features](#optional-features)
6. [Wiring Best Practices](#wiring-best-practices)

---

## Motor Outputs (32 Motors)

Each motor requires two signals: **STEP** (pulse) and **DIR** (direction).

### Motors 1-8 (Pins 22-37)

| Motor | Step Pin | Dir Pin | Notes |
|-------|----------|---------|-------|
| 1 | 22 | 23 | First motor channel |
| 2 | 24 | 25 | |
| 3 | 26 | 27 | |
| 4 | 28 | 29 | |
| 5 | 30 | 31 | |
| 6 | 32 | 33 | |
| 7 | 34 | 35 | |
| 8 | 36 | 37 | Last of first bank |

### Motors 9-16 (Pins 38-53)

| Motor | Step Pin | Dir Pin | Notes |
|-------|----------|---------|-------|
| 9  | 38 | 39 | |
| 10 | 40 | 41 | |
| 11 | 42 | 43 | |
| 12 | 44 | 45 | |
| 13 | 46 | 47 | |
| 14 | 48 | 49 | |
| 15 | 50 | 51 | |
| 16 | 52 | 53 | Last of second bank |

### Motors 17-24 (Pins 54-69)

| Motor | Step Pin | Dir Pin | Notes |
|-------|----------|---------|-------|
| 17 | 54 | 55 | |
| 18 | 56 | 57 | |
| 19 | 58 | 59 | |
| 20 | 60 | 61 | |
| 21 | 62 | 63 | |
| 22 | 64 | 65 | |
| 23 | 66 | 67 | |
| 24 | 68 | 69 | Last of third bank |

### Motors 25-32 (Pins 70-85)

| Motor | Step Pin | Dir Pin | Notes |
|-------|----------|---------|-------|
| 25 | 70 | 71 | |
| 26 | 72 | 73 | |
| 27 | 74 | 75 | |
| 28 | 76 | 77 | |
| 29 | 78 | 79 | |
| 30 | 80 | 81 | |
| 31 | 82 | 83 | |
| 32 | 84 | 85 | Last motor channel |

**Pin Pattern:** All motor outputs use alternating pins (even = STEP, odd = DIR)

---

## Input Pins

### Analog Inputs (A0-A11)

The Arduino Giga R1 has 12 analog input channels with 16-bit resolution.

| Channel | Pin | Use Cases |
|---------|-----|-----------|
| A0 | A0 | Position encoder, potentiometer |
| A1 | A1 | Temperature sensor |
| A2 | A2 | Pressure sensor |
| A3 | A3 | Current sensor |
| A4 | A4 | Distance sensor |
| A5 | A5 | Custom sensor |
| A6 | A6 | Custom sensor |
| A7 | A7 | Custom sensor |
| A8 | A8 | Custom sensor |
| A9 | A9 | Custom sensor |
| A10 | A10 | Custom sensor |
| A11 | A11 | Custom sensor |

**Reading:** Use DMC_MSG_ANALOG_IN (0x0301) protocol command
**Resolution:** 16-bit (0-65535 range)
**Voltage:** 0-3.3V input range

### Limit Switch Inputs (Optional - 16 switches)

Configure in `dmc_m7/config.h` by uncommenting the defines:

| Switch | Pin | Motor | Type |
|--------|-----|-------|------|
| Low 1  | D51 | 1 | Low limit |
| High 1 | D52 | 1 | High limit |
| Low 2  | D53 | 2 | Low limit |
| High 2 | D54 | 2 | High limit |
| Low 3  | D55 | 3 | Low limit |
| High 3 | D56 | 3 | High limit |
| Low 4  | D57 | 4 | Low limit |
| High 4 | D58 | 4 | High limit |
| Low 5  | D59 | 5 | Low limit |
| High 5 | D60 | 5 | High limit |
| Low 6  | D61 | 6 | Low limit |
| High 6 | D62 | 6 | High limit |
| Low 7  | D63 | 7 | Low limit |
| High 7 | D64 | 7 | High limit |
| Low 8  | D65 | 8 | Low limit |
| High 8 | D66 | 8 | High limit |

**Wiring:** Connect switch between pin and GND (uses internal pull-up)
**Logic:** Active low (closed switch = limit reached)
**Reading:** Use DMC_MSG_LIMIT_SWITCH_STATUS (0x0302)
**Auto-stop:** Firmware automatically halts motors at limits

---

## Camera Trigger Outputs

Camera triggers should be connected through relays to handle camera voltages.

| Signal | Pin | Purpose |
|--------|-----|---------|
| Meter | D52 | Camera meter/exposure start |
| Shutter | D53 | Camera shutter trigger |

**Important:** These are 3.3V outputs. Use appropriate relays for your camera voltage requirements.

---

## Logic Outputs

General-purpose logic outputs for triggers, relays, or external equipment.

| Output | Pin | Use Cases |
|--------|-----|-----------|
| LOGIC_OUT_0 | D40 | External trigger, relay control |
| LOGIC_OUT_1 | D41 | External trigger, relay control |

**Voltage:** 3.3V TTL
**Current:** Standard Arduino pin current (check datasheet)
**Control:** Via Dragonframe motion control software

---

## Optional Features

### Kill Switch / Emergency Stop

**Recommended for safety!** Stops all motors instantly.

```cpp
// In dmc_m7/config.h, uncomment:
#define KILL_SWITCH_PIN  D48
```

**Wiring:** Connect normally-open pushbutton between D48 and GND
**Logic:** Uses internal pull-up, triggers on pin LOW

### Logic Input Switch

General-purpose input for external switch feedback.

```cpp
// In dmc_m7/config.h, uncomment:
#define LOGIC_SWITCH_PIN  D49
```

**Wiring:** Connect switch between D49 and GND (uses pull-up)
**Reading:** Use DMC_MSG_GIO_IN (0x0022)

### Fan Control (PWM)

PWM output for cooling stepper motor drivers.

```cpp
// In dmc_m7/config.h, uncomment:
#define FAN_PWM_PIN  D50
```

**Wiring:**
1. Arduino D50 → MOSFET Gate
2. MOSFET Drain → Fan Negative
3. MOSFET Source → Ground
4. Fan Positive → +12V/24V (add flyback diode!)

**Control:** Use DMC_MSG_FAN_CONTROL (0x0300)

---

## Wiring Best Practices

### 1. Stepper Driver Connections

**Typical Stepper Driver Interface:**
```
Arduino Pin 22 (STEP1) → Driver STEP+ (with level shifter if needed)
Arduino Pin 23 (DIR1)  → Driver DIR+ (with level shifter if needed)
Arduino GND            → Driver STEP-, DIR-, GND
```

**Recommended Drivers:**
- Geckodrive (professional, accepts 3.3V)
- TMC2209 (accepts 3.3V)
- DRV8825 (may need level shifter)
- A4988 (may need level shifter)

### 2. Level Shifting (If Required)

If your drivers need 5V logic:
```
Arduino 3.3V → 74HCT245 (or similar) → Driver 5V inputs
```

**Popular level shifters:**
- 74HCT245 (8-channel bidirectional)
- TXS0108E (8-channel, auto-direction)
- 74AHCT125 (quad buffer)

### 3. Power Supply Considerations

- **Arduino power:** USB or 7-12V barrel jack
- **Stepper power:** Separate supply for motors (12-48V typical)
- **Ground all systems together:** Common GND is essential
- **Isolate motor power:** Keep high-current motor supply separate

### 4. Cable Length and Noise

- **Keep step/dir cables short** (< 3 feet if possible)
- **Use shielded or twisted-pair cables** for step/dir signals
- **Add 100Ω series resistors** at driver inputs if noise is an issue
- **Add 0.1µF capacitors** near each driver's logic inputs

### 5. Emergency Stop Wiring

**Example kill switch circuit:**
```
[Push Button]
    |
    +--- Arduino D48
    |
   GND
```

**For safety:** Consider adding hardware e-stop that cuts motor power directly.

### 6. Protection

- **Add flyback diodes** on all inductive loads (relays, fans)
- **Fuse the motor power supply** appropriately
- **Don't exceed pin current limits** (check STM32H747 datasheet)
- **Use optoisolators** for noisy environments

---

## Pin Availability and Conflicts

### Pins Used by dmc-lite

**Motor outputs:** 22-85 (64 pins for 32 motors)
**Camera triggers:** D52-D53
**Logic outputs:** D40-D41
**Optional inputs:** D48-D66 (limit switches, kill switch, etc.)
**Analog inputs:** A0-A11

### Available for Custom Use

The Arduino Giga R1 has 76 digital I/O pins total. After allocating:
- 64 pins for motor outputs (22-85)
- 2 pins for camera (D52-D53)
- 2 pins for logic outputs (D40-D41)
- Up to 19 pins for optional features (D48-D66)

**Remaining pins:** Approximately 10-20 pins available for custom applications, depending on which optional features you enable.

### Pin Conflicts to Avoid

**Note:** Some pins may conflict with board functions:
- **Serial/USB:** Already allocated to Serial
- **SPI:** May be needed for SD card or other peripherals
- **I2C:** May be needed for displays or sensors

Check Arduino Giga R1 pinout diagram for detailed pin functions.

---

## Testing Your Wiring

### Step 1: Power-On Test

1. Connect Arduino via USB (no motors yet)
2. Upload dmc-lite firmware
3. Check LED indicators:
   - Green LED: Blinking = M4 core running
   - Red LED: Solid = Error, Blinking = SDRAM issue

### Step 2: Connection Test

1. Connect Dragonframe to Arduino
2. Use Dragonframe's "Test Motors" function
3. Verify step/dir signals with oscilloscope or multimeter

### Step 3: Single Motor Test

1. Connect ONE motor with driver
2. Set driver to low microstepping (e.g., 200 steps/rev)
3. Jog motor slowly in Dragonframe
4. Verify smooth motion in both directions

### Step 4: Full System Test

1. Connect all motors
2. Test each motor individually
3. Verify limit switches (if installed)
4. Test emergency stop (if installed)

---

## Troubleshooting

### Motors Not Moving

- Check power supply to drivers
- Verify step/dir connections
- Check driver enable signal
- Test with oscilloscope: should see pulses on STEP pins

### Erratic Motion

- Add series resistors (100Ω) to reduce noise
- Shorten or shield step/dir cables
- Check for ground loops
- Verify driver microstepping settings

### Arduino Not Responding

- Check USB connection
- Verify firmware uploaded to both M7 and M4 cores
- Check for rapid LED blinking (SDRAM error)
- Try rebooting Arduino

### Limit Switches Not Working

- Verify pull-up resistor configuration in config.h
- Test switch continuity with multimeter
- Check pin definitions match your wiring
- Use DMC_MSG_LIMIT_SWITCH_STATUS to read status

---

## Reference: Complete Pin Map Summary

### Arduino Giga R1 - DMC-Lite Pin Assignments

```
Motors 1-8:   Pins 22-37  (STEP even, DIR odd)
Motors 9-16:  Pins 38-53  (STEP even, DIR odd)
Motors 17-24: Pins 54-69  (STEP even, DIR odd)
Motors 25-32: Pins 70-85  (STEP even, DIR odd)

Camera:       D52 (Meter), D53 (Shutter)
Logic Out:    D40, D41
Kill Switch:  D48 (optional)
Logic In:     D49 (optional)
Fan PWM:      D50 (optional)
Limits:       D51-D66 (optional, 16 switches)
Analog In:    A0-A11 (12 channels)
```

---

## Additional Resources

- **Arduino Giga R1 Pinout:** https://docs.arduino.cc/hardware/giga-r1-wifi
- **Dragonframe Manual:** Motion Control chapter
- **DMC-32 Reference:** https://www.dragonframe.com/product/dmc-32/
- **GitHub Repository:** Check for updates and examples

---

**Document Version:** 1.0
**Last Updated:** 2026-02-18
**Firmware Version:** 1.6.0+
**Hardware:** Arduino Giga R1 WiFi (STM32H747XI)
