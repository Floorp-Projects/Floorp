#!/bin/bash
# Debian10 linux bootstrap
set -x +e -v

# Set up a virtual display since we don't have an xdisplay
. $HOME/scripts/xvfb.sh
start_xvfb '1920x1080x24+32' 0

# Re-set `+e` after start_xvfb changes it
set +e

# Install iris's pipenv
cd $MOZ_FETCHES_DIR/iris_firefox
PIPENV_MAX_RETRIES="5" pipenv install
status=$?

# If pipenv installation fails for any reason, make another attempt.
if [ $status -eq 0 ]
then
    echo "Pipenv installed correctly, proceeding to Iris test run:"
else
    echo "Pipenv failed to install, attempting again:"
    pipenv lock --clear # This purges any partially/incorrectly generated lock files
    pipenv install
fi

# Handle the nightly smoketest suite differently
[ "$CURRENT_TEST_DIR" != "nightly" ] && irisstring="firefox -t $CURRENT_TEST_DIR" || irisstring="$CURRENT_TEST_DIR"
echo "$irisstring"

# Actually run the iris tests
pipenv run iris $irisstring -w ../../iris_runs -n --treeherder -f ../../fetches/firefox/firefox -y
status=$?

# Zip up the test run output
cd ../..
zip -r runs.zip iris_runs/runs

# Exit with iris's exit code so treeherder knows if tests failed
exit $status
