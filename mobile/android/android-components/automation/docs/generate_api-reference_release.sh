# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Print commands and fail this script if a sub script fails
set -ex

# Get version number from Gradle
VERSION=`TERM=dumb ./gradlew printVersion | sed -n 's/.*version: //p'`
echo "API reference for version: $VERSION"

# Generate API reference for the current state
./gradlew docs

# Move generated reference to destination
mkdir -p ./docs/api/$VERSION
cp -R ./build/javadoc/android-components/* ./docs/api/$VERSION/

# Generate a page that links to the reference pages for every module
python automation/docs/generate_reference_page.py $VERSION
