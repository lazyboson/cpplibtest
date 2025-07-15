#!/bin/bash

# Create installer resources directory and files

echo "Creating installer resources..."

# Create directories
mkdir -p installer/resources

# Create Welcome message
cat > installer/resources/welcome.rtf << 'EOF'
{\rtf1\ansi\ansicpg1252\cocoartf2709
\cocoatextscaling0\cocoaplatform0{\fonttbl\f0\fswiss\fcharset0 Helvetica-Bold;\f1\fswiss\fcharset0 Helvetica;}
{\colortbl;\red255\green255\blue255;}
{\*\expandedcolortbl;;}
\paperw11900\paperh16840\margl1440\margr1440\vieww11520\viewh8400\viewkind0
\pard\tx566\tx1133\tx1700\tx2267\tx2834\tx3401\tx3968\tx4535\tx5102\tx5669\tx6236\tx6803\pardirnatural\partightenfactor0

\f0\b\fs28 \cf0 Welcome to 3CLogicScreenRecorder Installer
\f1\b0\fs24 \
\
This installer will guide you through the installation of 3CLogicScreenRecorder on your Mac.\
\
3CLogicScreenRecorder is a professional screen recording solution that runs in the background and captures your screen and audio.\
\

\f0\b Key Features:
\f1\b0 \
\'95 Background screen recording\
\'95 Audio capture support\
\'95 Automatic startup on login\
\'95 Minimal system resource usage\
\

\f0\b Requirements:
\f1\b0 \
\'95 macOS 10.15 or later\
\'95 Apple Silicon Mac (M1/M2/M3)\
\'95 Screen Recording permission\
\'95 Microphone permission (for audio recording)\
\
Click Continue to proceed with the installation.}
EOF

# Create License
cat > installer/resources/license.rtf << 'EOF'
{\rtf1\ansi\ansicpg1252\cocoartf2709
\cocoatextscaling0\cocoaplatform0{\fonttbl\f0\fswiss\fcharset0 Helvetica-Bold;\f1\fswiss\fcharset0 Helvetica;}
{\colortbl;\red255\green255\blue255;}
{\*\expandedcolortbl;;}
\paperw11900\paperh16840\margl1440\margr1440\vieww11520\viewh8400\viewkind0
\pard\tx566\tx1133\tx1700\tx2267\tx2834\tx3401\tx3968\tx4535\tx5102\tx5669\tx6236\tx6803\pardirnatural\partightenfactor0

\f0\b\fs28 \cf0 Software License Agreement
\f1\b0\fs24 \
\
Copyright \'a9 2025 3CLogic. All rights reserved.\
\

\f0\b IMPORTANT - READ CAREFULLY:
\f1\b0 \
\
This End User License Agreement ("Agreement") is a legal agreement between you and 3CLogic for the 3CLogicScreenRecorder software product.\
\
By installing, copying, or otherwise using the SOFTWARE, you agree to be bound by the terms of this Agreement.\
\

\f0\b 1. LICENSE GRANT
\f1\b0 \
3CLogic grants you a non-exclusive, non-transferable license to use the SOFTWARE on any computer that you own or control.\
\

\f0\b 2. RESTRICTIONS
\f1\b0 \
You may not:\
\'95 Modify, reverse engineer, or decompile the SOFTWARE\
\'95 Rent, lease, or lend the SOFTWARE\
\'95 Remove any proprietary notices from the SOFTWARE\
\

\f0\b 3. PRIVACY
\f1\b0 \
The SOFTWARE requires access to screen recording and microphone. All recordings are processed locally and according to your organization's policies.\
\

\f0\b 4. DISCLAIMER OF WARRANTY
\f1\b0 \
THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND.\
\

\f0\b 5. LIMITATION OF LIABILITY
\f1\b0 \
IN NO EVENT SHALL 3CLOGIC BE LIABLE FOR ANY DAMAGES ARISING FROM THE USE OF THIS SOFTWARE.\
\
By clicking "Agree", you acknowledge that you have read and understand this Agreement and agree to be bound by its terms.}
EOF

# Create Readme
cat > installer/resources/readme.rtf << 'EOF'
{\rtf1\ansi\ansicpg1252\cocoartf2709
\cocoatextscaling0\cocoaplatform0{\fonttbl\f0\fswiss\fcharset0 Helvetica-Bold;\f1\fswiss\fcharset0 Helvetica;\f2\fmodern\fcharset0 Courier;}
{\colortbl;\red255\green255\blue255;}
{\*\expandedcolortbl;;}
\paperw11900\paperh16840\margl1440\margr1440\vieww11520\viewh8400\viewkind0
\pard\tx566\tx1133\tx1700\tx2267\tx2834\tx3401\tx3968\tx4535\tx5102\tx5669\tx6236\tx6803\pardirnatural\partightenfactor0

\f0\b\fs28 \cf0 Post-Installation Instructions
\f1\b0\fs24 \
\

\f0\b IMPORTANT: Grant Required Permissions
\f1\b0 \
\
After installation, you must grant the following permissions for 3CLogicScreenRecorder to function:\
\
1.
\f0\b Screen Recording Permission:
\f1\b0 \
   \'95 Open System Preferences > Security & Privacy > Privacy\
   \'95 Select "Screen Recording" from the left sidebar\
   \'95 Check the box next to "3CLogicScreenRecorder"\
   \'95 You may need to click the lock icon and enter your password\
\
2.
\f0\b Microphone Permission:
\f1\b0 \
   \'95 In the same Privacy settings\
   \'95 Select "Microphone" from the left sidebar\
   \'95 Check the box next to "3CLogicScreenRecorder"\
\

\f0\b Verifying Installation
\f1\b0 \
\
To verify that 3CLogicScreenRecorder is running:\
\
1. Open Terminal\
2. Run:
\f2 launchctl list | grep 3clogic
\f1 \
3. You should see the service listed\
\

\f0\b Log Files
\f1\b0 \
\
Logs are available at:\
\'95
\f2 /tmp/3clogic_screenrecorder.log
\f1 \
\'95
\f2 /tmp/3clogic_screenrecorder_error.log
\f1 \
\

\f0\b Uninstalling
\f1\b0 \
\
To uninstall 3CLogicScreenRecorder:\
\
1. Stop the service:\

\f2 sudo launchctl unload /Library/LaunchAgents/com.3clogic.screenrecorder.plist
\f1 \
\
2. Remove the application:\

\f2 sudo rm -rf "/Applications/3CLogicScreenRecorder.app"
\f1 \
\
3. Remove the LaunchAgent:\

\f2 sudo rm /Library/LaunchAgents/com.3clogic.screenrecorder.plist
\f1 \
\

\f0\b Support
\f1\b0 \
\
For support, please contact: support@3clogic.com}
EOF

# Create uninstall script template
cat > installer/uninstall.sh.in << 'EOF'
#!/bin/bash

echo "Uninstalling @BUNDLE_NAME@..."

# Stop and unload the LaunchAgent
if [ -f "/Library/LaunchAgents/@BUNDLE_ID@.plist" ]; then
    echo "Stopping service..."
    launchctl unload "/Library/LaunchAgents/@BUNDLE_ID@.plist" 2>/dev/null || true
    sudo rm -f "/Library/LaunchAgents/@BUNDLE_ID@.plist"
fi

# Remove the application
if [ -d "/Applications/@BUNDLE_NAME@.app" ]; then
    echo "Removing application..."
    sudo rm -rf "/Applications/@BUNDLE_NAME@.app"
fi

# Remove log files
rm -f /tmp/3clogic_screenrecorder*.log

echo "Uninstallation complete."
EOF

chmod +x installer/uninstall.sh.in

echo "✅ Installer resources created"