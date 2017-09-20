# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Start emulator (in background)
emulator64-arm -avd test -noaudio -no-window -no-accel -gpu off -verbose &

# Build and install app & test APKs (while the emulator is booting..)
./gradlew assembleFocusWebviewDebug assembleFocusWebviewDebugAndroidTest

# Start our server for running screencap on the emulator host (via HTTP)
python /opt/tools/screencap-server.py &

# Wait for emulator to finish booting
/opt/tools/android-wait-for-emulator.sh

# Install app and make sure directory for taking screenshot exists.
adb install -r app/build/outputs/apk/app-focus-webview-debug.apk
adb shell mkdir /data/data/org.mozilla.focus.debug/files

# Take screenshots
fastlane screengrab run

# Resize and optimize all screenshots
cd fastlane
find . -type f -iname "*.png" -print0 | xargs -I {} -0 mogrify -resize 75% "{}"
find . -type f -iname "*.png" -print0 | xargs -I {} -0 optipng -o5 "{}"