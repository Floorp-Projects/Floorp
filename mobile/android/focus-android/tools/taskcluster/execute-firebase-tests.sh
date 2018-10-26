#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script uses the flank tool (https://github.com/TestArmada/flank)
# to shard UI test execution on Google Firebase so tests can run in parallel.  
#
# NOTE: 
#   Google Firebase does not currently allow renaming nor grouping of test
#   jobs.  recommendation: look at test failure summary in taskcluster
#   (not on Firebase dashboard)


# If a command fails then do not proceed and fail this script too.
set -e
#########################
# The command line help #
#########################
display_help() {
    echo "Usage: $0 device_type [x86 | arm] build_variant [webview | geckoview]" >&2
    echo
    echo "NOTE: All Build Variants are be of Debug type"
    echo
}

# Basic parameter check
if [ $# -lt 2 ]; then
    echo "Your command line contains $# arguments"
    display_help
    exit 1
fi

# From now on disable exiting on error. If the tests fail we want to continue
# and try to download the artifacts. We will exit with the actual error code later.
set +e


URL_FLANK_BIN=`curl -s https://api.github.com/repos/TestArmada/flank/releases/latest | grep "browser_download_url*" | cut -d '"' -f 4`
JAVA_BIN="/usr/bin/java"
WORKDIR="/opt/focus-android"
PATH_TEST="$WORKDIR/app/src/androidTest/java/org/mozilla/focus/activity"
PATH_TOOLS="$WORKDIR/tools/taskcluster"
PACKAGE="org.mozilla.focus.activity"
FLANK_BIN="$PATH_TOOLS/flank.jar"
FLANK_CONF="$PATH_TOOLS/flank.yml"

echo "home: $HOME"
export GOOGLE_APPLICATION_CREDENTIALS="$WORKDIR/.firebase_token.json"

if [ "$1" == "x86" ] || [ "$1" == "arm" ]; then
    if [ "$2" == "geckoview" ] || [ "$2" == "webview" ]; then
        FLANK_CONF_TEMPLATE="$PATH_TOOLS/flank-conf-${2}-${1}.yml"
    else
	echo "ERROR: $1 does not match [x86 | arm]"
        echo "or"
	echo "ERROR: $2 does not match [geckoview | webview]"
	exit 1
    fi
fi

rm  -f TEST_TARGETS 
rm  -f $FLANK_BIN

echo
echo "---------------------------------------"
echo "FLANK FILES"
echo "---------------------------------------"
echo "FLANK_CONF_TEMPLATE: $FLANK_CONF_TEMPLATE"
echo "FLANK_CONF: $FLANK_CONF"
echo "FLANK_BIN: $FLANK_BIN"
echo

wget $URL_FLANK_BIN -O $FLANK_BIN 
echo
echo "---------------------------------------"
echo "FLANK VERSION"
echo "---------------------------------------"
chmod +x $FLANK_BIN
$JAVA_BIN -jar $FLANK_BIN -v
echo
echo


# create TEST_TARGETS 
# (aka: get list of all tests in androidTest)
file_list=$(ls $PATH_TEST | xargs -n 1 basename)
array=( $file_list )

test_targets=""
for i in "${array[@]}"; do
    test_list=`cat $PATH_TEST/$i | grep -hr "public void" | grep 'Test\|test'| sed 's/ \{1,\}/ /g'  | cut -d" " -f 4 | sed 's/()//g'`
    array2=( $test_list)
    test_file=$(basename $i .java)
    
    for j in "${array2[@]}"; do
        echo "    - class $PACKAGE.$test_file#$j" >> TEST_TARGETS
    done
done

test_targets=`cat TEST_TARGETS`
test_targets=$(echo "${test_targets}" | sed '$!s~$~\\~g')

# create flank.yml config file 
sed "s/TEST_TARGETS/${test_targets}/g" $FLANK_CONF_TEMPLATE > $FLANK_CONF
rm -f TEST_TARGETS
$JAVA_BIN -jar $FLANK_BIN android run --config=$FLANK_CONF
exitcode=$?

cp -r "$WORKDIR/results" "$WORKDIR/test_artifacts"

# Now exit the script with the exit code from the test run. (Only 0 if all test executions passed)
exit $exitcode
