# Makefile for LVGL Custom Widget
# This makefile helps compile the custom widget with LVGL

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
INCLUDES = -I. -I./lvgl -I./lvgl/src

# LVGL library path (adjust as needed)
LVGL_DIR = ./lvgl
LVGL_LIB = $(LVGL_DIR)/liblvgl.a

# Source files
WIDGET_SOURCES = lv_custom_widget.c
EXAMPLE_SOURCES = example_usage.c
SOURCES = $(WIDGET_SOURCES) $(EXAMPLE_SOURCES)

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target executable (for testing)
TARGET = custom_widget_test

# Default target
all: $(TARGET)

# Build the test executable
$(TARGET): $(OBJECTS) $(LVGL_LIB)
	$(CC) $(OBJECTS) $(LVGL_LIB) -o $(TARGET) -lm -lpthread

# Compile widget source
lv_custom_widget.o: lv_custom_widget.c lv_custom_widget.h
	$(CC) $(CFLAGS) $(INCLUDES) -c lv_custom_widget.c -o lv_custom_widget.o

# Compile example source
example_usage.o: example_usage.c lv_custom_widget.h
	$(CC) $(CFLAGS) $(INCLUDES) -c example_usage.c -o example_usage.o

# Build LVGL library (if not already built)
$(LVGL_LIB):
	@echo "Building LVGL library..."
	@if [ -d "$(LVGL_DIR)" ]; then \
		$(MAKE) -C $(LVGL_DIR); \
	else \
		echo "LVGL directory not found. Please clone LVGL to $(LVGL_DIR)"; \
		echo "git clone https://github.com/lvgl/lvgl.git $(LVGL_DIR)"; \
		exit 1; \
	fi

# Clean build files
clean:
	rm -f $(OBJECTS) $(TARGET)

# Clean everything including LVGL
clean-all: clean
	@if [ -d "$(LVGL_DIR)" ]; then \
		$(MAKE) -C $(LVGL_DIR) clean; \
	fi

# Install LVGL (clone from GitHub)
install-lvgl:
	@if [ ! -d "$(LVGL_DIR)" ]; then \
		echo "Cloning LVGL..."; \
		git clone https://github.com/lvgl/lvgl.git $(LVGL_DIR); \
		echo "LVGL cloned successfully"; \
	else \
		echo "LVGL already exists"; \
	fi

# Create a library from the widget
library: lv_custom_widget.o
	ar rcs liblv_custom_widget.a lv_custom_widget.o
	@echo "Library created: liblv_custom_widget.a"

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build the test executable"
	@echo "  library      - Create a static library"
	@echo "  clean        - Remove build files"
	@echo "  clean-all    - Remove all build files including LVGL"
	@echo "  install-lvgl - Clone LVGL from GitHub"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  1. First run 'make install-lvgl' to get LVGL"
	@echo "  2. Run 'make' to build the test executable"
	@echo "  3. Or run 'make library' to create a static library"

.PHONY: all clean clean-all install-lvgl library help