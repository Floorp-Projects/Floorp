#!/bin/bash

# *!*!*!*!*!*!*!*!*!*!**! NOTICE *!*!*!*!*!*!**!*!*!*!*!*!*!*!**!*!*!*!*!*!*!*!
# If you wish to change the location of the profile directory, then you will
# need to extensively edit this file.  For simplicity, the profile directory
# is hard coded to:
# Darwin/Linux:
# /tmp/profile
# Windows
# C:\tmp\profile
# Note that this value cannot be parameterized in the CreateProfile call, and
# this is why it was opted for a hard-coded path.  If that call can be
# parameterized, then the paths should be returned to a non-hardcoded path
# and getOsInfo can be modified to return a proper path that "CreateProfile"
# can accept (only accepts full, native paths: i.e. /Users/clint/tmp/profile
# not ~/tmp/profile or $Profile.

# Uncomment following line for debug output
# set -x

options="n:f:m:l:o:b:c:"

function usage()
{
    cat<<EOF
    usage:
    $script -n buildName -f FirefoxDir -m MinotaurDir -l locale -o outputXML -b bookmarks -c release-channel

variable             description
=============        ================================================
-n buildName         required, a name for Firefox build under test
-m MinotaurDir       required, the path to the directory where Minotaur is
-f firefoxDir        required, path to Firefox installed build
-l locale            required, locale identifier of this build
-o outputXML         optional, the output XML to compare against
-b bookmarks         optional, bookmarks.html to compare against
-c release-channel   optional, release-channel verification file

Notes
==========
* If you do not include the verification files, the test will run and export
information for you to use on future runs of this build and locale.
* minotaurdir/tests.manifest must also point to the containing directory
of the minotaur tests, with a trailing slash
* Output will be in ./<buildname>-<locale>/ relevant to the directory where
Minotaur was run from.

EOF
exit 2
}

while getopts $options optname ;
do
    case $optname in
        n) fxname=$OPTARG;;
        m) minotaurdir=$OPTARG;;
        f) fxdir=$OPTARG;;
        l) locale=$OPTARG;;
        o) outputVerify=$OPTARG;;
        b) bookmarksVerify=$OPTARG;;
        c) releaseVerify=$OPTARG;;
    esac
done

# If anything is not defined, display the usage string above
if [[ -z "$fxname" || -z "$fxdir" || -z "$locale" || -z "$minotaurdir" ]]
then
    usage
fi

# Get our OS and Executable name
OS=$(python getOsInfo.py)
FFX_EXE=$(python getOsInfo.py -o $OS -f)

if [ $OS = "Windows" ]; then
  baseProfile=c:/tmp
else
  baseProfile=/tmp
fi

rm -rf $baseProfile/test
rm -rf $baseProfile/profile
mkdir $baseProfile

cp ${minotaurdir}/tests.manifest $fxdir/chrome/.

#CreateProfile: NOTICE: See above
if [ $OS = "Windows" ]; then
  $fxdir/$FFX_EXE -CreateProfile 'minotaurTestProfile C:\tmp\profile'
else
  $fxdir/$FFX_EXE -CreateProfile 'minotaurTestProfile /tmp/profile'
fi

mkdir $baseProfile/profile
cp ${minotaurdir}/user.js $baseProfile/profile/.
$fxdir/$FFX_EXE -P minotaurTestProfile -chrome chrome://minotaur/content/quit.xul
sleep 10
cp ${minotaurdir}/user.js $baseProfile/profile/.

# Turn on http debugging
export NSPR_LOG_MODULES=nsHttp:5,nsSocketTransport:5,nsHostResolver:5
export NSPR_LOG_FILE=$baseProfile/http.log
rm $baseProfile/http.log

#run the tests
$fxdir/$FFX_EXE -P minotaurTestProfile -chrome chrome://minotaur/content

# create verification repository
mkdir ${minotaurdir}/$fxname-$locale
cp $baseProfile/profile/test-* ${minotaurdir}/$fxname-$locale/.
cp $baseProfile/http.log ${minotaurdir}/$fxname-$locale/http.log

# If verification files not given, then skip verification
if [[ -z "$outputVerify" || -z "$bookmarksVerify" || -z "$releaseVerify" ]]; then
  echo ----------- Verification Files Generated --------------
else
  # Do Verification
  #Prepare the log file
  rm $fxname-$locale/results.log

  #Perform the output.xml diff
  diff ${minotaurdir}/$fxname-$locale/test-output.xml $outputVerify >> ${minotaurdir}/$fxname-$locale/results.log

  # Check the Bookmarks file
  python diffBookmarks.py -l ${minotaurdir}/$fxname-$locale/test-bookmarks.html -r $bookmarksVerify -f ${minotaurdir}/$fxname-$locale/results.log

  # Check the Http Debug Log to catch the release channel
  python checkReleaseChannel.py -d ${minotaurdir}/$fxname-$locale/http.log -r $releaseVerify -l ${minotaurdir}/$fxname-$locale/results.log

  # Check to see if we fail or pass
  if [ -s ${minotaurdir}/$fxname-$locale/results.log ]; then
    echo !!!!!!!!!!!!! TEST FAILS !!!!!!!!!!!!!!
  else
    echo ------------- TEST PASSES -------------
  fi
fi

#CLEANUP
# Uncomment this line if you want to delete the firefox install directory
# rm -rdf $fxdir
rm -rf $baseProfile/profile
rm -rf $baseProfile/test
