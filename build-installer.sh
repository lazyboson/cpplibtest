#!/bin/bash

# Simple build script for macOS installer
set -e

echo "Building 3CLogic Screen Recorder for macOS..."

# Clean and create build directory
rm -rf cmake-build-release
mkdir -p cmake-build-release
cd cmake-build-release

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_OSX_ARCHITECTURES=arm64 \
         -DCMAKE_INSTALL_PREFIX=/Applications

# Build the project
# shellcheck disable=SC2046
make -j$(sysctl -n hw.ncpu)

# Create installers using the built-in CPack configuration
echo "Creating installers..."

# Create ZIP package
echo "Creating ZIP package..."
cpack -G ZIP

# Create DMG by temporarily removing AppleScript reference
echo "Creating DMG package..."

# Create a temporary minimal CPackConfig.cmake
cat > CPackConfig_DMG.cmake << 'EOF'
# Get the installation info from the main project
include(CPackConfig.cmake)

# Override only the DMG-specific settings
set(CPACK_GENERATOR "DragNDrop")
set(CPACK_DMG_VOLUME_NAME "3CLogic Screen Recorder")
set(CPACK_DMG_FORMAT "UDZO")

# Remove any AppleScript references
unset(CPACK_DMG_DS_STORE_SETUP_SCRIPT)
unset(CPACK_DMG_BACKGROUND_IMAGE)
EOF

# Create DMG using the minimal config
cpack --config CPackConfig_DMG.cmake

echo "Build completed!"
echo "Files created:"
ls -la *.dmg *.zip 2>/dev/null || echo "No installer files found"

# Copy to project root
cp *.dmg ../ 2>/dev/null && echo "DMG copied to project root" || echo "No DMG to copy"
cp *.zip ../ 2>/dev/null && echo "ZIP copied to project root" || echo "No ZIP to copy"

cd ..

echo ""
echo "=== FINAL RESULTS ==="
echo "Installer files in project root:"
ls -la *.dmg *.zip 2>/dev/null || echo "No installer files found"

echo ""
echo "To install:"
echo "1. ZIP: Extract and copy to desired location"
echo "2. DMG: Double-click to mount, then drag to Applications"
