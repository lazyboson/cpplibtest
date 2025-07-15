#!/bin/bash

# Manual PKG builder that ensures files are included
# Usage: build_pkg_manual.sh <install_root> <build_dir> <version>

INSTALL_ROOT="$1"
BUILD_DIR="$2"
VERSION="$3"

if [ $# -ne 3 ]; then
    echo "Usage: $0 <install_root> <build_dir> <version>"
    exit 1
fi

echo "=== Building PKG Manually ==="
echo "Install root: $INSTALL_ROOT"
echo "Build dir: $BUILD_DIR"
echo "Version: $VERSION"

# Verify files exist
APP_DIR="${INSTALL_ROOT}/Applications/3CLogic Screen Recorder"
if [ ! -f "${APP_DIR}/bin/http_server" ]; then
    echo "❌ Error: http_server not found in ${APP_DIR}/bin/"
    echo "Contents of ${APP_DIR}:"
    find "${APP_DIR}" -type f
    exit 1
fi

# Create package build directory
PKG_BUILD="${BUILD_DIR}/manual_pkg_build"
rm -rf "${PKG_BUILD}"
mkdir -p "${PKG_BUILD}/scripts"
mkdir -p "${PKG_BUILD}/resources"

# Create postinstall script
cat > "${PKG_BUILD}/scripts/postinstall" << 'EOF'
#!/bin/bash

INSTALL_DIR="/Applications/3CLogic Screen Recorder"
LAUNCHD_PLIST="com.3clogic.screenrecorder"
DAEMON_DIR="${INSTALL_DIR}/daemon"
LAUNCHD_PATH="/Library/LaunchDaemons/${LAUNCHD_PLIST}.plist"

echo "Configuring 3CLogic Screen Recorder daemon..."

# Stop existing daemon if running
launchctl unload "${LAUNCHD_PATH}" 2>/dev/null || true

# Copy LaunchDaemon plist to system location
if [ -f "${DAEMON_DIR}/${LAUNCHD_PLIST}.plist" ]; then
    cp "${DAEMON_DIR}/${LAUNCHD_PLIST}.plist" "${LAUNCHD_PATH}"
    chown root:wheel "${LAUNCHD_PATH}"
    chmod 644 "${LAUNCHD_PATH}"

    # Load and start the daemon
    launchctl load "${LAUNCHD_PATH}"
    echo "✅ Daemon configured and started"
else
    echo "⚠️  Warning: LaunchDaemon plist not found"
fi

# Set permissions
chown -R root:admin "${INSTALL_DIR}"
chmod -R 755 "${INSTALL_DIR}"

# Create log files
mkdir -p /var/log
touch /var/log/3clogic_screenrecorder.log
touch /var/log/3clogic_screenrecorder_error.log
chown root:wheel /var/log/3clogic_screenrecorder*.log
chmod 644 /var/log/3clogic_screenrecorder*.log

echo "✅ Installation completed"
exit 0
EOF

chmod +x "${PKG_BUILD}/scripts/postinstall"

# Create a simple component package first
echo "Creating component package..."
pkgbuild --root "${INSTALL_ROOT}" \
         --identifier "com.3clogic.screenrecorder" \
         --version "${VERSION}" \
         --scripts "${PKG_BUILD}/scripts" \
         --install-location "/" \
         "${PKG_BUILD}/component.pkg"

if [ ! -f "${PKG_BUILD}/component.pkg" ]; then
    echo "❌ Failed to create component package"
    exit 1
fi

# Check component package size
COMP_SIZE=$(du -h "${PKG_BUILD}/component.pkg" | cut -f1)
echo "Component package size: $COMP_SIZE"

# Create distribution XML
cat > "${PKG_BUILD}/distribution.xml" << EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2.0">
    <title>3CLogic Screen Recorder</title>
    <organization>com.3clogic</organization>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <options customize="never" require-scripts="true" rootVolumeOnly="true" />

    <choices-outline>
        <line choice="default">
            <line choice="com.3clogic.screenrecorder"/>
        </line>
    </choices-outline>

    <choice id="default"/>
    <choice id="com.3clogic.screenrecorder" visible="false">
        <pkg-ref id="com.3clogic.screenrecorder"/>
    </choice>

    <pkg-ref id="com.3clogic.screenrecorder">component.pkg</pkg-ref>
</installer-gui-script>
EOF

# Create final distribution package
echo "Creating distribution package..."
productbuild --distribution "${PKG_BUILD}/distribution.xml" \
             --package-path "${PKG_BUILD}" \
             "${BUILD_DIR}/3CLogic-Screen-Recorder-${VERSION}.pkg"

if [ -f "${BUILD_DIR}/3CLogic-Screen-Recorder-${VERSION}.pkg" ]; then
    FINAL_SIZE=$(du -h "${BUILD_DIR}/3CLogic-Screen-Recorder-${VERSION}.pkg" | cut -f1)
    echo "✅ Package created successfully!"
    echo "📦 Package: ${BUILD_DIR}/3CLogic-Screen-Recorder-${VERSION}.pkg"
    echo "📏 Size: $FINAL_SIZE"

    # Verify package contents
    echo ""
    echo "Verifying package contents..."
    installer -pkg "${BUILD_DIR}/3CLogic-Screen-Recorder-${VERSION}.pkg" -dominfo
else
    echo "❌ Failed to create final package"
    exit 1
fi