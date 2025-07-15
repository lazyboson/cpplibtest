cmake_minimum_required(VERSION 3.18)
project(http_server LANGUAGES C CXX)

# Set the executable name explicitly
set(EXECUTABLE_NAME http_server)
set(BUNDLE_NAME "3CLogicScreenRecorder")
set(BUNDLE_ID "com.3clogic.screenrecorder")

# Version information
set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

# Architecture and build options
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable warnings and optimization
add_compile_options(-Wall -O2)

# OBS include and lib paths
set(OBS_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/obs/include")
set(OBS_FRAMEWORKS_DIR "${CMAKE_SOURCE_DIR}/lib/apple_silicon/Frameworks")
set(OBS_PLUGIN_LIB_DIR "${CMAKE_SOURCE_DIR}/lib/apple_silicon/PlugIns")

include_directories(${OBS_INCLUDE_DIR})

# Source files
set(SOURCES
        main.cpp
)

# Create the executable
add_executable(${EXECUTABLE_NAME} ${SOURCES})

# Set rpath for app bundle structure
target_link_options(${EXECUTABLE_NAME} PRIVATE
        -F${OBS_FRAMEWORKS_DIR}
        -Wl,-rpath,@executable_path/../Frameworks
        -Wl,-rpath,@loader_path/../Frameworks
)

set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH TRUE
)

# Link against OBS framework and system frameworks
target_link_libraries(${EXECUTABLE_NAME}
        "-framework libobs"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AVFoundation"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework VideoToolbox"
        "-framework OpenGL"
)

# Set staging directory
set(STAGING_DIR "${CMAKE_BINARY_DIR}/staging")

# Create Info.plist
configure_file(
        "${CMAKE_SOURCE_DIR}/installer/Info.plist.in"
        "${CMAKE_BINARY_DIR}/Info.plist"
        @ONLY
)

# Create LaunchAgent plist
configure_file(
        "${CMAKE_SOURCE_DIR}/installer/com.3clogic.screenrecorder.plist.in"
        "${CMAKE_BINARY_DIR}/com.3clogic.screenrecorder.plist"
        @ONLY
)

# Install executable into app bundle (relative paths only)
install(TARGETS ${EXECUTABLE_NAME}
        RUNTIME DESTINATION "Applications/${BUNDLE_NAME}.app/Contents/MacOS"
        COMPONENT app
)

# Install Info.plist
install(FILES "${CMAKE_BINARY_DIR}/Info.plist"
        DESTINATION "Applications/${BUNDLE_NAME}.app/Contents"
        COMPONENT app
)

# Install frameworks
install(DIRECTORY "${OBS_FRAMEWORKS_DIR}/"
        DESTINATION "Applications/${BUNDLE_NAME}.app/Contents/Frameworks"
        COMPONENT app
        USE_SOURCE_PERMISSIONS
)

# Install plugins
install(DIRECTORY "${OBS_PLUGIN_LIB_DIR}/"
        DESTINATION "Applications/${BUNDLE_NAME}.app/Contents/PlugIns"
        COMPONENT app
        USE_SOURCE_PERMISSIONS
)

# Install LaunchAgent plist
install(FILES "${CMAKE_BINARY_DIR}/com.3clogic.screenrecorder.plist"
        DESTINATION "Library/LaunchAgents"
        COMPONENT launchagent
)

# Create scripts
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/scripts")

file(WRITE "${CMAKE_BINARY_DIR}/scripts/postinstall"
        "#!/bin/bash
# Post-installation script for 3CLogicScreenRecorder

echo \"Configuring 3CLogicScreenRecorder...\"

# The executable needs to be renamed after installation
if [ -f \"/Applications/${BUNDLE_NAME}.app/Contents/MacOS/${EXECUTABLE_NAME}\" ]; then
    mv \"/Applications/${BUNDLE_NAME}.app/Contents/MacOS/${EXECUTABLE_NAME}\" \"/Applications/${BUNDLE_NAME}.app/Contents/MacOS/${BUNDLE_NAME}\"
fi

# Load LaunchAgent
if [ -f \"/Library/LaunchAgents/com.3clogic.screenrecorder.plist\" ]; then
    launchctl unload /Library/LaunchAgents/com.3clogic.screenrecorder.plist 2>/dev/null || true
    launchctl load -w /Library/LaunchAgents/com.3clogic.screenrecorder.plist 2>/dev/null || true
fi

# Create log files
touch /tmp/3clogic_screenrecorder.log
touch /tmp/3clogic_screenrecorder_error.log
chmod 666 /tmp/3clogic_screenrecorder*.log

echo \"Installation complete. Please grant Screen Recording and Microphone permissions.\"
exit 0
")

file(WRITE "${CMAKE_BINARY_DIR}/scripts/preinstall"
        "#!/bin/bash
# Pre-installation script

# Stop any running instance
if [ -f \"/Library/LaunchAgents/com.3clogic.screenrecorder.plist\" ]; then
    launchctl unload /Library/LaunchAgents/com.3clogic.screenrecorder.plist 2>/dev/null || true
fi

pkill -f 3CLogicScreenRecorder 2>/dev/null || true
exit 0
")

# Make scripts executable
file(CHMOD "${CMAKE_BINARY_DIR}/scripts/postinstall"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
file(CHMOD "${CMAKE_BINARY_DIR}/scripts/preinstall"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Create distribution.xml
configure_file(
        "${CMAKE_SOURCE_DIR}/installer/distribution.xml.in"
        "${CMAKE_BINARY_DIR}/distribution.xml"
        @ONLY
)

# Custom target to build PKG
add_custom_target(build_pkg
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${STAGING_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${STAGING_DIR}"

        # Install components to staging
        COMMAND ${CMAKE_COMMAND} --install . --prefix "${STAGING_DIR}" --component app
        COMMAND ${CMAKE_COMMAND} --install . --prefix "${STAGING_DIR}" --component launchagent

        # Build component packages
        COMMAND pkgbuild --root "${STAGING_DIR}/Applications"
        --identifier "${BUNDLE_ID}.app"
        --version "${PROJECT_VERSION}"
        --install-location "/Applications"
        "${CMAKE_BINARY_DIR}/app.pkg"

        COMMAND pkgbuild --root "${STAGING_DIR}/Library"
        --identifier "${BUNDLE_ID}.launchagent"
        --version "${PROJECT_VERSION}"
        --scripts "${CMAKE_BINARY_DIR}/scripts"
        --install-location "/Library"
        "${CMAKE_BINARY_DIR}/launchagent.pkg"

        # Build final distribution package
        COMMAND productbuild --distribution "${CMAKE_BINARY_DIR}/distribution.xml"
        --package-path "${CMAKE_BINARY_DIR}"
        --resources "${CMAKE_SOURCE_DIR}/installer/resources"
        "${CMAKE_BINARY_DIR}/${BUNDLE_NAME}-${PROJECT_VERSION}.pkg"

        DEPENDS ${EXECUTABLE_NAME}
        COMMENT "Building macOS PKG installer..."
)

# Debug info
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Bundle Name: ${BUNDLE_NAME}")
message(STATUS "Bundle ID: ${BUNDLE_ID}")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "OBS Framework Path: ${OBS_FRAMEWORKS_DIR}")
message(STATUS "OBS Plugin Path: ${OBS_PLUGIN_LIB_DIR}")