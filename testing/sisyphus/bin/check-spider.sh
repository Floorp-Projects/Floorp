#!/usr/local/bin/bash -e
# -*- Mode: Shell-script; tab-width: 4; indent-tabs-mode: nil; -*-
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2006.
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Bob Clary <bob@bclary.com>
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

TEST_DIR=${TEST_DIR:-/work/mozilla/mozilla.com/test.mozilla.com/www}
TEST_BIN=${TEST_BIN:-$TEST_DIR/bin}
source ${TEST_BIN}/library.sh

#
# options processing
#
options="p:b:x:N:d:"
function usage()
{
    cat <<EOF
usage: 
$SCRIPT -p product -b branch -x executablepath -N profilename 
       [-d datafiles]

variable            description
===============     ============================================================
-p product          required. firefox|thunderbird
-b branch           required. 1.8.0|1.8.1|1.9.0
-x executablepath   required. directory-tree containing executable named 
                    'product'
-N profilename      required. name of profile to be used
-d datafiles        optional. one or more filenames of files containing 
                    environment variable definitions to be included.

note that the environment variables should have the same names as in the 
"variable" column.

Checks if the Spider extension is installed either in the named profile
or as a global extension, by attempting up to 3 times to launch the Spider.

If Spider fails to launch, the script returns exit code 66.

EOF
    exit 1
}

unset product branch executablepath profilename datafiles

while getopts $options optname ; 
  do 
  case $optname in
      p) product=$OPTARG;;
      b) branch=$OPTARG;;
      x) executablepath=$OPTARG;;
      N) profilename=$OPTARG;;
      d) datafiles=$OPTARG;;
  esac
done

# include environment variables
if [[ -n "$datafiles" ]]; then
    for datafile in $datafiles; do 
        cat $datafile | sed 's|^|data: |'
        source $datafile
    done
fi

if [[ -z "$product" || -z "$branch" || -z "$executablepath" || -z "$profilename" ]]; 
    then
    usage
fi

if [[ "$product" != "firefox" && "$product" != "thunderbird" ]]; then
    error "product \"$product\" must be one of firefox or thunderbird"
fi

if [[ "$branch" != "1.8.0" && "$branch" != "1.8.1" && "$branch" != "1.9.0" ]]; 
    then
    error "branch \"$branch\" must be one of 1.8.0, 1.8.1, 1.9.0"
fi

executable=`get_executable $product $branch $executablepath`

if [[ -z "$executable" ]]; then
    error "get_executable $product $branch $executablepath returned empty path"
fi

if [[ ! -x "$executable" ]]; then 
    error "executable \"$executable\" is not executable"
fi

if echo "$profilename" | egrep -qiv '[a-z0-9_]'; then
    error "profile name must consist of letters, digits or _"
fi

echo # attempt to force Spider to load

tries=1
while ! $TEST_BIN/timed_run.py ${TEST_STARTUP_TIMEOUT} "Start Spider: try $tries" \
    "$executable" -P "$profilename" \
    -spider -start -quit \
    -uri "http://${TEST_HTTP}/bin/start-spider.html" \
    -hook "http://${TEST_HTTP}/bin/userhook-checkspider.js"; do
  let tries=tries+1
  if [ "$tries" -gt 3 ]; then
      echo "Failed to start spider. Exiting..."
      exit 66
  fi
done

