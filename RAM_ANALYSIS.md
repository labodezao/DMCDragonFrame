# Arduino Giga R1 RAM Analysis for dmc-lite

## Arduino Giga R1 RAM Specifications

- **Internal SRAM**: ~864 KB usable (1 MB total, some reserved for system)
- **External SDRAM**: 8 MB (AS4C4M16SA chip, 166 MHz)
- **Architecture**: STM32H747XI dual-core (M7 + M4)

## Current RAM Usage Analysis

### Major Memory Allocations

#### 1. AxisMoveData Structure (Biggest consumer)
```cpp
struct AxisMoveData {
  int32_t frameCount;           // 4 bytes
  int32_t position[FRAME_COUNT]; // 4 × 10,000 = 40,000 bytes
};
```
**Per motor**: 40,004 bytes
**For 16 motors**: 640,064 bytes ≈ **625 KB**

#### 2. Trigger Data Array
```cpp
static uint8_t triggerData[FRAME_COUNT];  // 10,000 bytes ≈ 10 KB
```

#### 3. Motor Structures
```cpp
struct Motor {
  // ~200 bytes per motor including moves array
};
static Motor motors[MOTOR_CAM_COUNT];  // 9 motors ≈ 1.8 KB
```

#### 4. GoMotion Structures
```cpp
struct GoMotionMove {
  float arrays[MOTOR_COUNT];  // Multiple float arrays
  // ~2.5 KB total
};
static GoMotionOverride goMotionOverride[MOTOR_COUNT];  // ~200 bytes
```

#### 5. Shared Memory (Between M7 and M4)
```cpp
struct DmcSharedData {
  volatile int64_t nextSpeed[MOTOR_CAM_COUNT];    // 9 × 8 = 72 bytes
  volatile int64_t accum[MOTOR_CAM_COUNT];        // 9 × 8 = 72 bytes
  // ~200 bytes total
};
```

#### 6. Message Buffers
```cpp
dmc_user_msg = malloc(2048);   // 2 KB
dmc_out_msg = malloc(256);     // 256 bytes
RingBufferN<1024> messageBuffer; // 1 KB
```

### Total Current RAM Usage (Estimated)

| Component | Size |
|-----------|------|
| AxisMoveData (16 motors) | 625 KB |
| Trigger data | 10 KB |
| Motor structures | 2 KB |
| GoMotion structures | 3 KB |
| Message buffers | 3.5 KB |
| Stack + heap overhead | ~20 KB |
| **TOTAL** | **~664 KB** |

**Remaining Internal SRAM**: 864 KB - 664 KB = **~200 KB available**

## Maximum Inputs Analysis

### Option 1: Stay Within Internal SRAM (Current Approach)

**Current Limits**:
- **Motors**: 16 (limited by MOTOR_COUNT)
- **Frames**: 10,000 (limited by FRAME_COUNT)
- **Logic Inputs**: Currently 1, could easily add 8-16 more (minimal RAM)
- **DMX Channels**: 0 (protocol ready, but no buffer allocated)

**Possible Improvements Without External RAM**:
1. **Add more logic inputs**: Up to 32+ digital inputs (uses only a few bytes each)
2. **Add analog inputs**: 12 analog inputs available (ADC channels)
3. **Reduce FRAME_COUNT** to 5,000: Saves 312 KB → **512 KB available**
4. **Add DMX with reduced frames**: 512 channels × 5,000 frames = 2.5 MB (won't fit in internal RAM)

### Option 2: Use External SDRAM (8 MB Available)

**Potential Maximum Capacity**:

#### Scenario A: Maximum Motors with 10K Frames
- **Motors**: 32 motors (need 1.25 MB for AxisMoveData)
- **Frames**: 10,000
- **DMX Channels**: 512 (5 MB for full DMX buffer)
- **Total**: ~6.5 MB (fits in 8 MB SDRAM)

#### Scenario B: Maximum Frames with 16 Motors
- **Motors**: 16 motors
- **Frames**: 50,000 (3.2 MB for AxisMoveData)
- **DMX Channels**: 512 with 50K frames (25 MB - won't fit)
- **DMX Channels**: 512 with 10K frames (5 MB - fits)
- **Total**: ~8.2 MB (tight fit)

#### Scenario C: Balanced Approach
- **Motors**: 24 motors (1.9 MB for AxisMoveData)
- **Frames**: 15,000 (2.8 MB for AxisMoveData with 24 motors)
- **DMX Channels**: 512 with 15K frames (7.5 MB - tight)
- **Logic Inputs**: 32+
- **Analog Inputs**: 12

### Option 3: Commercial Version Recommendations

Based on https://www.dmclite.com/ and professional DMC-32 standards:

**Recommended Configuration**:
```cpp
#define MOTOR_COUNT 32          // Match DMC-32 capability
#define FRAME_COUNT 20000       // Double current capacity
#define DMX_CHANNELS 512        // Full DMX universe
#define LOGIC_INPUTS 16         // 16 limit switch inputs (8 motors × 2)
#define LOGIC_OUTPUTS 8         // Enhanced trigger outputs
#define ANALOG_INPUTS 8         // Analog sensor inputs
```

**RAM Requirements**:
- AxisMoveData: 32 motors × 20K frames × 4 bytes = 2.56 MB
- DMX buffer: 512 channels × 20K frames = 10 MB (need compression or limit)
- Other structures: ~50 KB
- **Total**: 2.6 MB + DMX

**Solution**: Use SDRAM for large buffers, keep real-time data in internal RAM

## Implementation Strategy

### Phase 1: Optimize Current Code (No SDRAM needed)
✅ Already done in v1.3.0

### Phase 2: Add More Digital/Analog Inputs (Within Internal RAM)
**Easy additions** (uses < 10 KB total):
- ✅ Logic input query (already added)
- ⬜ 16 limit switch inputs (8 motors × 2 limits)
- ⬜ 8 additional logic outputs
- ⬜ 8 analog inputs (sensors, potentiometers)
- ⬜ Emergency stop chain monitoring

### Phase 3: Enable External SDRAM (Unlock 8 MB)
**Requires code changes**:
1. Initialize SDRAM controller
2. Move AxisMoveData arrays to SDRAM
3. Move DMX buffer to SDRAM (if implemented)
4. Keep real-time structures in fast internal RAM

**Example code**:
```cpp
// Use SDRAM for large buffers
#define SDRAM_BASE 0xC0000000
AxisMoveData *move = (AxisMoveData*)SDRAM_BASE;
uint8_t *dmxBuffer = (uint8_t*)(SDRAM_BASE + 0x200000);
```

### Phase 4: Expand Capacity (With SDRAM)
**New limits**:
- Motors: 32 (matching DMC-32)
- Frames: 20,000+ (limited only by SDRAM)
- DMX: 512 channels with smart buffering
- Inputs: 32+ digital, 12 analog

## Comparison: dmclite.com vs Current Implementation

### Current dmc-lite v1.3.0
- Motors: 16
- Frames: 10,000
- RAM: 664 KB (internal SRAM only)
- DMX: Protocol support only
- Cost: DIY ($60-80 for Arduino Giga R1)

### dmclite.com Commercial Version
- Motors: 32 (estimated)
- Frames: Unknown (likely 20K+)
- RAM: Uses full hardware capabilities
- DMX: Full hardware implementation
- Limit switches: 16 inputs (8 motors)
- Additional I/O: Multiple analog/digital
- Cost: $695

### Improvements Needed to Match Commercial Version
1. ✅ Enhanced protocol support (done in v1.3.0)
2. ⬜ SDRAM utilization for 32 motors
3. ⬜ Hardware DMX512 transceiver
4. ⬜ 16 limit switch inputs
5. ⬜ Analog input reading (sensors)
6. ⬜ Enhanced error detection and recovery
7. ⬜ Better UI feedback (LEDs, display)
8. ⬜ Professional enclosure and connectors

## Recommended Next Steps

### Immediate Improvements (No Hardware Changes)
1. **Add limit switch input reading** (16 pins)
   - RAM impact: < 1 KB
   - Uses existing pull-up configuration

2. **Add analog input reading** (8 channels)
   - RAM impact: < 1 KB
   - Useful for sensors, feedback, calibration

3. **Add more logic outputs** (expand from 2 to 8)
   - RAM impact: negligible
   - Better trigger control

4. **Optimize frame storage** (compression)
   - Could reduce RAM by 30-50%
   - Delta encoding for position arrays

### Medium-Term Improvements (Requires SDRAM)
1. **Enable external SDRAM**
   - Unlock 8 MB for buffers
   - Keep real-time data in internal RAM

2. **Increase motor count to 32**
   - RAM: 2.56 MB with 20K frames
   - Matches professional DMC-32

3. **Implement DMX buffer**
   - 512 channels with smart buffering
   - Sparse storage for efficiency

### Long-Term Improvements (Hardware + SDRAM)
1. **Hardware DMX512 transceiver**
   - MAX485 + XLR connector
   - Full 512 channel support

2. **Hardware limit switch conditioning**
   - Debouncing circuits
   - Opto-isolation for safety

3. **Professional PCB design**
   - Custom board with all features
   - Industrial-grade connectors

## Code Examples

### Example 1: Add Limit Switch Reading
```cpp
// In config.h - already prepared in v1.3.0
#define LIMIT_SWITCH_LOW_1  D51
#define LIMIT_SWITCH_HIGH_1 D52
// ... define all 16 limit switches

// In setup()
#ifdef LIMIT_SWITCH_LOW_1
  pinMode(LIMIT_SWITCH_LOW_1, INPUT_PULLUP);
#endif

// In loop() - monitor switches
uint16_t readLimitSwitches() {
  uint16_t switches = 0;
  #ifdef LIMIT_SWITCH_LOW_1
    if (!digitalRead(LIMIT_SWITCH_LOW_1)) switches |= (1 << 0);
  #endif
  // ... read all switches
  return switches;
}
```

### Example 2: Enable SDRAM (Requires Arduino SDRAM library)
```cpp
#include <SDRAM.h>

void setup() {
  // Initialize SDRAM
  SDRAM.begin();

  // Allocate large buffers in SDRAM
  AxisMoveData *moveSDRAM = (AxisMoveData*)SDRAM.malloc(
    sizeof(AxisMoveData) * MOTOR_COUNT
  );

  // Use SDRAM buffers for non-time-critical data
  // Keep real-time structures in internal RAM
}
```

### Example 3: Analog Input Reading
```cpp
// Read analog sensors
uint16_t readAnalogInputs(uint8_t channel) {
  if (channel < 12) {  // Giga R1 has 12 ADC channels
    return analogRead(A0 + channel);
  }
  return 0;
}

// Add command to query analog inputs
case DMC_MSG_ANALOG_IN:
  uint8_t channel = dmc_msg_read_byte();
  dmc_msg_prepare(cmd | DMC_MSG_FLAG_ACK, msgId);
  dmc_msg_out_dword(DMC_ACK_OK);
  dmc_msg_out_word(readAnalogInputs(channel));
  writeOutputMessage();
  break;
```

## Conclusion

**Current Status** (v1.3.0):
- RAM usage: ~664 KB / 864 KB internal SRAM (77% utilized)
- **Available RAM**: ~200 KB for new features
- External SDRAM: 8 MB unused

**Maximum Inputs Without SDRAM**:
- Digital inputs: 32+ (limited only by available pins)
- Analog inputs: 12 (ADC channels on Giga R1)
- Motors: 16 (could go to ~20 with current RAM)
- Frames: 10,000 (could go to ~15,000 with current RAM)

**Maximum Inputs With SDRAM**:
- Digital inputs: 64+ (limited by pin count)
- Analog inputs: 12
- Motors: 32 (matching DMC-32)
- Frames: 50,000+ (limited by SDRAM size)
- DMX channels: 512 (with smart buffering)

**Recommendation**:
1. First, add more digital/analog inputs (easy, no SDRAM needed)
2. Then, enable SDRAM to expand motor count and frames
3. Finally, add hardware for DMX and professional features

This approach allows gradual improvement while maintaining compatibility with the commercial dmclite.com version.
