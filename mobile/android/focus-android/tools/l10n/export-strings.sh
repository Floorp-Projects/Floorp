# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# If a command fails then do not proceed and fail this script too.
set -e

# Go to project root
cd "$(dirname "$0")"
cd ../..

# Checkout l10n repository or update already existing checkout
if [ ! -d "l10n-repo" ]; then
	git clone https://github.com/mozilla-l10n/focus-android-l10n.git l10n-repo
else
	cd l10n-repo
	git fetch origin
	cd ..
fi

# Reset the repo to the master state
cd l10n-repo
git reset --hard origin/master
git checkout master
git reset --hard origin/master

# Create a branch for the export
BRANCH="export-"`date +%Y-%m-%d`
git branch -D $BRANCH || echo ""
git checkout -b $BRANCH
cd ..

# Export strings and convert them from Android strings.xml files to po files
python tools/l10n/android2po/a2po.py export || echo "Could not export all locales"

# Create a separate commit for every locale.
tools/l10n/create_commits.sh

echo ""
echo "Please push and review branch $BRANCH in l10n-repo/"

