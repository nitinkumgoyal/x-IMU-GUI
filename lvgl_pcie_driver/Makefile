obj-m += lvgl_pcie_driver.o

# Object files that make up the driver
lvgl_pcie_driver-objs := src/lvgl_pcie_main.o \
                        src/lvgl_pcie_dma.o \
                        src/lvgl_pcie_dma_chunked.o \
                        src/lvgl_pcie_buffer.o \
                        src/lvgl_pcie_lvgl.o \
                        src/lvgl_pcie_interrupt.o

# Kernel build directory
KDIR := /lib/modules/$(shell uname -r)/build

# Current directory
PWD := $(shell pwd)

# Compiler flags for 4K video optimization
ccflags-y += -O2 -DCONFIG_LVGL_PCIE_4K_SUPPORT=1

# Include paths
ccflags-y += -I$(PWD)/include

# Default target
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean target
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f Module.symvers modules.order

# Install target
install: all
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

# Uninstall target
uninstall:
	rm -f /lib/modules/$(shell uname -r)/extra/lvgl_pcie_driver.ko
	depmod -a

# Load module
load: install
	modprobe lvgl_pcie_driver

# Unload module
unload:
	modprobe -r lvgl_pcie_driver

# Development targets
debug: ccflags-y += -DDEBUG -g
debug: all

# Check syntax without building
check:
	$(MAKE) -C $(KDIR) M=$(PWD) C=2 modules

# Show module information
info:
	@echo "LVGL PCIe Driver Build Information"
	@echo "=================================="
	@echo "Kernel version: $(shell uname -r)"
	@echo "Build directory: $(KDIR)"
	@echo "Source directory: $(PWD)"
	@echo "Object files: $(lvgl_pcie_driver-objs)"
	@echo ""
	@echo "Usage:"
	@echo "  make          - Build the driver"
	@echo "  make clean    - Clean build files"
	@echo "  make install  - Install the driver"
	@echo "  make load     - Load the driver module"
	@echo "  make unload   - Unload the driver module"
	@echo "  make debug    - Build with debug symbols"
	@echo "  make check    - Check syntax with sparse"

.PHONY: all clean install uninstall load unload debug check info