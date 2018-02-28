# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script takes screenshots of Focus in different locales on
# taskcluster. The locales need to be passed to this script, e.g.:
# ./take-screenshots.sh en de-DE fr

# If a command fails then do not proceed and fail this script too.
set -e

# Make sure we passed locales to this script.
if (( $# < 1 )); then
	echo "No locales passed to this script, e.g.: ./take-screenshots.sh en de-DE fr"
    exit 1
fi

echo "Taking screenshots for locales: $@"
directory="$(dirname "$0")"

# Required for fastlane (otherwise it just crashes randomly)
export LC_ALL="en_US.UTF-8"

# Start emulator (in background)
emulator64-arm -avd test -noaudio -no-window -no-accel -gpu off -verbose &

# Build and install app & test APKs (while the emulator is booting..)
./gradlew --no-daemon assembleFocusWebviewDebug assembleFocusWebviewDebugAndroidTest

# Start our server for running screencap on the emulator host (via HTTP)
python $directory/screencap-server.py &

# Generate Screengrab configuration
python $directory/generate_screengrab_config.py $@

# Wait for emulator to finish booting
/opt/focus-android/tools/taskcluster/android-wait-for-emulator.sh

# Install app and make sure directory for taking screenshot exists.
adb install -r app/build/outputs/apk/app-focus-webview-debug.apk
adb shell mkdir /data/data/org.mozilla.focus.debug/files

# Take screenshots
fastlane screengrab run

# Resize and optimize all screenshots
cd fastlane
find . -type f -iname "*.png" -print0 | xargs -I {} -0 mogrify -resize 75% "{}"
find . -type f -iname "*.png" -print0 | xargs -I {} -0 optipng -o5 "{}"