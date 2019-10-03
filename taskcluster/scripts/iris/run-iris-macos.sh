#!/bin/bash
set -x +e -v

cd $MOZ_FETCHES_DIR/iris_firefox

# FIXME: Install the following without homebrew:
# tesseract
# p7zip
# xquartz
# pipenv
# pyobjc
# pyobjc-core

# FIXME: Find a way to set these values:
# https://github.com/mozilla/iris_firefox/blob/master/bootstrap/osx_bootstrap.sh#L87-L91

# FIXME: Maybe find a way to create these download files:
# https://github.com/mozilla/iris_firefox/blob/master/bootstrap/osx_bootstrap.sh#L93-L104

# FIXME: Ensure all of the necessary python packages are available in the pypi mirror

# Mount the downloaded Firefox and install iris's pipenv
hdiutil attach ../target.dmg
python3 -m ensurepip --upgrade
pipenv install

# Handle the nightly smoketest suite differently
[ "$CURRENT_TEST_DIR" != "nightly" ] && irisstring="firefox -t $CURRENT_TEST_DIR" || irisstring="$CURRENT_TEST_DIR"
echo "$irisstring"

# Run this chunk of iris tests
pipenv run iris $irisstring -w ../../iris_runs -f /Volumes/Firefox\ Nightly/Firefox\ Nightly.app/Contents/MacOS/firefox-bin -n --treeherder -y
runstatus=$?

# FIXME: Return to the starting dir (../..) and zip up the iris_runs/runs dir

# Exit with the iris test run's exit code
exit $runstatus
