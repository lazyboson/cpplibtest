cmake_minimum_required(VERSION 3.18)  # Ensure support for Apple Silicon/arm64
project(http_server LANGUAGES C CXX)

# Set the executable name explicitly
set(EXECUTABLE_NAME http_server)

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

# Set the framework search path and rpath correctly
# Update rpath to look for frameworks relative to the executable location
target_link_options(${EXECUTABLE_NAME} PRIVATE
        -F${OBS_FRAMEWORKS_DIR}
        -Wl,-rpath,@executable_path/../lib/apple_silicon/Frameworks
        -Wl,-rpath,@loader_path/../lib/apple_silicon/Frameworks
)

# Set the install name directory for better plugin loading
set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        INSTALL_RPATH "@executable_path/../lib/apple_silicon/Frameworks"
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

# Install configuration
set(CMAKE_INSTALL_PREFIX "/Applications")

# For macOS, we want to create a proper directory structure
# The executable goes into a bin directory
install(TARGETS ${EXECUTABLE_NAME}
        RUNTIME DESTINATION "3CLogic Screen Recorder/bin"
)

# Install the entire lib directory structure as-is
# This preserves the exact structure: lib/apple_silicon/Frameworks and lib/apple_silicon/PlugIns
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/
        DESTINATION "3CLogic Screen Recorder/lib"
        USE_SOURCE_PERMISSIONS
)

# CPack configuration for macOS installer
# Basic CPack settings - MUST come before include(CPack)
set(CPACK_GENERATOR "DragNDrop;ZIP")
set(CPACK_PACKAGE_NAME "3CLogic-Screen-Recorder")
set(CPACK_PACKAGE_VENDOR "3CLogic")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "3CLogic Screen Recorder for M1 MacBook Pro")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

# macOS specific settings
set(CPACK_SYSTEM_NAME "macOS-arm64")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CPACK_SYSTEM_NAME}")

# DragNDrop specific settings
set(CPACK_DMG_VOLUME_NAME "3CLogic Screen Recorder")
set(CPACK_DMG_FORMAT "UDZO")

# Use simpler AppleScript to avoid positioning issues
if(EXISTS "${CMAKE_SOURCE_DIR}/installer/setup_dmg_simple.applescript")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_SOURCE_DIR}/installer/setup_dmg_simple.applescript")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/installer/setup_dmg.applescript")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_SOURCE_DIR}/installer/setup_dmg.applescript")
endif()

# Only set background if file exists
if(EXISTS "${CMAKE_SOURCE_DIR}/installer/background.png")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/installer/background.png")
endif()

# ZIP specific settings
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)

# Include CPack AFTER setting all variables
include(CPack)

# Optional: Show in output where libs are being searched
message(STATUS "OBS Framework Path: ${OBS_FRAMEWORKS_DIR}")
message(STATUS "OBS Plugin Lib Path: ${OBS_PLUGIN_LIB_DIR}")

# Debug: Print the absolute paths
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message(STATUS "Absolute OBS Framework Path: ${CMAKE_SOURCE_DIR}/lib/apple_silicon/Frameworks")

# Custom target for creating installer
add_custom_target(installer
        COMMAND ${CMAKE_COMMAND} --build . --target package
        DEPENDS ${EXECUTABLE_NAME}
        COMMENT "Creating macOS installer..."
)