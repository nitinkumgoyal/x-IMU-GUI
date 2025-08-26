# LVGL PCIe Driver with 4K Video Support

A high-performance Linux kernel driver that provides LVGL display support through PCIe interface with DMA transfers to FPGA, specifically designed for 4K video frame processing with double buffering.

## Features

- **4K Video Support**: Optimized for 3840x2160 @ 32bpp video frames (~31.2MB per frame)
- **Double Buffering**: Seamless frame transitions without tearing or flickering
- **Chunked DMA Transfers**: Efficient transfer of large frames using 4MB chunks
- **PCIe Interface**: High-bandwidth communication with FPGA hardware
- **LVGL Integration**: Full LVGL display driver with hardware acceleration
- **MSI Interrupt Support**: Low-latency interrupt handling for better performance
- **Error Recovery**: Robust error handling and automatic recovery mechanisms

## Architecture

### Buffer Management
- **2 DMA Buffers**: Each buffer holds one complete 4K frame (31.2MB)
- **Total Memory**: ~62.4MB of coherent DMA memory
- **Double Buffering**: One buffer for display, one for rendering

### DMA Transfer System
- **Chunked Transfers**: 4MB chunks for optimal PCIe performance
- **~8 Chunks per Frame**: Each 4K frame divided into manageable chunks
- **Interrupt-Driven**: Asynchronous completion notifications
- **Error Handling**: Automatic retry and recovery mechanisms

### LVGL Integration
- **Direct Mode**: Full-frame rendering for maximum performance
- **Hardware Buffers**: LVGL draws directly to DMA buffers
- **Flush Callback**: Automatic buffer swapping and DMA initiation
- **32-Pixel Alignment**: Optimized for DMA performance

## File Structure

```
lvgl_pcie_driver/
├── include/
│   └── lvgl_pcie_driver.h          # Main header file
├── src/
│   ├── lvgl_pcie_main.c            # Driver initialization and PCIe setup
│   ├── lvgl_pcie_dma.c             # DMA buffer allocation and management
│   ├── lvgl_pcie_dma_chunked.c     # Chunked DMA transfer for 4K frames
│   ├── lvgl_pcie_buffer.c          # Double buffer management
│   ├── lvgl_pcie_lvgl.c            # LVGL display driver interface
│   └── lvgl_pcie_interrupt.c       # Interrupt handling and synchronization
├── examples/
├── docs/
└── README.md
```

## Hardware Requirements

### PCIe Device
- **Vendor ID**: 0x1234 (configurable in header)
- **Device ID**: 0x5678 (configurable in header)
- **BAR0**: Memory-mapped I/O region for registers
- **DMA Engine**: Capable of 64-bit addressing
- **Interrupts**: MSI or legacy interrupt support

### FPGA Register Map
```
0x00: CONTROL     - Control register (enable, reset, DMA start, VSYNC enable)
0x04: STATUS      - Status register (ready, DMA done, error, VSYNC)
0x08: DMA_SRC_ADDR - DMA source address (64-bit)
0x10: DMA_SIZE    - DMA transfer size
0x14: DMA_CTRL    - DMA control (chunk info)
0x18: INTERRUPT   - Interrupt status/mask
0x1C: VERSION     - Hardware version
```

### Memory Requirements
- **Minimum RAM**: 128MB available for DMA buffers
- **DMA Coherent**: Preferably coherent DMA for best performance
- **Address Space**: 64-bit DMA addressing recommended

## Configuration

### Buffer Size Configuration
The driver is configured for 4K video frames:
```c
#define DEFAULT_SCREEN_WIDTH 3840   /* 4K width */
#define DEFAULT_SCREEN_HEIGHT 2160  /* 4K height */
#define DEFAULT_COLOR_DEPTH 32      /* 32-bit RGBA */
#define FRAME_SIZE_4K (~31.2MB)     /* Per frame */
```

### DMA Configuration
```c
#define DMA_BUFFER_COUNT 2          /* Double buffering */
#define DMA_CHUNK_SIZE (4MB)        /* Chunk size */
#define DMA_TIMEOUT_MS 5000         /* Extended timeout for large transfers */
#define MAX_DMA_SIZE (64MB)         /* Maximum transfer size */
```

## Usage

### Module Loading
```bash
# Load the driver
sudo modprobe lvgl_pcie_driver

# Check driver status
dmesg | grep lvgl_pcie

# Verify device creation
ls -la /dev/lvgl_pcie*
```

### LVGL Integration Example
```c
#include "lvgl/lvgl.h"

/* LVGL is automatically initialized by the driver */
/* Create your LVGL objects normally */

lv_obj_t *label = lv_label_create(lv_scr_act());
lv_label_set_text(label, "4K Video Display");
lv_obj_center(label);

/* The driver handles all DMA transfers automatically */
```

### Character Device Interface
```c
#include <sys/ioctl.h>

int fd = open("/dev/lvgl_pcie0", O_RDWR);

/* Get device information */
struct lvgl_pcie_info info;
ioctl(fd, LVGL_PCIE_IOC_GET_INFO, &info);
printf("4K Display: %dx%d, Frame size: %.1f MB\n", 
       info.screen_width, info.screen_height,
       info.buffer_size / (1024.0 * 1024.0));

/* Get statistics */
struct lvgl_pcie_stats stats;
ioctl(fd, LVGL_PCIE_IOC_GET_STATS, &stats);
printf("Frames sent: %llu, Errors: %llu, FPS: %u\n",
       stats.frames_sent, stats.dma_errors, stats.current_fps);
```

## Performance Characteristics

### Transfer Performance
- **4K Frame Size**: 31.2MB (3840×2160×4 bytes)
- **Chunk Size**: 4MB (8 chunks per frame)
- **Theoretical PCIe Gen3 x8**: ~6.4 GB/s
- **Expected Frame Rate**: Up to 60 FPS with proper FPGA implementation

### Memory Bandwidth
- **Per Frame**: 31.2MB
- **60 FPS**: ~1.87 GB/s sustained throughput
- **Double Buffering**: Additional memory for seamless transitions

### Latency Considerations
- **Chunked Transfer**: Reduces maximum latency per operation
- **Interrupt-Driven**: Minimal CPU overhead for completion detection
- **VSYNC Synchronization**: Frame-accurate timing

## Debugging

### Enable Debug Logging
```bash
# Enable debug messages
echo 8 > /proc/sys/kernel/printk

# View driver messages
dmesg -w | grep lvgl_pcie
```

### Performance Monitoring
```bash
# Monitor transfer statistics
watch -n 1 'cat /proc/interrupts | grep lvgl_pcie'

# Check memory usage
cat /proc/meminfo | grep -i dma
```

### Common Issues

1. **DMA Allocation Failure**
   - Insufficient contiguous memory
   - Solution: Boot with `cma=128M` kernel parameter

2. **High Error Rate**
   - PCIe link issues or FPGA problems
   - Check PCIe link status and FPGA firmware

3. **Performance Issues**
   - Insufficient PCIe bandwidth
   - Check PCIe generation and lane count

## Building

### Prerequisites
```bash
# Install kernel headers
sudo apt-get install linux-headers-$(uname -r)

# Install LVGL development files
sudo apt-get install liblvgl-dev
```

### Compilation
```bash
cd lvgl_pcie_driver/src
make

# Install module
sudo make install
sudo depmod -a
```

### Makefile Example
```makefile
obj-m += lvgl_pcie_driver.o
lvgl_pcie_driver-objs := lvgl_pcie_main.o lvgl_pcie_dma.o \
                        lvgl_pcie_dma_chunked.o lvgl_pcie_buffer.o \
                        lvgl_pcie_lvgl.o lvgl_pcie_interrupt.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

install:
	make -C $(KDIR) M=$(PWD) modules_install
```

## License

GPL v2 - See LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Create a pull request

## Support

For issues and questions:
- Check the kernel log: `dmesg | grep lvgl_pcie`
- Verify hardware connectivity
- Ensure FPGA firmware compatibility
- Review PCIe configuration

## Version History

- **v1.0.0**: Initial release with 4K video support
  - Double buffering implementation
  - Chunked DMA transfers
  - LVGL integration
  - MSI interrupt support

## Technical Notes

### Buffer Alignment
- DMA buffers are allocated with proper alignment for PCIe transfers
- 32-pixel alignment for optimal performance
- Cache-coherent memory when available

### Error Recovery
- Automatic retry on transient errors
- Hardware reset on critical failures
- Statistics tracking for performance monitoring

### Future Enhancements
- Variable chunk size optimization
- Multi-display support
- Hardware compression support
- Power management improvements