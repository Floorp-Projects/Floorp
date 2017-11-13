# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script imports the latest strings, creates a commit, pushes
# it to the bot's repository and creates a pull request.

# If a command fails then do not proceed and fail this script too.
set -e

# Go to project root
cd "$(dirname "$0")"
cd ../..

# Import strings from L10N repository
tools/l10n/import-strings.sh > import-log.txt
cat import-log.txt

# Timestamp used in branch name and commit
TIMESTAMP=`date "+%Y%m%d-%H%M%S"`

# Create a branch and commit local changes
git checkout -b $TIMESTAMP
git add app/src/main/res/
git commit -m \
	"Import translations from L10N repository ($TIMESTAMP)" \
	--author="MickeyMoz <sebastian@mozilla.com>" \
	|| (echo "No new translations available" && exit 0)

# Create a pull request for the current state of the repo
python tools/taskcluster/create-pull-request.py $TIMESTAMP
