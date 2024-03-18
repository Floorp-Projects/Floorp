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

# This will be 'engine-gecko-$CHANNEL' for non-release channels.
# Intentionally left empty.
COMPONENT_NAME=

# Build and test engine component as well as sample browser
if [ "$CHANNEL" = "nightly" ]
then
    COMPONENT_NAME="engine-gecko-nightly"
elif [ "$CHANNEL" = "beta" ]
then
    COMPONENT_NAME="engine-gecko-beta"
elif [ "$CHANNEL" = "release" ]
then
    COMPONENT_NAME="engine-gecko"
else
    echo "Unknown channel: $CHANNEL"
    exit 1
fi

# The build system automatically updates the docs. Add them to the
# docs PR, if they exist.
if git add components/browser/${COMPONENT_NAME}/docs/metrics.md ; then
    git commit --no-edit --amend
fi

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
git push  --no-verify --quiet $URL $BRANCH > /dev/null 2>&1 || echo "Failed ($?)"
echo "Done ($?)"

echo "Opening pull request"
./gradlew openPR -Ptitle="GeckoView update ($CHANNEL) ($TIMESTAMP)" -Pbranch="$BRANCH" -PtokenFile="token.properties"
