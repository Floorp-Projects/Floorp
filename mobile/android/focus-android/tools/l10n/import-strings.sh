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
	git reset --hard origin/master
	cd ..
fi

# Import and convert po files in L10N repository to Android strings.xml files
python tools/l10n/android2po/a2po.py import || echo "Could not import all locales"

# a2po creates wrong folder names for locales with country (e.g. values-es-MX). Rename
# those so that Android can find them (e.g. values-es-rMX)
tools/l10n/fix_locale_folders.sh

python tools/l10n/check_translations.py
