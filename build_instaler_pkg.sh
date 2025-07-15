#!/bin/bash

# Build script for creating 3CLogicScreenRecorder PKG installer

set -e

echo "=== Building 3CLogicScreenRecorder PKG Installer ==="
echo ""

# Step 1: Create installer resources if they don't exist
if [ ! -d "installer/resources" ]; then
    echo "Creating installer resources..."
    ./create_installer_resources.sh
fi

# Step 2: Clean and create build directory
echo "Setting up build environment..."
rm -rf build
mkdir build
cd build

# Step 3: Configure with CMake
echo "Configuring project..."
cmake ..

# Step 4: Build the executable
echo "Building executable..."
make -j$(sysctl -n hw.ncpu)

# Step 5: Build the PKG installer
echo "Creating PKG installer..."
make build_pkg

# Step 6: Verify the package
if [ -f "3CLogicScreenRecorder-1.0.0.pkg" ]; then
    echo ""
    echo "✅ SUCCESS! PKG installer created:"
    echo "   File: build/3CLogicScreenRecorder-1.0.0.pkg"
    echo "   Size: $(du -h 3CLogicScreenRecorder-1.0.0.pkg | cut -f1)"
    echo ""
    echo "To install:"
    echo "   sudo installer -pkg build/3CLogicScreenRecorder-1.0.0.pkg -target /"
    echo ""
    echo "Or double-click the PKG file to use the GUI installer."

    # Optional: Copy to parent directory for easy access
    cp 3CLogicScreenRecorder-1.0.0.pkg ../
else
    echo ""
    echo "❌ Error: PKG file not created"
    exit 1
fi

cd ..

# Step 7: Create a distribution folder (optional)
echo ""
echo "Creating distribution folder..."
mkdir -p dist
cp 3CLogicScreenRecorder-1.0.0.pkg dist/
cp installer/resources/readme.rtf dist/README.rtf

# Create a simple install guide
cat > dist/INSTALL.txt << 'EOF'
3CLogicScreenRecorder Installation Guide
========================================

1. Double-click the 3CLogicScreenRecorder-1.0.0.pkg file
2. Follow the installation wizard
3. After installation, grant permissions:
   - Open System Preferences > Security & Privacy > Privacy
   - Grant Screen Recording permission to 3CLogicScreenRecorder
   - Grant Microphone permission to 3CLogicScreenRecorder
4. The service will start automatically

For command-line installation:
   sudo installer -pkg 3CLogicScreenRecorder-1.0.0.pkg -target /

To verify installation:
   launchctl list | grep 3clogic

For support: support@3clogic.com
EOF

echo "✅ Distribution folder created in ./dist/"
echo ""
echo "Installation package is ready for distribution!"