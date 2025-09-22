# STM32 GitLab CI/CD Setup

This repository contains a complete setup for building STM32 firmware using GitLab CI/CD pipelines in Docker containers, eliminating the need for STM32CubeIDE in your build environment.

## 🚀 Quick Start

### 1. For Existing STM32CubeIDE Projects

If you have an existing STM32CubeIDE project:

```bash
# Convert your STM32CubeIDE project to command-line buildable
./scripts/convert-cube-project.sh

# Test the build locally
./scripts/build.sh

# Commit and push to trigger CI/CD
git add .
git commit -m "Add CI/CD configuration"
git push
```

### 2. For New Projects

Copy the following files to your STM32 project repository:

- `Dockerfile` - Docker image with ARM toolchain
- `.gitlab-ci.yml` - GitLab CI/CD pipeline configuration
- `cmake/arm-none-eabi-gcc.cmake` - CMake toolchain file
- `scripts/build.sh` - Build script
- `scripts/convert-cube-project.sh` - Project conversion script

## 📁 Project Structure

```
your-stm32-project/
├── .gitlab-ci.yml              # GitLab CI/CD configuration
├── Dockerfile                  # Docker build environment
├── CMakeLists.txt             # CMake build configuration (recommended)
├── Makefile                   # Make build configuration (alternative)
├── cmake/
│   └── arm-none-eabi-gcc.cmake # ARM toolchain configuration
├── scripts/
│   ├── build.sh               # Unified build script
│   └── convert-cube-project.sh # STM32CubeIDE converter
├── src/                       # Source files
├── inc/                       # Header files
└── linker/                    # Linker scripts
```

## 🐳 Docker Build Environment

The included `Dockerfile` creates a complete STM32 build environment with:

- **ARM GNU Toolchain** (latest LTS version)
- **CMake** and **Make** build systems
- **OpenOCD** for debugging/flashing
- **Python tools** (stm32pio, pyocd)
- **Static analysis tools**

### Building the Docker Image

```bash
# Build locally
docker build -t stm32-builder:latest .

# Or use pre-built images
# docker pull jasonyangee/stm32-builder:ubuntu-latest
# docker pull chille/stm32-docker:latest
```

### Using the Docker Image Locally

```bash
# Run interactive container
docker run -it --rm -v $(pwd):/workspace stm32-builder:latest

# Build project in container
docker run --rm -v $(pwd):/workspace stm32-builder:latest ./scripts/build.sh
```

## 🔧 Build Systems

### CMake (Recommended)

CMake provides better dependency management and is more suitable for complex projects:

```bash
# Configure and build
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi-gcc.cmake ..
cmake --build . --parallel

# Or use the build script
./scripts/build.sh
```

### Make (Legacy)

For projects with existing Makefiles:

```bash
# Build with Make
make -j$(nproc) all

# Or use the build script
./scripts/build.sh
```

## 🚀 GitLab CI/CD Pipeline

The pipeline includes the following stages:

### 1. **Validate** - Project structure validation
- Checks for build system files (CMakeLists.txt/Makefile)
- Validates source file structure
- Verifies toolchain availability

### 2. **Build** - Firmware compilation
- Supports both CMake and Make builds
- Parallel compilation for faster builds
- Generates ELF, BIN, and HEX files
- Caches build artifacts

### 3. **Test** - Quality assurance
- **Static Analysis**: Code quality checks with cppcheck
- **Unit Tests**: Automated testing (if configured)
- **Size Analysis**: Memory usage analysis

### 4. **Deploy** - Deployment automation
- Development deployment (automatic on develop branch)
- Production deployment (manual approval required)

### Pipeline Configuration

Key variables in `.gitlab-ci.yml`:

```yaml
variables:
  STM32_BUILD_IMAGE: "stm32-builder:latest"  # Docker image
  BUILD_TYPE: "Release"                       # Debug/Release
  TARGET_MCU: "STM32F4xx"                    # MCU family
  ARTIFACTS_EXPIRE_IN: "1 week"              # Artifact retention
```

## 🛠️ Build Scripts

### build.sh

Unified build script with the following features:

```bash
# Basic usage
./scripts/build.sh                    # Build project
./scripts/build.sh clean              # Clean artifacts
./scripts/build.sh analyze            # Analyze existing build

# Advanced options
./scripts/build.sh -c -v -t Debug     # Clean, verbose, debug build
./scripts/build.sh -j 8               # Use 8 parallel jobs
```

### convert-cube-project.sh

Converts STM32CubeIDE projects to command-line buildable projects:

```bash
# Convert entire project
./scripts/convert-cube-project.sh

# Generate only CMakeLists.txt
./scripts/convert-cube-project.sh --cmake

# Generate only Makefile
./scripts/convert-cube-project.sh --makefile
```

## 🔍 Troubleshooting

### Common Issues

1. **Build fails with "arm-none-eabi-gcc not found"**
   - Ensure you're using the correct Docker image
   - Check that the ARM toolchain is properly installed

2. **Linker errors**
   - Verify your linker script (.ld file) is correct
   - Check memory definitions match your MCU

3. **Missing include files**
   - Ensure all HAL/CMSIS libraries are included
   - Check include paths in CMakeLists.txt/Makefile

4. **Pipeline fails on validation stage**
   - Ensure CMakeLists.txt or Makefile exists
   - Check that source files are present

### Debug Build Issues

```bash
# Build locally with verbose output
./scripts/build.sh -v

# Check build tools
arm-none-eabi-gcc --version
cmake --version
make --version

# Analyze ELF file
arm-none-eabi-objdump -h firmware.elf
arm-none-eabi-size firmware.elf
```

## 📊 Memory Analysis

The pipeline automatically generates memory usage reports:

```
=== Memory Usage ===
   text    data     bss     dec     hex filename
  45678    1234    5678   52590    cd6e firmware.elf

=== Section Details ===
Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .isr_vector   000001c4  08000000  08000000  00010000  2**0
  1 .text         0000b26e  080001c4  080001c4  000101c4  2**2
  2 .rodata       00000f18  0800b432  0800b432  0001b432  2**2
```

## 🔐 Security Considerations

- Never commit sensitive data (keys, certificates) to the repository
- Use GitLab CI/CD variables for secrets
- Consider using signed commits for production deployments
- Regularly update the Docker base image and toolchain

## 📚 Additional Resources

- [STM32CubeMX Documentation](https://www.st.com/en/development-tools/stm32cubemx.html)
- [ARM GNU Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
- [GitLab CI/CD Documentation](https://docs.gitlab.com/ee/ci/)
- [CMake Documentation](https://cmake.org/documentation/)

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test with your STM32 project
5. Submit a pull request

## 📄 License

This configuration is provided as-is for educational and development purposes. Please ensure compliance with your organization's policies and relevant licenses for the tools used.