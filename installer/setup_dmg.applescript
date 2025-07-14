tell application "Finder"
    tell disk "3CLogic Screen Recorder"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {400, 100, 900, 400}
        set viewOptions to the icon view options of container window
        set arrangement of viewOptions to not arranged
        set icon size of viewOptions to 72
        set background picture of viewOptions to file ".background:background.png"

        -- Position the app icon and Applications folder
        set position of item "http_server" of container window to {125, 175}
        set position of item "Applications" of container window to {375, 175}

        close
        open
        update without registering applications
        delay 2
    end tell
end tell