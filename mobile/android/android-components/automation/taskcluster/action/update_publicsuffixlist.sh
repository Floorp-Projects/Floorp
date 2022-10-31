# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# If a command fails then do not proceed and fail this script too.
set -ex

# TODO: This script should only run if there's no PR open already.

# Setup git
git config --global user.email "sebastian@mozilla.com"
git config --global user.name "MickeyMoz"

# Use Gradle plugin to look for new GeckoView version (non major version update!)
./gradlew updatePSL

# Timestamp used in branch name and commit
TIMESTAMP=`date "+%Y%m%d-%H%M%S"`
BRANCH="PSL-$TIMESTAMP"

# Create a branch and commit local changes
git checkout -b $BRANCH
git add components/lib/publicsuffixlist/src/main/assets/publicsuffixes
git commit -m \
	"Update Public Suffix List ($TIMESTAMP)" \
	--author="MickeyMoz <sebastian@mozilla.com>" \
|| { echo "No new public suffix list available available"; exit 0; }

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
./gradlew openPR -Ptitle="Public Suffix List update ($TIMESTAMP)" -Pbranch="$BRANCH" -PtokenFile="token.properties"
