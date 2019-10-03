#!/bin/bash
set -x +e -v

# Store our starting dir so we can get back to it later
dir=$(pwd)

# Install scoop
powershell -Command "Set-ExecutionPolicy RemoteSigned -scope CurrentUser"
powershell -Command "iex (new-object net.webclient).downloadstring('https://get.scoop.sh')"
scoopstatus=$?

# Install some packages
scoop install git # Needed to update scoop and pick up newer packages
scoop install python # Worker only has 3.6.5 out of the box, we need 3.7.3+

# Enable some extra packages to be installed
scoop bucket add versions
scoop bucket add extras

# Update scoop and scoop manifests
scoop update

# `scoop update` seems to intermittently fail, add a retry attempt
if [ $scoopstatus -eq 0 ]
then
    echo "Scoop updated successfully"
else
    echo "scoop update failed, retrying"
    scoop update
fi

# Install the rest of the needed packages
scoop install which

# Install tesseract-ocr
cd $MOZ_FETCHES_DIR/iris_firefox
scoop install bootstrap\\tesseract.json

# Set up the pipenv
python3 -m pip install --upgrade pip
python3 -m pip install --upgrade pipenv
python3 -m pipenv install
pipstatus=$?

# If any part of the pipenv's install failed, try it again
if [ $pipstatus -eq 0 ]
then
    echo "Pipenv installed correctly, proceeding to Iris test run:"
else
    echo "Pipenv failed to install, attempting again:"
    python3 -m pipenv lock --clear
    python3 -m pipenv install
fi

# Handle the nightly smoketest suite differently
[ "$CURRENT_TEST_DIR" != "nightly" ] && irisstring="firefox -t $CURRENT_TEST_DIR" || irisstring="$CURRENT_TEST_DIR"
echo "$irisstring"

# Run the iris test suite
python3 -m pipenv run iris $irisstring -w ../../iris_runs -n --treeherder -f ../../fetches/firefox/firefox.exe -y
runstatus=$?

# Return to our starting dir and zip up the output of the test run
cd ../..
zip -r runs.zip iris_runs/runs

# Exit with the status from the iris run
exit $runstatus
