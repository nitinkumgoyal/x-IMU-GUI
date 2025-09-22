#!/bin/bash

# Local Testing Script for STM32 GitLab CI/CD Setup
# This script helps you test the build environment locally before pushing to GitLab

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

# Test Docker installation
test_docker() {
    log_info "Testing Docker installation..."
    
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed or not in PATH"
        log_info "Please install Docker: https://docs.docker.com/get-docker/"
        return 1
    fi
    
    if ! docker info &> /dev/null; then
        log_error "Docker daemon is not running or not accessible"
        log_info "Please start Docker daemon or check permissions"
        return 1
    fi
    
    log_success "Docker is available: $(docker --version)"
    return 0
}

# Test Docker Compose (optional)
test_docker_compose() {
    log_info "Testing Docker Compose..."
    
    if command -v docker-compose &> /dev/null; then
        log_success "Docker Compose is available: $(docker-compose --version)"
        return 0
    elif docker compose version &> /dev/null 2>&1; then
        log_success "Docker Compose (plugin) is available: $(docker compose version)"
        return 0
    else
        log_warning "Docker Compose not found (optional for local development)"
        return 1
    fi
}

# Build Docker image
build_docker_image() {
    log_info "Building STM32 Docker image..."
    
    if [ ! -f "Dockerfile" ]; then
        log_error "Dockerfile not found in current directory"
        return 1
    fi
    
    docker build -t stm32-builder:latest . || {
        log_error "Failed to build Docker image"
        return 1
    }
    
    log_success "Docker image built successfully"
    return 0
}

# Test Docker image
test_docker_image() {
    log_info "Testing Docker image..."
    
    # Test ARM toolchain
    if ! docker run --rm stm32-builder:latest arm-none-eabi-gcc --version; then
        log_error "ARM toolchain test failed"
        return 1
    fi
    
    # Test CMake
    if ! docker run --rm stm32-builder:latest cmake --version; then
        log_error "CMake test failed"
        return 1
    fi
    
    # Test Make
    if ! docker run --rm stm32-builder:latest make --version; then
        log_error "Make test failed"
        return 1
    fi
    
    log_success "Docker image tests passed"
    return 0
}

# Test build scripts
test_build_scripts() {
    log_info "Testing build scripts..."
    
    if [ ! -f "scripts/build.sh" ]; then
        log_error "Build script not found: scripts/build.sh"
        return 1
    fi
    
    if [ ! -x "scripts/build.sh" ]; then
        log_error "Build script is not executable: scripts/build.sh"
        log_info "Run: chmod +x scripts/build.sh"
        return 1
    fi
    
    # Test script help
    if ! ./scripts/build.sh --help > /dev/null; then
        log_error "Build script help test failed"
        return 1
    fi
    
    log_success "Build scripts are ready"
    return 0
}

# Test project structure
test_project_structure() {
    log_info "Testing project structure..."
    
    local errors=0
    
    # Check essential files
    local essential_files=(
        ".gitlab-ci.yml"
        "Dockerfile"
        "cmake/arm-none-eabi-gcc.cmake"
        "scripts/build.sh"
    )
    
    for file in "${essential_files[@]}"; do
        if [ ! -f "$file" ]; then
            log_error "Missing essential file: $file"
            errors=$((errors + 1))
        fi
    done
    
    # Check for build system
    if [ ! -f "CMakeLists.txt" ] && [ ! -f "Makefile" ]; then
        log_warning "No build system found (CMakeLists.txt or Makefile)"
        log_info "You may need to convert your STM32CubeIDE project"
        log_info "Run: ./scripts/convert-cube-project.sh"
    fi
    
    # Check for source files
    local src_count=$(find . -name "*.c" -o -name "*.cpp" | wc -l)
    if [ $src_count -eq 0 ]; then
        log_warning "No source files (*.c, *.cpp) found"
    else
        log_success "Found $src_count source files"
    fi
    
    # Check for header files
    local hdr_count=$(find . -name "*.h" -o -name "*.hpp" | wc -l)
    if [ $hdr_count -eq 0 ]; then
        log_warning "No header files (*.h, *.hpp) found"
    else
        log_success "Found $hdr_count header files"
    fi
    
    if [ $errors -eq 0 ]; then
        log_success "Project structure looks good"
        return 0
    else
        log_error "Project structure has $errors errors"
        return 1
    fi
}

# Test local build
test_local_build() {
    log_info "Testing local build..."
    
    if [ ! -f "CMakeLists.txt" ] && [ ! -f "Makefile" ]; then
        log_warning "Skipping build test - no build system found"
        return 0
    fi
    
    # Test build in Docker container
    if docker run --rm -v "$(pwd)":/workspace stm32-builder:latest ./scripts/build.sh; then
        log_success "Local build test passed"
        return 0
    else
        log_error "Local build test failed"
        return 1
    fi
}

# Test GitLab CI configuration
test_gitlab_ci() {
    log_info "Testing GitLab CI configuration..."
    
    if [ ! -f ".gitlab-ci.yml" ]; then
        log_error ".gitlab-ci.yml not found"
        return 1
    fi
    
    # Basic YAML syntax check (if yq is available)
    if command -v yq &> /dev/null; then
        if yq eval '.stages' .gitlab-ci.yml > /dev/null; then
            log_success "GitLab CI YAML syntax is valid"
        else
            log_error "GitLab CI YAML syntax error"
            return 1
        fi
    else
        log_warning "yq not available - skipping YAML syntax check"
    fi
    
    # Check for required stages
    local required_stages=("build")
    for stage in "${required_stages[@]}"; do
        if ! grep -q "stage: $stage" .gitlab-ci.yml; then
            log_warning "GitLab CI stage '$stage' not found"
        fi
    done
    
    log_success "GitLab CI configuration looks good"
    return 0
}

# Run example project test
test_example_project() {
    log_info "Testing example project..."
    
    if [ ! -d "example-project" ]; then
        log_warning "Example project not found - skipping"
        return 0
    fi
    
    cd example-project
    
    # Test build
    if docker run --rm -v "$(pwd)":/workspace stm32-builder:latest bash -c "
        if [ -f CMakeLists.txt ]; then
            mkdir -p build && cd build
            cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi-gcc.cmake ..
            cmake --build .
        else
            echo 'No CMakeLists.txt found in example project'
            exit 1
        fi
    "; then
        log_success "Example project build test passed"
        cd ..
        return 0
    else
        log_error "Example project build test failed"
        cd ..
        return 1
    fi
}

# Show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Test STM32 GitLab CI/CD setup locally"
    echo ""
    echo "Options:"
    echo "  -h, --help        Show this help message"
    echo "  -q, --quick       Run quick tests only (skip build tests)"
    echo "  -b, --build-only  Run build tests only"
    echo "  -a, --all         Run all tests (default)"
    echo "  --no-docker       Skip Docker-related tests"
    echo "  --example         Test example project only"
}

# Main function
main() {
    local quick_mode=0
    local build_only=0
    local skip_docker=0
    local example_only=0
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_usage
                exit 0
                ;;
            -q|--quick)
                quick_mode=1
                shift
                ;;
            -b|--build-only)
                build_only=1
                shift
                ;;
            -a|--all)
                # Default behavior
                shift
                ;;
            --no-docker)
                skip_docker=1
                shift
                ;;
            --example)
                example_only=1
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    log_info "Starting STM32 GitLab CI/CD local tests..."
    echo ""
    
    local test_count=0
    local passed_count=0
    
    # Run tests based on options
    if [ $example_only -eq 1 ]; then
        # Test example project only
        test_count=$((test_count + 1))
        test_example_project && passed_count=$((passed_count + 1))
    elif [ $build_only -eq 1 ]; then
        # Build tests only
        if [ $skip_docker -eq 0 ]; then
            test_count=$((test_count + 1))
            test_docker && passed_count=$((passed_count + 1))
            
            test_count=$((test_count + 1))
            build_docker_image && passed_count=$((passed_count + 1))
            
            test_count=$((test_count + 1))
            test_docker_image && passed_count=$((passed_count + 1))
        fi
        
        test_count=$((test_count + 1))
        test_local_build && passed_count=$((passed_count + 1))
    else
        # Full test suite (default) or quick mode
        if [ $skip_docker -eq 0 ]; then
            test_count=$((test_count + 1))
            test_docker && passed_count=$((passed_count + 1))
            
            test_count=$((test_count + 1))
            test_docker_compose && passed_count=$((passed_count + 1))
            
            if [ $quick_mode -eq 0 ]; then
                test_count=$((test_count + 1))
                build_docker_image && passed_count=$((passed_count + 1))
                
                test_count=$((test_count + 1))
                test_docker_image && passed_count=$((passed_count + 1))
            fi
        fi
        
        test_count=$((test_count + 1))
        test_project_structure && passed_count=$((passed_count + 1))
        
        test_count=$((test_count + 1))
        test_build_scripts && passed_count=$((passed_count + 1))
        
        test_count=$((test_count + 1))
        test_gitlab_ci && passed_count=$((passed_count + 1))
        
        if [ $quick_mode -eq 0 ]; then
            test_count=$((test_count + 1))
            test_local_build && passed_count=$((passed_count + 1))
            
            test_count=$((test_count + 1))
            test_example_project && passed_count=$((passed_count + 1))
        fi
    fi
    
    # Summary
    echo ""
    log_info "Test Summary: $passed_count/$test_count tests passed"
    
    if [ $passed_count -eq $test_count ]; then
        log_success "All tests passed! Your setup is ready for GitLab CI/CD"
        echo ""
        log_info "Next steps:"
        log_info "1. Copy your STM32 project files to this directory"
        log_info "2. Run: ./scripts/convert-cube-project.sh (if needed)"
        log_info "3. Test build: ./scripts/build.sh"
        log_info "4. Commit and push to GitLab"
        exit 0
    else
        log_error "Some tests failed. Please fix the issues before proceeding."
        exit 1
    fi
}

# Run main function
main "$@"