# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# If a command fails then do not proceed and fail this script too.
set -ex

# TODO: This script should only run if there's no PR open already.

# Setup git
git config --global user.email "sebastian@mozilla.com"
git config --global user.name "MickeyMoz"

# Generate docs and copy to destination
./gradlew clean docs
rm -rf docs/api
mkdir docs/api
cp -R build/javadoc/android-components/* docs/api/

# Timestamp used in branch name and commit
TIMESTAMP=`date "+%Y%m%d-%H%M%S"`
BRANCH="DOCS-$TIMESTAMP"

# Create a branch and commit local changes
git checkout -b $BRANCH
git add docs/api
git commit -m \
	"Update docs ($TIMESTAMP)" \
	--author="MickeyMoz <sebastian@mozilla.com>" \
|| { echo "No changes to commit"; exit 0; }

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
./gradlew openPR -Ptitle="Docs update ($TIMESTAMP) [ci skip]" -Pbranch="$BRANCH" -PtokenFile="token.properties"
