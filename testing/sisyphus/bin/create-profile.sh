#!/bin/bash -e
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

source $TEST_DIR/bin/library.sh

#
# options processing
#
options="p:b:x:D:N:L:U:d:"
function usage()
{
    cat <<EOF
usage:
$SCRIPT -p product -b branch -x executablepath -D directory -N profilename
       [-L profiletemplate] [-U user] [-d datafiles]

variable            description
===============     ============================================================
-p product          required. firefox.
-b branch           required. supported branch. see library.sh
-x executablepath   required. directory-tree containing executable 'product'
-D directory        required. directory where profile is to be created.
-N profilename      required. profile name
-L profiletemplate  optional. location of a template profile to be used.
-U user             optional. user.js preferences file.
-d datafiles        optional. one or more filenames of files containing
                    environment variable definitions to be included.

note that the environment variables should have the same names as in the
"variable" column.

EOF
    exit 1
}

unset product branch executablepath directory profilename profiletemplate user datafiles

while getopts $options optname ;
  do
  case $optname in
      p) product=$OPTARG;;
      b) branch=$OPTARG;;
      x) executablepath=$OPTARG;;
      D) directory=$OPTARG;;
      N) profilename=$OPTARG;;
      L) profiletemplate=$OPTARG;;
      U) user=$OPTARG;;
      d) datafiles=$OPTARG;;
  esac
done

# include environment variables
loaddata $datafiles

if [[ -z "$product" || -z "$branch" || -z "$executablepath" || \
    -z "$directory" || -z "$profilename" ]]; then
    usage
fi

checkProductBranch $product $branch

executable=`get_executable $product $branch $executablepath`

$TEST_DIR/bin/create-directory.sh -d "$directory" -n

if echo "$profilename" | egrep -qiv '[a-z0-9_]'; then
    error "profile name \"$profilename\" must consist of letters, digits or _" $LINENO
fi

if [ $OSID == "nt" ]; then
    directoryospath=`cygpath -a -w $directory`
    if [[ -z "$directoryospath" ]]; then
        error "unable to convert unix path to windows path" $LINENO
    fi
else
    directoryospath="$directory"
fi

echo "creating profile $profilename in directory $directory"

tries=1
while ! $TEST_DIR/bin/timed_run.py ${TEST_STARTUP_TIMEOUT} "-" \
        $EXECUTABLE_DRIVER \
        $executable -CreateProfile "$profilename $directoryospath"; do
    let tries=tries+1
    if [ "$tries" -gt $TEST_STARTUP_TRIES ]; then
        error "Failed to create profile $directory Exiting..." $LINENO
    fi
    sleep 30
done

if [[ -n $profiletemplate ]]; then
    if [[ ! -d $profiletemplate ]]; then
        error "profile template directory $profiletemplate does not exist" $LINENO
    fi
    echo "copying template profile $profiletemplate to $directory"
    cp -R $profiletemplate/* $directory
fi

if [[ ! -z $user ]]; then
    cp $user $directory/user.js
fi
