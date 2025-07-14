# Get the installation info from the main project
include(CPackConfig.cmake)

# Override only the DMG-specific settings
set(CPACK_GENERATOR "DragNDrop")
set(CPACK_DMG_VOLUME_NAME "3CLogic Screen Recorder")
set(CPACK_DMG_FORMAT "UDZO")

# Remove any AppleScript references
unset(CPACK_DMG_DS_STORE_SETUP_SCRIPT)
unset(CPACK_DMG_BACKGROUND_IMAGE)
