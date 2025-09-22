#!/bin/bash

# STM32CubeIDE to Command-Line Build Converter
# This script helps convert STM32CubeIDE projects to command-line buildable projects

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

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

# Check if STM32CubeIDE project exists
check_cube_project() {
    log_info "Checking for STM32CubeIDE project files..."
    
    local ioc_file=$(find . -name "*.ioc" | head -1)
    local cproject_file=$(find . -name ".cproject" | head -1)
    local project_file=$(find . -name ".project" | head -1)
    
    if [ -n "$ioc_file" ]; then
        log_success "Found STM32CubeMX configuration: $ioc_file"
        IOC_FILE="$ioc_file"
    fi
    
    if [ -n "$cproject_file" ] && [ -n "$project_file" ]; then
        log_success "Found STM32CubeIDE project files"
        CUBE_PROJECT=1
    else
        log_warning "No STM32CubeIDE project files found"
        CUBE_PROJECT=0
    fi
}

# Generate Makefile from STM32CubeIDE project
generate_makefile() {
    if [ $CUBE_PROJECT -eq 0 ]; then
        log_warning "No STM32CubeIDE project found, skipping Makefile generation"
        return
    fi
    
    log_info "Generating Makefile from STM32CubeIDE project..."
    
    # Look for existing Makefile
    if [ -f "Makefile" ]; then
        log_warning "Makefile already exists, backing up as Makefile.backup"
        cp Makefile Makefile.backup
    fi
    
    # Try to use STM32CubeIDE's built-in Makefile generation
    # This usually requires the project to be properly configured
    log_info "Looking for generated Makefile in Debug/Release directories..."
    
    local debug_makefile=$(find . -path "*/Debug/makefile" -o -path "*/Debug/Makefile" | head -1)
    local release_makefile=$(find . -path "*/Release/makefile" -o -path "*/Release/Makefile" | head -1)
    
    if [ -n "$debug_makefile" ]; then
        log_success "Found Debug Makefile: $debug_makefile"
        cp "$debug_makefile" ./Makefile
        log_success "Copied Debug Makefile to project root"
        return
    fi
    
    if [ -n "$release_makefile" ]; then
        log_success "Found Release Makefile: $release_makefile"
        cp "$release_makefile" ./Makefile
        log_success "Copied Release Makefile to project root"
        return
    fi
    
    log_warning "No generated Makefile found. You may need to:"
    log_warning "1. Open the project in STM32CubeIDE"
    log_warning "2. Build it once to generate the Makefile"
    log_warning "3. Or manually create a Makefile"
}

# Generate CMakeLists.txt for the project
generate_cmake() {
    log_info "Generating CMakeLists.txt..."
    
    if [ -f "CMakeLists.txt" ]; then
        log_warning "CMakeLists.txt already exists, backing up as CMakeLists.txt.backup"
        cp CMakeLists.txt CMakeLists.txt.backup
    fi
    
    # Detect project name
    local project_name="stm32_project"
    if [ -n "$IOC_FILE" ]; then
        project_name=$(basename "$IOC_FILE" .ioc)
    elif [ -f ".project" ]; then
        project_name=$(grep -oP '(?<=<name>)[^<]+' .project | head -1 2>/dev/null || echo "stm32_project")
    fi
    
    # Detect MCU family
    local mcu_family="STM32F4xx"
    if [ -n "$IOC_FILE" ]; then
        local mcu_line=$(grep -i "Mcu.Name=" "$IOC_FILE" | head -1)
        if [[ $mcu_line == *"STM32F0"* ]]; then
            mcu_family="STM32F0xx"
        elif [[ $mcu_line == *"STM32F1"* ]]; then
            mcu_family="STM32F1xx"
        elif [[ $mcu_line == *"STM32F2"* ]]; then
            mcu_family="STM32F2xx"
        elif [[ $mcu_line == *"STM32F3"* ]]; then
            mcu_family="STM32F3xx"
        elif [[ $mcu_line == *"STM32F4"* ]]; then
            mcu_family="STM32F4xx"
        elif [[ $mcu_line == *"STM32F7"* ]]; then
            mcu_family="STM32F7xx"
        elif [[ $mcu_line == *"STM32H7"* ]]; then
            mcu_family="STM32H7xx"
        elif [[ $mcu_line == *"STM32L0"* ]]; then
            mcu_family="STM32L0xx"
        elif [[ $mcu_line == *"STM32L1"* ]]; then
            mcu_family="STM32L1xx"
        elif [[ $mcu_line == *"STM32L4"* ]]; then
            mcu_family="STM32L4xx"
        fi
    fi
    
    # Find source files
    local src_files=$(find . -name "*.c" -not -path "./build/*" -not -path "./.git/*" | sort)
    local inc_dirs=$(find . -name "*.h" -not -path "./build/*" -not -path "./.git/*" -exec dirname {} \; | sort -u)
    
    # Find linker script
    local linker_script=$(find . -name "*.ld" | head -1)
    
    cat > CMakeLists.txt << EOF
cmake_minimum_required(VERSION 3.16)

# Project configuration
project(${project_name} C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# MCU configuration
set(MCU_FAMILY ${mcu_family})
set(MCU_MODEL ${mcu_family}) # Update this to specific model if needed

# Toolchain configuration
set(CMAKE_TOOLCHAIN_FILE \${CMAKE_SOURCE_DIR}/cmake/arm-none-eabi-gcc.cmake)

# Compiler definitions
add_definitions(
    -DUSE_HAL_DRIVER
    -D\${MCU_FAMILY}
    # Add other definitions as needed
)

# Include directories
include_directories(
EOF
    
    # Add include directories
    for inc_dir in $inc_dirs; do
        echo "    $inc_dir" >> CMakeLists.txt
    done
    
    cat >> CMakeLists.txt << EOF
)

# Source files
set(SOURCES
EOF
    
    # Add source files
    for src_file in $src_files; do
        echo "    $src_file" >> CMakeLists.txt
    done
    
    cat >> CMakeLists.txt << EOF
)

# Create executable
add_executable(\${PROJECT_NAME}.elf \${SOURCES})

# Linker script
EOF
    
    if [ -n "$linker_script" ]; then
        echo "set_target_properties(\${PROJECT_NAME}.elf PROPERTIES LINK_FLAGS \"-T$linker_script\")" >> CMakeLists.txt
    else
        echo "# TODO: Add linker script path" >> CMakeLists.txt
        echo "# set_target_properties(\${PROJECT_NAME}.elf PROPERTIES LINK_FLAGS \"-Tpath/to/linker.ld\")" >> CMakeLists.txt
    fi
    
    cat >> CMakeLists.txt << EOF

# Generate additional output files
add_custom_command(TARGET \${PROJECT_NAME}.elf POST_BUILD
    COMMAND \${CMAKE_OBJCOPY} -O ihex \$<TARGET_FILE:\${PROJECT_NAME}.elf> \${PROJECT_NAME}.hex
    COMMAND \${CMAKE_OBJCOPY} -O binary \$<TARGET_FILE:\${PROJECT_NAME}.elf> \${PROJECT_NAME}.bin
    COMMAND \${CMAKE_SIZE} \$<TARGET_FILE:\${PROJECT_NAME}.elf>
    COMMENT "Generating \${PROJECT_NAME}.hex and \${PROJECT_NAME}.bin"
)
EOF
    
    log_success "Generated CMakeLists.txt for project: $project_name"
    log_info "MCU Family detected as: $mcu_family"
    log_info "Found $(echo "$src_files" | wc -l) source files"
    log_info "Found $(echo "$inc_dirs" | wc -l) include directories"
    
    if [ -z "$linker_script" ]; then
        log_warning "No linker script (.ld) found. Please add it manually to CMakeLists.txt"
    fi
}

# Create project structure
create_project_structure() {
    log_info "Creating recommended project structure..."
    
    # Create directories if they don't exist
    mkdir -p cmake
    mkdir -p scripts
    mkdir -p docs
    
    # Copy toolchain file if it doesn't exist
    if [ ! -f "cmake/arm-none-eabi-gcc.cmake" ]; then
        if [ -f "../cmake/arm-none-eabi-gcc.cmake" ]; then
            cp ../cmake/arm-none-eabi-gcc.cmake cmake/
            log_success "Copied ARM toolchain file"
        fi
    fi
    
    # Create .gitignore if it doesn't exist
    if [ ! -f ".gitignore" ]; then
        cat > .gitignore << EOF
# Build directories
build/
Debug/
Release/

# Generated files
*.elf
*.bin
*.hex
*.map
*.list
*.lst

# IDE files
.metadata/
.settings/
*.launch

# OS files
.DS_Store
Thumbs.db

# Temporary files
*~
*.tmp
*.bak
*.swp
EOF
        log_success "Created .gitignore"
    fi
}

# Show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Convert STM32CubeIDE project to command-line buildable project"
    echo ""
    echo "Options:"
    echo "  -h, --help      Show this help message"
    echo "  -m, --makefile  Generate/copy Makefile only"
    echo "  -c, --cmake     Generate CMakeLists.txt only"
    echo "  -a, --all       Generate all build files (default)"
}

# Main function
main() {
    local generate_makefile_only=0
    local generate_cmake_only=0
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_usage
                exit 0
                ;;
            -m|--makefile)
                generate_makefile_only=1
                shift
                ;;
            -c|--cmake)
                generate_cmake_only=1
                shift
                ;;
            -a|--all)
                # Default behavior
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    log_info "Converting STM32CubeIDE project to command-line build..."
    
    check_cube_project
    
    if [ $generate_makefile_only -eq 1 ]; then
        generate_makefile
    elif [ $generate_cmake_only -eq 1 ]; then
        generate_cmake
    else
        # Generate all
        generate_makefile
        generate_cmake
        create_project_structure
    fi
    
    log_success "Conversion completed!"
    log_info "Next steps:"
    log_info "1. Review generated CMakeLists.txt/Makefile"
    log_info "2. Test build with: ./scripts/build.sh"
    log_info "3. Commit changes to Git"
    log_info "4. Push to GitLab to trigger CI/CD pipeline"
}

# Initialize variables
IOC_FILE=""
CUBE_PROJECT=0

# Run main function
main "$@"