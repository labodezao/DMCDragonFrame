# Professional Features Guide (v1.7.0+)

This guide documents the professional-grade features added to dmc-lite v1.7.0, transforming it from a hobbyist motion controller into a professional cinematography tool.

## Overview of Professional Features

dmc-lite v1.7.0 introduces three major professional features:

1. **SMPTE Timecode Input** - Frame-accurate synchronization with external equipment
2. **Rotary Encoder Support** - Manual control with physical encoder wheels
3. **Backlash Compensation** - Eliminates mechanical play for precise motion

These features are implemented in firmware and require minimal additional hardware.

---

## 1. SMPTE Timecode Input

### What is Timecode?

SMPTE timecode is an industry-standard method for synchronizing video, audio, and motion control equipment. It provides a unique timestamp (HH:MM:SS:FF) for every frame.

### Supported Formats

- **24 fps** (Film production)
- **25 fps** (PAL video)
- **30 fps** (NTSC non-drop)
- **29.97 fps** (NTSC drop-frame) *

*Note: 29.97 fps drop-frame compensation is approximated

### Hardware Requirements

**Minimum Setup:**
- 1 analog input pin (default: A0)
- Direct connection for TTL-level LTC (0-3.3V)

**Professional Setup:**
- LTC signal conditioner/level shifter
- Converts professional audio levels (-10 dBV to +4 dBu) to 3.3V TTL
- Example: Simple op-amp circuit or dedicated LTC-to-TTL converter

### Wiring Diagram

```
Professional LTC Source          Arduino Giga R1
┌─────────────────┐              ┌──────────────┐
│  Audio Out      │              │              │
│  (LTC Signal)   ├──────────────┤ A0 (Analog)  │
│  -10dBV to +4dBu│  Optional    │              │
└─────────────────┘  Level Shift └──────────────┘
                     Circuit
```

### Configuration Commands

#### DMC_MSG_TIMECODE_CONFIG (0x0400)

Configure timecode decoder settings.

**Packet Format:**
```
Offset | Size | Description
-------|------|------------
0      | 2    | Message ID (0x0400)
2      | 1    | Frame rate (0=24fps, 1=25fps, 2=30fps, 3=29.97fps)
3      | 1    | Sync mode (0=off, 1=read, 2=chase, 3=jam)
4      | 1    | Analog pin number (0-11 for A0-A11)
5      | 2    | Detection threshold (0-1023, default 512)
7      | 1    | Checksum
```

#### DMC_MSG_TIMECODE_STATUS (0x0401)

Query current timecode reading.

**Response Format:**
```
Offset | Size | Description
-------|------|------------
0      | 2    | Message ID (0x0401 | 0x8000)
2      | 1    | Hours (0-23)
3      | 1    | Minutes (0-59)
4      | 1    | Seconds (0-59)
5      | 1    | Frames (0-29)
6      | 1    | Frame rate
7      | 1    | Flags (bit 0: valid, bit 1: drop-frame)
8      | 4    | Total frame count (32-bit)
12     | 1    | Checksum
```

#### DMC_MSG_TIMECODE_SYNC (0x0402)

Enable/disable timecode synchronization.

**Packet Format:**
```
Offset | Size | Description
-------|------|------------
0      | 2    | Message ID (0x0402)
2      | 1    | Sync mode (0=off, 1=read, 2=chase, 3=jam)
3      | 1    | Checksum
```

### Sync Modes

| Mode | Value | Description |
|------|-------|-------------|
| **Off** | 0 | Timecode reading disabled |
| **Read** | 1 | Read and display timecode, no sync |
| **Chase** | 2 | Continuous sync - playback follows external timecode |
| **Jam** | 3 | Jam sync - lock to timecode once, then free-run |

### Typical Workflows

#### Multi-Camera Motion Control

```cpp
// Setup timecode for 24 fps film production
send_message(DMC_MSG_TIMECODE_CONFIG, {24, SYNC_CHASE, A0, 512});

// Start motion sequence
send_message(DMC_MSG_RT_RUN_MOVE, {...});

// Motion automatically syncs to external timecode
// Multiple cameras stay frame-locked
```

#### Audio-Sync Recording

```cpp
// Use jam sync for audio recorder compatibility
send_message(DMC_MSG_TIMECODE_CONFIG, {30, SYNC_JAM, A0, 512});

// Lock to timecode at start of take
// Then free-run for remainder (reduces audio interference)
```

---

## 2. Rotary Encoder Support

### What are Rotary Encoders?

Rotary encoders are electromechanical devices that convert rotation into digital signals. They provide tactile, precise manual control for positioning motors.

### Supported Encoder Types

- **Quadrature encoders** (most common)
- **Incremental encoders** with A/B phases
- Resolution: any (tested with 100-600 PPR)

### Hardware Requirements

**Per Encoder:**
- 2 digital input pins (Phase A and B)
- 5V or 3.3V compatible encoder
- Optional: Push-button for reset/zero

**Recommended Encoders:**
- Bourns PEC11 series (mechanical, 24 detents)
- Omron E6B2 series (optical, high resolution)
- CUI AMT103 series (capacitive, robust)

### Wiring Diagram

```
Rotary Encoder                   Arduino Giga R1
┌──────────────┐                ┌──────────────┐
│  Phase A     ├────────────────┤ D48          │
│  Phase B     ├────────────────┤ D49          │
│  Common/GND  ├────────────────┤ GND          │
│  +V (opt)    ├────────────────┤ 3.3V         │
└──────────────┘                └──────────────┘
```

### Configuration Commands

#### DMC_MSG_ENCODER_CONFIG (0x0410)

Configure an encoder for a specific motor.

**Packet Format:**
```
Offset | Size | Description
-------|------|------------
0      | 2    | Message ID (0x0410)
2      | 1    | Encoder index (0-7)
3      | 1    | Phase A pin number
4      | 1    | Phase B pin number
5      | 1    | Motor index (0-31)
6      | 2    | Scale factor (signed, steps per encoder count)
8      | 1    | Mode (0=direct position, 1=jog speed)
9      | 1    | Checksum
```

**Scale Factor Examples:**
- `1` = 1 motor step per encoder click (1:1)
- `10` = 10 motor steps per encoder click (fine control)
- `100` = 100 motor steps per encoder click (coarse control)
- `-10` = Reverse direction

#### DMC_MSG_ENCODER_STATUS (0x0411)

Query encoder position.

**Response Format:**
```
Offset | Size | Description
-------|------|------------
0      | 2    | Message ID (0x0411 | 0x8000)
2      | 1    | Encoder index
3      | 4    | Position (signed 32-bit)
7      | 1    | Checksum
```

#### DMC_MSG_ENCODER_RESET (0x0412)

Reset encoder position to zero.

**Packet Format:**
```
Offset | Size | Description
-------|------|------------
0      | 2    | Message ID (0x0412)
2      | 1    | Encoder index (0-7, or 0xFF for all)
3      | 1    | Checksum
```

### Control Modes

| Mode | Description | Use Case |
|------|-------------|----------|
| **Direct Position** | Encoder directly sets motor position | Frame-by-frame animation |
| **Jog Speed** | Encoder sets jog velocity | Live motion control |

### Typical Workflows

#### Stop-Motion Animation

```cpp
// Configure encoder for frame-by-frame control
// Motor 0, scale factor 1000 (1 frame = 1 encoder click)
send_message(DMC_MSG_ENCODER_CONFIG, {0, 48, 49, 0, 1000, MODE_DIRECT});

// Animator turns encoder to position motor
// Each click moves exactly 1 frame
```

#### Focus Pulling

```cpp
// Configure encoder for smooth focus control
// Motor 2 (focus motor), scale factor 50 (fine control)
send_message(DMC_MSG_ENCODER_CONFIG, {0, 48, 49, 2, 50, MODE_JOG});

// Focus puller turns encoder during shot
// Motor speed matches encoder rotation speed
```

---

## 3. Backlash Compensation

### What is Backlash?

Backlash is mechanical play (slack) in gears, belts, or lead screws. When direction reverses, the motor must take up this slack before the load moves, causing position errors.

### How Compensation Works

The firmware tracks movement direction for each motor. When direction reverses:

1. Detect direction change
2. Add extra steps equal to backlash value
3. Resume normal positioning
4. Transparent to host software

### Configuration Commands

#### DMC_MSG_MOTOR_SET_BACKLASH (0x0420)

Set backlash compensation value for a motor.

**Packet Format:**
```
Offset | Size | Description
-------|------|------------
0      | 2    | Message ID (0x0420)
2      | 1    | Motor index (0-31)
3      | 4    | Backlash value in motor steps (signed 32-bit)
7      | 1    | Checksum
```

**Value Examples:**
- `0` = No compensation (default)
- `100` = Add 100 steps on direction reversal
- `500` = Add 500 steps (for worn gear systems)

#### DMC_MSG_MOTOR_GET_BACKLASH (0x0421)

Query backlash setting for a motor.

**Response Format:**
```
Offset | Size | Description
-------|------|------------
0      | 2    | Message ID (0x0421 | 0x8000)
2      | 1    | Motor index
3      | 4    | Backlash value (signed 32-bit)
7      | 1    | Checksum
```

### Measuring Backlash

**Method 1: Visual Inspection**
1. Move motor forward until load just starts to move
2. Note position
3. Reverse motor until load moves backward
4. Note position
5. Difference = backlash in steps

**Method 2: Dial Indicator**
1. Mount dial indicator on moving axis
2. Move forward 1000 steps, note indicator
3. Move backward 1000 steps
4. Position error = backlash

### Typical Values

| System Type | Typical Backlash |
|-------------|------------------|
| **Precision ball screw** | 10-50 steps |
| **Standard lead screw** | 50-200 steps |
| **Belt drive (GT2)** | 100-300 steps |
| **Gear reduction** | 200-500 steps |
| **Worn/loose system** | 500-2000 steps |

### Best Practices

1. **Measure per axis** - Each motor may have different backlash
2. **Re-measure periodically** - Backlash increases with wear
3. **Minimize mechanically** - Preload bearings and adjust belt tension
4. **Over-compensate slightly** - Better to overshoot than undershoot
5. **Test bidirectional moves** - Verify smooth motion in both directions

---

## Hardware Setup Examples

### Example 1: Professional Stop-Motion Rig

**Components:**
- 4× rotary encoders (one per axis)
- 1× LTC timecode input
- Arduino Giga R1

**Configuration:**
```cpp
// Configure encoders for 4 axes
send_message(DMC_MSG_ENCODER_CONFIG, {0, 48, 49, 0, 100, MODE_DIRECT});  // X-axis
send_message(DMC_MSG_ENCODER_CONFIG, {1, 50, 51, 1, 100, MODE_DIRECT});  // Y-axis
send_message(DMC_MSG_ENCODER_CONFIG, {2, 52, 53, 2, 100, MODE_DIRECT});  // Z-axis
send_message(DMC_MSG_ENCODER_CONFIG, {3, 54, 55, 3, 50, MODE_JOG});      // Focus (fine control)

// Configure backlash for belt-driven X/Y axes
send_message(DMC_MSG_MOTOR_SET_BACKLASH, {0, 200});  // X-axis
send_message(DMC_MSG_MOTOR_SET_BACKLASH, {1, 200});  // Y-axis

// Configure timecode for multi-camera sync
send_message(DMC_MSG_TIMECODE_CONFIG, {24, SYNC_CHASE, A0, 512});
```

### Example 2: Time-Lapse with Audio Sync

**Components:**
- Arduino Giga R1
- 1× LTC input from audio recorder

**Configuration:**
```cpp
// Jam sync to audio recorder timecode
send_message(DMC_MSG_TIMECODE_CONFIG, {25, SYNC_JAM, A0, 512});

// Enable sync at start of sequence
send_message(DMC_MSG_TIMECODE_SYNC, {SYNC_JAM});

// After lock, system free-runs (reduces EMI on audio)
```

---

## Performance Considerations

### CPU Usage

| Feature | Update Rate | CPU Load | Notes |
|---------|-------------|----------|-------|
| **Timecode** | ~2400 Hz | ~2% | Analog sampling |
| **Encoders (×4)** | 50 Hz poll | ~1% | Can use interrupts |
| **Backlash** | Event-driven | <0.1% | Only on direction change |
| **Total** | - | ~3% | Minimal impact |

### Memory Usage

| Feature | RAM Usage | SDRAM Usage | Notes |
|---------|-----------|-------------|-------|
| **Timecode** | ~16 KB | 0 | Decoder state |
| **Encoders (×8)** | ~4 KB | 0 | Position tracking |
| **Backlash (×32)** | ~128 bytes | 0 | 4 bytes per motor |
| **Total** | ~20 KB | 0 | 3% of free internal RAM |

### Latency

- **Timecode sync**: <1 frame (20-40 ms)
- **Encoder response**: <20 ms (50 Hz update rate)
- **Backlash comp**: 0 ms (applied in motion planner)

---

## Troubleshooting

### Timecode Issues

**Problem:** "No timecode detected"
- Check LTC signal level (should be 0-3.3V on analog pin)
- Verify frame rate setting matches source
- Try adjusting threshold value (lower for weak signals)

**Problem:** "Timecode jitter/unstable"
- Check for electrical noise near analog input
- Use shielded cable for LTC signal
- Add 100nF capacitor across analog input to GND

**Problem:** "Timecode valid but sync not working"
- Verify sync mode is set to CHASE or JAM
- Check that RT_RUN_MOVE was called after enabling sync
- Ensure timecode frame rate matches motion sequence frame rate

### Encoder Issues

**Problem:** "Encoder counts wrong direction"
- Swap Phase A and B pins in configuration
- Or use negative scale factor to reverse

**Problem:** "Encoder skips counts"
- Increase polling rate (use hardware interrupts)
- Check for electrical noise on encoder lines
- Add 10K pull-up resistors to encoder outputs

**Problem:** "Encoder position drifts"
- Check for bounce on mechanical encoders (add debounce)
- Verify encoder has good electrical contact
- Test with slower rotation speed

### Backlash Issues

**Problem:** "Over-compensation (motor overshoots)"
- Reduce backlash value by 10-20%
- Verify mechanical system isn't binding

**Problem:** "Under-compensation (still see errors)"
- Increase backlash value by 20-30%
- Re-measure backlash with dial indicator
- Check for loose mechanical components

---

## API Reference Summary

### Message IDs (v1.7.0)

```cpp
// Timecode
#define DMC_MSG_TIMECODE_CONFIG 0x0400
#define DMC_MSG_TIMECODE_STATUS 0x0401
#define DMC_MSG_TIMECODE_SYNC   0x0402

// Encoders
#define DMC_MSG_ENCODER_CONFIG  0x0410
#define DMC_MSG_ENCODER_STATUS  0x0411
#define DMC_MSG_ENCODER_RESET   0x0412

// Backlash
#define DMC_MSG_MOTOR_SET_BACKLASH 0x0420
#define DMC_MSG_MOTOR_GET_BACKLASH 0x0421
```

### Frame Rate Constants

```cpp
#define TIMECODE_FPS_24    0
#define TIMECODE_FPS_25    1
#define TIMECODE_FPS_30    2
#define TIMECODE_FPS_29_97 3
```

### Sync Mode Constants

```cpp
#define TIMECODE_SYNC_OFF   0
#define TIMECODE_SYNC_READ  1
#define TIMECODE_SYNC_CHASE 2
#define TIMECODE_SYNC_JAM   3
```

---

## Future Enhancements (Roadmap)

Planned for future versions:

- **v1.8.0**: Advanced trigger sequences, position presets
- **v1.9.0**: S-curve acceleration profiles, homing sequences
- **v2.0.0**: DMX lighting control (hardware dependent)

---

## Credits

Professional features implemented by dmc-lite development team.

SMPTE timecode specification: Society of Motion Picture and Television Engineers
Quadrature encoding: Standard incremental encoder protocol

For support and contributions: https://github.com/labodezao/DMCDragonFrame
