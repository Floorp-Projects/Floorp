# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Corporation Code.
#
# The Initial Developer of the Original Code is
# Clint Talbert.
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Clint Talbert <ctalbert@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#!/bin/bash

# Uncomment following line for debug output
# set -x

options="n:f:m:l:o:b:c:v:"

function usage()
{
    cat<<EOF
    usage:
    $script -n buildName -f FirefoxDir -m MinotaurDir -l locale -o outputXML -b bookmarks -c release-channel -v FirefoxVersion

variable             description
=============        ================================================
-n buildName         required, a name for Firefox build under test
-m MinotaurDir       required, the path to the directory where Minotaur is
-f firefoxDir        required, path to Firefox installed build
-l locale            required, locale identifier of this build
-o outputXML         optional, the output XML to compare against
-b bookmarks         optional, bookmarks.html to compare against
-c release-channel   optional, release-channel verification file
-v firefox Version   required, the firefox version number under test

Notes
==========
* If you do not include the verification files, the test will run and export
information for you to use on future runs of this build and locale.
* minotaurdir/tests.manifest must also point to the containing directory
of the minotaur tests, with a trailing slash
* Output will be in ./<buildname>/<version>/<locale>/ relevant to the directory
where Minotaur was run from.
* Note that the test REQUIRES that you have write access to ~/ on your machine,
yes, that goes for Windows machines too.  Your shell should resolve ~/ somewhere
on your box.

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
        v) fxver=$OPTARG;;
    esac
done

# If anything is not defined, display the usage string above
if [[ -z "$fxname" || -z "$fxdir" || -z "$locale" || -z "$minotaurdir" || -z "$fxver" ]]
then
    usage
fi

# Get our OS and Executable and profile directory names
OS=$(python getOsInfo.py)
FFX_EXE=$(python getOsInfo.py -o $OS -f)
profileName=minotaur-"`uuidgen`"

cp ${minotaurdir}/tests.manifest $fxdir/chrome/tests.manifest

profileDir=`$fxdir/$FFX_EXE -CreateProfile $profileName 2>&1 | sed -e "s/.*at '\(.*\)[\/|\\]prefs\.js.*/\1/"`

# Seed that profile with our preferences
cp ${minotaurdir}/user.js "$profileDir"/user.js

$fxdir/$FFX_EXE -no-remote -P $profileName -chrome chrome://minotaur/content/quit.xul
sleep 10

# create verification repository
mkdir ${minotaurdir}/$fxname
mkdir ${minotaurdir}/$fxname/$fxver
mkdir ${minotaurdir}/$fxname/$fxver/$locale
resultsdir=${minotaurdir}/$fxname/$fxver/$locale

# Turn on http debugging - we now put the log file directly into the resultsdir
export NSPR_LOG_MODULES=nsHttp:5,nsSocketTransport:5,nsHostResolver:5
export NSPR_LOG_FILE=${resultsdir}/http.log

# Remove any existing log file
if [ -f $resultsdir/http.log ]; then
  rm $resultsdir/http.log
fi

#run the extraction
$fxdir/$FFX_EXE -P $profileName -chrome chrome://minotaur/content

cp ./test-output.xml $resultsdir/test-output.xml
cp ./test-bookmarks.html $resultsdir/test-bookmarks.html

# If verification files not given, then skip verification
if [[ -z "$outputVerify" || -z "$bookmarksVerify" || -z "$releaseVerify" ]]; then
  echo ----------- Verification Files Generated --------------
else
  # Do Verification
  #Prepare the log file
  rm $fxname/$fxver/$locale/results.log

  #Perform the output.xml diff
  diff -u $outputVerify $resultsdir/test-output.xml >> $resultsdir/results.log

  # Check the Bookmarks file
  python diffBookmarks.py -l $bookmarksVerify -r $resultsdir/test-bookmarks.html -f $resultsdir/results.log

  # Check the Http Debug Log to catch the release channel
  python checkReleaseChannel.py -d $resultsdir/http.log -r $releaseVerify -l $resultsdir/results.log

  # Check to see if we fail or pass
  if [ -s $resultsdir/results.log ]; then
    echo !!!!!!!!!!!!! TEST FAILS !!!!!!!!!!!!!!
    echo Results.log:
    cat $resultsdir/results.log
  else
    echo ------------- TEST PASSES -------------
  fi
fi

#CLEANUP
rm ./test-output.xml
rm ./test-bookmarks.html
rm $fxdir/chrome/tests.manifest
if [ -f ${minotaurdir}/EULA.txt ]; then
  rm ${minotaurdir}/EULA.txt
fi

# Delete the profile directory
python mozInstall.py -o d -d "$profileDir"
