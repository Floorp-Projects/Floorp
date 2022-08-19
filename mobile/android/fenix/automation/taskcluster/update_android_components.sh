# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# If a command fails then do not proceed and fail this script too.
set -ex

export BRANCH="ac-update"
export GITHUB_USER="MickeyMoz"
export EMAIL="sebastian@mozilla.com"
export REPO="fenix"

git config --global user.email "$EMAIL"
git config --global user.name "$GITHUB_USER"

COMPONENT_TO_WATCH='browser-engine-gecko'
MAVEN_URL="https://nightly.maven.mozilla.org/maven2/org/mozilla/components/$COMPONENT_TO_WATCH"

# Fetch latest version
LATEST_VERSION=$(curl "$MAVEN_URL/maven-metadata.xml" | sed -ne '/latest/{s/.*<latest>\(.*\)<\/latest>.*/\1/p;q;}')

# Check the latest version was uploaded by Mozilla
LATEST_POM_URL="$MAVEN_URL/$LATEST_VERSION/$COMPONENT_TO_WATCH-$LATEST_VERSION.pom"
POM_FILE='component.pom'
$CURL "$LATEST_POM_URL" --output "$POM_FILE"
$CURL "$LATEST_POM_URL.asc" --output "$POM_FILE.asc"
gpg --verify "$POM_FILE.asc" "$POM_FILE"

# Updating version file
sed -i "s/VERSION = \".*\"/VERSION = \"$LATEST_VERSION\"/g" "buildSrc/src/main/java/AndroidComponents.kt"

# Create a branch and commit local changes
git checkout -b "$BRANCH"
git add buildSrc/src/main/java/AndroidComponents.kt
git commit -m \
	"Update Android Components version to $LATEST_VERSION." \
	--author="MickeyMoz <sebastian@mozilla.com>" \
|| { echo "No new Android Components version ($LATEST_VERSTION) available"; exit 0; }


# From here on we do not want to print the commands since they contain tokens
set +x

export GITHUB_TOKEN=$(cat .github_token)

if [[ $GITHUB_TOKEN == 'faketoken' ]]; then
    echo '"faketoken" detected, not pushing anything'
    exit 0
fi

# Push changes to GitHub
echo "Pushing branch to GitHub"
URL="https://${GITHUB_USER}:${GITHUB_TOKEN}@github.com/$GITHUB_USER/$REPO/"
# XXX git sometimes leaks the URL including the token when the network request failed (regardless of --quiet).
git push --force --no-verify --quiet "$URL" "$BRANCH" > /dev/null 2>&1 || { echo "Failed ($?)"; exit 1; }

# Open a PR if needed
if [[ $(hub pr list --head "$GITHUB_USER:$BRANCH") ]]; then
    echo "There's already an open PR."
else
    echo "No PR found. Opening new PR."
    hub pull-request --base main --head "$GITHUB_USER:$BRANCH" --no-edit -m "Update Android Components version"
fi

unset GITHUB_TOKEN
