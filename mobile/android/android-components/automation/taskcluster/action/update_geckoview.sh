# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# If a command fails then do not proceed and fail this script too.
set -ex

# TODO: This script should only run if there's no PR open already.

CHANNEL=$1

# Setup git
git config --global user.email "sebastian@mozilla.com"
git config --global user.name "MickeyMoz"

# Use Gradle plugin to look for new GeckoView version (non major version update!)
if [ "$CHANNEL" = "nightly" ]
then
    ./gradlew updateGVNightlyVersion
elif [ "$CHANNEL" = "beta" ]
then
    ./gradlew updateGVBetaVersion
elif [ "$CHANNEL" = "release" ]
then
    ./gradlew updateGVStableVersion
else
    echo "Unknown channel: $CHANNEL"
    exit 1
fi

# Timestamp used in branch name and commit
TIMESTAMP=`date "+%Y%m%d-%H%M%S"`
BRANCH="GV-$CHANNEL-$TIMESTAMP"

# Create a branch and commit local changes
git checkout -b $BRANCH
git add buildSrc/src/main/java/Gecko.kt
git commit -m \
	"Update GeckoView ($CHANNEL) ($TIMESTAMP)" \
	--author="MickeyMoz <sebastian@mozilla.com>" \
|| { echo "No new GeckoView version ($CHANNEL) available"; exit 0; }

# Build and test engine component as well as sample browser
if [ "$CHANNEL" = "nightly" ]
then
    ./gradlew browser-engine-gecko-nightly:assemble \
          browser-engine-gecko-nightly:test \
          sample-browser:assembleGeckoNightlyArm
elif [ "$CHANNEL" = "beta" ]
then
    ./gradlew browser-engine-gecko-beta:assemble \
          browser-engine-gecko-beta:test \
          sample-browser:assembleGeckoBetaArm
elif [ "$CHANNEL" = "release" ]
then
    ./gradlew browser-engine-gecko:assemble \
          browser-engine-gecko:test \
          sample-browser:assembleGeckoReleaseArm
else
    echo "Unknown channel: $CHANNEL"
    exit 1
fi

exit 28

# Get token for using GitHub
python automation/taskcluster/helper/get-secret.py \
    -s project/mobile/github \
    -k botAccountToken \
    -f .github_token \

# From here on we do not want to print the commands since they contain tokens
set +x

USER='MickeyMoz'
REPO='android-components'
TOKEN=`cat .github_token`
URL="https://$USER:$TOKEN@github.com/$USER/$REPO/"

echo "token=$TOKEN" > token.properties

echo "Pushing branch to GitHub"
git push  --no-verify --quiet $URL $BRANCH
echo "Done ($?)"

echo "Opening pull request"
./gradlew openPR -Ptitle="GeckoView update ($TIMESTAMP)" -Pbranch="$BRANCH" -PtokenFile="token.properties"
