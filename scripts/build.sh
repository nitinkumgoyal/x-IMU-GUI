#!/bin/bash

# STM32 Build Script
# This script provides a unified way to build STM32 projects
# Supports both CMake and Make-based builds

set -e  # Exit on any error

# Configuration
BUILD_TYPE="${BUILD_TYPE:-Release}"
PARALLEL_JOBS="${PARALLEL_JOBS:-$(nproc)}"
VERBOSE="${VERBOSE:-0}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if required tools are available
check_tools() {
    log_info "Checking build tools..."
    
    if ! command -v arm-none-eabi-gcc &> /dev/null; then
        log_error "arm-none-eabi-gcc not found. Please install ARM GCC toolchain."
        exit 1
    fi
    
    log_info "ARM GCC Toolchain: $(arm-none-eabi-gcc --version | head -1)"
    
    if command -v cmake &> /dev/null; then
        log_info "CMake: $(cmake --version | head -1)"
    fi
    
    if command -v make &> /dev/null; then
        log_info "Make: $(make --version | head -1)"
    fi
}

# Clean build artifacts
clean_build() {
    log_info "Cleaning build artifacts..."
    
    if [ -d "build" ]; then
        rm -rf build
        log_success "Removed build directory"
    fi
    
    # Clean Make artifacts
    if [ -f "Makefile" ]; then
        make clean 2>/dev/null || true
        log_success "Cleaned Make artifacts"
    fi
    
    # Remove common output files
    rm -f *.elf *.bin *.hex *.map *.lst 2>/dev/null || true
}

# Build using CMake
build_cmake() {
    log_info "Building with CMake..."
    
    if [ ! -f "CMakeLists.txt" ]; then
        log_error "CMakeLists.txt not found"
        return 1
    fi
    
    mkdir -p build
    cd build
    
    # Configure
    local cmake_args="-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    
    if [ -f "../cmake/arm-none-eabi-gcc.cmake" ]; then
        cmake_args="${cmake_args} -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi-gcc.cmake"
    fi
    
    log_info "Configuring CMake project..."
    cmake ${cmake_args} ..
    
    # Build
    local build_args="--parallel ${PARALLEL_JOBS}"
    if [ "${VERBOSE}" = "1" ]; then
        build_args="${build_args} --verbose"
    fi
    
    log_info "Building project..."
    cmake --build . ${build_args}
    
    cd ..
    log_success "CMake build completed"
}

# Build using Make
build_make() {
    log_info "Building with Make..."
    
    if [ ! -f "Makefile" ]; then
        log_error "Makefile not found"
        return 1
    fi
    
    local make_args="-j${PARALLEL_JOBS}"
    if [ "${VERBOSE}" = "1" ]; then
        make_args="${make_args} V=1"
    fi
    
    make ${make_args} all
    log_success "Make build completed"
}

# Analyze build output
analyze_output() {
    log_info "Analyzing build output..."
    
    # Find ELF files
    local elf_files=$(find . -name "*.elf" 2>/dev/null)
    
    if [ -z "$elf_files" ]; then
        log_warning "No ELF files found"
        return
    fi
    
    for elf_file in $elf_files; do
        log_info "Analyzing $elf_file"
        echo "=== Memory Usage ==="
        arm-none-eabi-size "$elf_file"
        echo ""
        
        # Generate additional output formats
        local base_name=$(basename "$elf_file" .elf)
        local dir_name=$(dirname "$elf_file")
        
        log_info "Generating binary files..."
        arm-none-eabi-objcopy -O binary "$elf_file" "${dir_name}/${base_name}.bin"
        arm-none-eabi-objcopy -O ihex "$elf_file" "${dir_name}/${base_name}.hex"
        
        log_success "Generated ${base_name}.bin and ${base_name}.hex"
    done
}

# Show usage information
show_usage() {
    echo "Usage: $0 [OPTIONS] [TARGET]"
    echo ""
    echo "Options:"
    echo "  -h, --help     Show this help message"
    echo "  -c, --clean    Clean build artifacts before building"
    echo "  -v, --verbose  Enable verbose output"
    echo "  -t, --type     Build type (Debug/Release, default: Release)"
    echo "  -j, --jobs     Number of parallel jobs (default: $(nproc))"
    echo ""
    echo "Targets:"
    echo "  build          Build the project (default)"
    echo "  clean          Clean build artifacts"
    echo "  analyze        Analyze existing build output"
    echo ""
    echo "Environment variables:"
    echo "  BUILD_TYPE     Build configuration (Debug/Release)"
    echo "  PARALLEL_JOBS  Number of parallel build jobs"
    echo "  VERBOSE        Enable verbose output (0/1)"
}

# Main function
main() {
    local target="build"
    local clean_first=0
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_usage
                exit 0
                ;;
            -c|--clean)
                clean_first=1
                shift
                ;;
            -v|--verbose)
                VERBOSE=1
                shift
                ;;
            -t|--type)
                BUILD_TYPE="$2"
                shift 2
                ;;
            -j|--jobs)
                PARALLEL_JOBS="$2"
                shift 2
                ;;
            build|clean|analyze)
                target="$1"
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # Execute target
    case $target in
        clean)
            clean_build
            ;;
        analyze)
            analyze_output
            ;;
        build)
            check_tools
            
            if [ $clean_first -eq 1 ]; then
                clean_build
            fi
            
            # Try CMake first, fallback to Make
            if [ -f "CMakeLists.txt" ]; then
                build_cmake
            elif [ -f "Makefile" ]; then
                build_make
            else
                log_error "No build system found (CMakeLists.txt or Makefile)"
                exit 1
            fi
            
            analyze_output
            log_success "Build completed successfully!"
            ;;
    esac
}

# Run main function
main "$@"