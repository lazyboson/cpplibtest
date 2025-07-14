# Create a gradient background
magick -size 500x300 gradient:#f0f0f0-#e0e0e0 background.png

# Add text overlay
magick background.png \
    -fill black \
    -pointsize 24 \
    -gravity center \
    -annotate +0-50 "3CLogic Screen Recorder" \
    -pointsize 16 \
    -annotate +0-20 "Drag to Applications folder to install" \
    background.png

echo "Background image created at installer/background.png"
echo "You can replace this with your own custom background image."