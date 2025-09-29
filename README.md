# Liquid Lens Camera - Out of the Box Usage

This repository provides a complete implementation for out-of-the-box usage of liquid lens technology in camera applications. Liquid lenses offer ultra-fast focusing, versatile focal lengths, and enhanced durability compared to traditional mechanical focusing systems.

## Features

- **Ultra-fast focusing**: Sub-millisecond focus adjustment
- **Versatile operation**: Single lens functions as both macro and telephoto
- **Hardware abstraction**: Works with various liquid lens modules
- **Real-time control**: Live preview with interactive focus control
- **Focus stacking**: Automated capture sequences for enhanced depth of field
- **Auto-focus**: Contrast-based auto-focusing algorithm
- **Cross-platform**: Works on Linux, Windows, and Raspberry Pi

## Quick Start

### Installation

```bash
# Install dependencies
pip install -r requirements.txt

# For hardware I2C support (Linux/Raspberry Pi)
sudo apt-get install i2c-tools python3-smbus

# Enable I2C interface (Raspberry Pi)
sudo raspi-config  # Interface Options > I2C > Enable
```

### Basic Usage

```python
from liquid_lens_camera import LiquidLensCamera

# Create and initialize camera
camera = LiquidLensCamera()
if camera.initialize():
    # Quick focus modes
    camera.macro_mode()      # Close-up photography
    camera.telephoto_mode()  # Distance photography
    
    # Manual focus control
    camera.set_focus_position(2048)  # Mid-range focus
    
    # Start live preview with keyboard controls
    camera.start_preview()
```

### Run Demo

```bash
# Run all demonstrations
python demo_liquid_lens.py

# Run specific demo
python demo_liquid_lens.py --mode interactive

# Available modes: basic, stacking, hardware, interactive, all
```

## Hardware Integration

### Supported Hardware

- **I2C-based liquid lens modules** (most common)
- **Standard USB/CSI cameras** with liquid lens attachment
- **Raspberry Pi Camera Module** with liquid lens
- **Custom liquid lens drivers**

### Wiring (Raspberry Pi Example)

```
Liquid Lens Module    Raspberry Pi
==================    ============
VCC                   3.3V (Pin 1)
GND                   Ground (Pin 6)
SDA                   GPIO 2 (Pin 3)
SCL                   GPIO 3 (Pin 5)
```

### I2C Configuration

```python
from liquid_lens_hardware import LiquidLensDriver, LiquidLensController

# Initialize hardware driver
driver = LiquidLensDriver(i2c_bus=1, i2c_address=0x48)
controller = LiquidLensController(driver)

# Connect and calibrate
controller.connect()
controller.calibrate()

# Focus control
controller.set_focus_distance(100)  # 100mm focus distance
controller.set_focus_percentage(75)  # 75% focus range
```

## API Reference

### LiquidLensCamera Class

Main camera interface with liquid lens control:

```python
class LiquidLensCamera:
    def __init__(self, camera_id=0, config=None)
    def initialize() -> bool
    def set_focus_position(position: int) -> bool  # 0-4095 range
    def macro_mode() -> bool                       # Closest focus
    def telephoto_mode() -> bool                   # Infinity focus
    def auto_focus(frame) -> bool                  # Contrast-based AF
    def capture_frame() -> np.ndarray              # Single frame capture
    def start_preview()                            # Live preview with controls
    def focus_stack_sequence(num_steps=10) -> list # Focus stacking
```

### Hardware Control

Low-level hardware interface:

```python
class LiquidLensController:
    def set_focus_distance(distance_mm: float) -> bool
    def set_focus_percentage(percentage: float) -> bool
    def macro_mode() -> bool
    def infinity_mode() -> bool
    def focus_sweep(start_pct, end_pct, steps) -> list
    def calibrate() -> bool
```

## Applications

### 1. Macro Photography
```python
camera.macro_mode()
frame = camera.capture_frame()
cv2.imwrite("macro_shot.jpg", frame)
```

### 2. Focus Stacking
```python
# Capture sequence at different focus distances
photos = camera.focus_stack_sequence(num_steps=20)
# Use external software to combine into focus-stacked image
```

### 3. Real-time Focus Control
```python
camera.start_preview()
# Interactive controls:
# 'm' - macro mode
# 't' - telephoto mode  
# 'a' - auto focus
# '+'/'-' - manual adjustment
```

### 4. Industrial Inspection
```python
# Set specific focus distance for consistent imaging
controller.set_focus_distance(150)  # 150mm working distance
# Capture inspection images at fixed focus
```

## Advanced Features

### Auto-Focus Algorithm

The implementation includes a contrast-based auto-focus system:

```python
def auto_focus(self, frame):
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    focus_measure = cv2.Laplacian(gray, cv2.CV_64F).var()
    # Search for optimal focus position
    # Returns True when optimal focus found
```

### Focus Stacking

Automated capture of multiple images at different focus distances:

```python
# Capture 15 images from macro to infinity
photos = camera.focus_stack_sequence(num_steps=15)

# Custom focus positions
positions = [0, 1000, 2000, 3000, 4095]
photos = camera.capture_photo_sequence(positions)
```

### Live Preview Controls

Interactive preview window with keyboard shortcuts:

- **'m'**: Switch to macro mode (close focus)
- **'t'**: Switch to telephoto mode (far focus)
- **'a'**: Trigger auto-focus
- **'+'/'='**: Increase focus distance
- **'-'**: Decrease focus distance
- **'r'**: Start/stop video recording
- **'q'**: Quit preview

## Troubleshooting

### Common Issues

1. **Camera not detected**
   ```bash
   # List available cameras
   v4l2-ctl --list-devices
   
   # Try different camera ID
   camera = LiquidLensCamera(camera_id=1)
   ```

2. **I2C communication errors**
   ```bash
   # Check I2C devices
   sudo i2cdetect -y 1
   
   # Verify wiring and power supply
   # Try different I2C address
   driver = LiquidLensDriver(i2c_address=0x49)
   ```

3. **Focus not responding**
   ```python
   # Calibrate the lens
   controller.calibrate()
   
   # Check focus range
   controller.set_focus_percentage(0)    # Minimum
   controller.set_focus_percentage(100)  # Maximum
   ```

### Performance Optimization

- **Reduce focus settling time** for faster operation
- **Use threaded focus control** for real-time applications
- **Implement focus position caching** to avoid redundant commands
- **Optimize auto-focus search range** for specific applications

## Hardware Specifications

### Typical Liquid Lens Parameters

- **Focus range**: 0-4095 (12-bit resolution)
- **Settling time**: <1ms to <10ms depending on module
- **Operating voltage**: 3.3V or 5V
- **Interface**: I2C (100kHz to 400kHz)
- **Focus distance**: 10mm to infinity
- **Temperature range**: -20°C to +70°C

### Compatibility

- **Operating Systems**: Linux, Windows, macOS
- **Python versions**: 3.7+
- **Hardware platforms**: x86, ARM, Raspberry Pi
- **Camera interfaces**: USB, CSI, MIPI

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Submit a pull request

## License

MIT License - see LICENSE file for details.

## References

- [Liquid Lens Technology Overview](https://www.pocket-lint.com/cameras/news/xiaomi/156400-what-is-a-liquid-lens-camera-the-technology-explained/)
- [Focus Stacking Techniques](https://en.wikipedia.org/wiki/Focus_stacking)
- [Camera Calibration Methods](https://docs.opencv.org/4.x/dc/dbb/tutorial_py_calibration.html)