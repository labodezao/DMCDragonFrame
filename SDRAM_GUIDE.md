# SDRAM Configuration Guide

## Overview

Version 1.6.0 of dmc-lite adds optional support for the 8 MB external SDRAM available on the Arduino Giga R1. This enables expanded capacity for commercial-grade motion control applications.

## Features Enabled by SDRAM

### Without SDRAM (Default Configuration)
- **Motors**: 16 motors
- **Frame capacity**: 10,000 frames per motor
- **RAM usage**: ~664 KB internal SRAM
- **Compatibility**: Works on all supported boards

### With SDRAM (Enhanced Configuration)
- **Motors**: 32 motors (matching DMC-32 commercial version)
- **Frame capacity**: 20,000 frames per motor
- **RAM usage**: ~2.6 MB in external SDRAM + ~200 KB internal SRAM
- **Performance**: Same real-time performance (SDRAM is hardware-cached)
- **Compatibility**: Arduino Giga R1 only

## Enabling SDRAM Support

### Step 1: Edit Configuration File

Open `dmc_m7/dfx.h` and locate this section near the top:

```cpp
// Configuration: Enable SDRAM for expanded capacity
// Uncomment the line below to use external SDRAM (8 MB)
// This allows for 32 motors and 20,000 frames
//#define USE_SDRAM
```

Uncomment the `#define USE_SDRAM` line:

```cpp
// Configuration: Enable SDRAM for expanded capacity
// Uncomment the line below to use external SDRAM (8 MB)
// This allows for 32 motors and 20,000 frames
#define USE_SDRAM
```

### Step 2: Recompile and Upload

1. Open `dmc_m7/dmc_m7.ino` in Arduino IDE
2. Select **Tools > Board > Arduino Mbed OS Giga Boards > Arduino Giga R1**
3. Select **Tools > Target core > Main Core**
4. Select **Tools > Flash split > 1.5MB M7 + 0.5MB M4**
5. Click **Verify** to compile
6. Click **Upload** to flash the M7 core
7. Upload `dmc_m4/dmc_m4.ino` to the M4 core (no changes needed)

### Step 3: Verify Operation

After uploading:
- The blue LED should blink normally (USB communication)
- If the red LED blinks rapidly, SDRAM initialization failed
- Send a version query command to verify 32 motor support

## Memory Architecture

### Internal SRAM (864 KB)
Used for real-time critical data:
- Motor state structures (~2 KB)
- Shared memory with M4 core (200 bytes)
- Message buffers (~3.5 KB)
- GoMotion calculations (~3 KB)
- Stack and heap (~200 KB remaining)

### External SDRAM (8 MB)
Used for large data buffers:
- AxisMoveData arrays: 32 motors × 20K frames × 4 bytes = 2.56 MB
- Trigger data: 20,000 bytes = 20 KB
- **Available for future use**: ~5.4 MB (DMX buffers, etc.)

### Performance Considerations

The STM32H747XI microcontroller features:
- **Hardware FMC (Flexible Memory Controller)**: Direct SDRAM interface
- **L1 cache**: 16 KB data + 16 KB instruction cache on M7 core
- **SDRAM clock**: 166 MHz (same as CPU clock)
- **Access time**: ~6 ns (cached), ~30 ns (uncached)

**Result**: No performance penalty for buffer access. Real-time motion control maintains 50 Hz update rate.

## Troubleshooting

### Red LED Blinking Rapidly

This indicates SDRAM allocation failed. Possible causes:

1. **SDRAM library not available**: Ensure you're using Arduino Mbed OS core for Giga R1
   - Update to latest core version via Board Manager

2. **Insufficient SDRAM**: Unlikely with 8 MB available
   - Check if other libraries are using SDRAM

3. **Hardware issue**: SDRAM chip not responding
   - Try power cycling the board
   - Check for loose connections

### Compilation Errors

**Error**: `SDRAM.h: No such file or directory`
- **Solution**: Update Arduino Mbed OS core to version 4.0.0 or later

**Error**: `USE_SDRAM is only supported on Arduino Giga R1`
- **Solution**: Don't enable USE_SDRAM on Portenta H7 or other boards

**Error**: `'SDRAM' was not declared in this scope`
- **Solution**: Ensure you're compiling for Arduino Giga R1, not Portenta H7

### Runtime Issues

**Motors not responding correctly**:
- Verify both M7 and M4 cores are programmed correctly
- Check that flash split is set to 1.5MB M7 + 0.5MB M4
- Try disabling SDRAM to isolate the issue

**Reduced performance**:
- SDRAM should not affect performance
- If you notice slowdowns, disable SDRAM and report the issue

## RAM Usage Calculations

### Without SDRAM (16 motors, 10K frames)
```
AxisMoveData: 16 × 10,000 × 4 bytes = 640 KB
Trigger data:  10,000 bytes = 10 KB
Other data:    ~14 KB
Total:         ~664 KB (77% of 864 KB internal SRAM)
```

### With SDRAM (32 motors, 20K frames)
```
Internal SRAM:
  Motor structures: ~2 KB
  Message buffers:  ~3.5 KB
  Other data:       ~14 KB
  Available:        ~844 KB (97% free)

External SDRAM:
  AxisMoveData: 32 × 20,000 × 4 bytes = 2.56 MB
  Trigger data: 20,000 bytes = 20 KB
  Used:         ~2.58 MB (32% of 8 MB)
  Available:    ~5.42 MB for future features
```

## Future Enhancements

The remaining 5.4 MB of SDRAM can be used for:

### DMX Buffer (Planned)
- **Capacity**: 512 channels × 20K frames = 10 MB (compressed storage)
- **Implementation**: Sparse array for efficiency (only store non-zero values)
- **Benefit**: Synchronized lighting control with motion

### Extended Frame Capacity
- **Theoretical maximum**: 50,000 frames with 32 motors = 6.4 MB
- **Trade-off**: Less space for DMX buffer
- **Use case**: Very long animation sequences

### Data Logging
- **Motion recording**: Store actual motor positions during playback
- **Sensor history**: Record analog inputs over time
- **Performance metrics**: Track timing and errors

## Best Practices

1. **Start without SDRAM**: Test your setup with default 16 motors first
2. **Gradual expansion**: Only enable SDRAM when you need >16 motors
3. **Monitor performance**: Watch for any timing issues (should be none)
4. **Backup configuration**: Keep a working firmware copy before enabling SDRAM
5. **Document your setup**: Note which motors are connected when using 32-motor mode

## Comparison with Commercial DMC-32

| Feature | dmc-lite v1.6 (no SDRAM) | dmc-lite v1.6 (with SDRAM) | Commercial DMC-32 |
|---------|-------------------------|---------------------------|-------------------|
| Motors | 16 | 32 | 32 |
| Frame capacity | 10,000 | 20,000 | Unknown (likely 20K+) |
| Limit switches | 16 (8×2) | 16 (8×2) | 16 (8×2) |
| Analog inputs | 12 | 12 | Unknown |
| DMX support | Protocol only | Protocol only | Full hardware |
| Cost | $60-80 (DIY) | $60-80 (DIY) | $695 (professional) |

## Support and Feedback

If you encounter issues with SDRAM support:

1. Check this guide's troubleshooting section
2. Verify your Arduino Mbed OS core version (need 4.0.0+)
3. Try the default configuration (without SDRAM) to isolate the problem
4. Report issues with detailed error messages and board information

## Technical References

- [Arduino Giga R1 Documentation](https://docs.arduino.cc/hardware/giga-r1-wifi)
- [STM32H747XI Datasheet](https://www.st.com/resource/en/datasheet/stm32h747xi.pdf)
- [SDRAM Configuration Guide](https://github.com/arduino/ArduinoCore-mbed/issues/38)
- [FMC Interface Documentation](https://www.st.com/resource/en/reference_manual/dm00176879.pdf)

---

**Note**: SDRAM support is optional and experimental. The default configuration (16 motors, 10K frames) is proven and reliable for most applications. Only enable SDRAM if you specifically need more capacity.
