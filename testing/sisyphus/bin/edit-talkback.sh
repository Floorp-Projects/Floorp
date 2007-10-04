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

TEST_DIR=${TEST_DIR:-/work/mozilla/mozilla.com/test.mozilla.com/www}
TEST_BIN=${TEST_BIN:-$TEST_DIR/bin}
source ${TEST_BIN}/library.sh

#
# options processing
#
options="p:b:x:i:d:"
function usage()
{
    cat <<EOF
usage: 
$SCRIPT -p product -b branch -x executablepath -i talkbackid [-d datafiles]

variable            description
===============     ============================================================
-p product          required. firefox|thunderbird
-b branch           required. 1.8.0|1.8.1|1.9.0
-x executablepath   required. directory-tree containing executable named 
                    'product'
-i talkbackid       required. identifier to add to talkback url
-d datafiles        optional. one or more filenames of files containing 
                    environment variable definitions to be included.

note that the environment variables should have the same names as in the 
"variable" column.

EOF
    exit 1
}

unset product branch executablepath talkbackid datafiles

while getopts $options optname ; 
  do 
  case $optname in
      p) product=$OPTARG;;
      b) branch=$OPTARG;;
      x) executablepath=$OPTARG;;
      i) talkbackid=$OPTARG;;
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

if [[ -z "$product" || -z "$branch" || \
    -z "$executablepath" || -z "$talkbackid" ]]; then
    usage
fi

executable=`get_executable $product $branch $executablepath`

if [[ -z "$executable" ]]; then
    error "get_executable $product $branch $executablepath returned empty path"
fi

if [[ ! -x "$executable" ]]; then 
    error "executable \"$executable\" is not executable"
fi

executablepath=`dirname $executable`

# escape & in talkback id to prevent replacements
talkbackid=`echo $talkbackid | sed 's@&@\\\\&@g'`

#
# edit talkback to automatically submit
#
talkback=1

if [[ -e "$executablepath/extensions/talkback@mozilla.org/components/master.ini" ]]; then
    cd "$executablepath/extensions/talkback@mozilla.org/components/"
elif [[ -e "$executablepath/extensions/talkback@mozilla.org/components/talkback/master.ini" ]]; then
    cd "$executablepath/extensions/talkback@mozilla.org/components/talkback/"
elif [[ -e "$executablepath/components/master.ini" ]]; then
    cd "$executablepath/components"
else 
    # talkback not found
    talkback=0 
fi

if [[ $talkback -eq 1 ]]; then
    # edit to automatically send talkback incidents
    if [[ ! -e master.sed ]]; then
        #echo "$0: editing talkback master.ini in `pwd`"
        cp $TEST_BIN/master.sed .
        sed -f master.sed -i.bak master.ini
    fi

    case $OSID in
        win32)
            vendorid=`dos2unix < master.ini | grep '^VendorID = "' | sed 's@VendorID = "\([^"]*\)"@\1@'`
            productid=`dos2unix < master.ini | grep '^ProductID = "' | sed 's@ProductID = "\([^"]*\)"@\1@'`
            platformid=`dos2unix < master.ini | grep '^PlatformID = "' | sed 's@PlatformID = "\([^"]*\)"@\1@'`
            buildid=`dos2unix < master.ini | grep '^BuildID = "' | sed 's@BuildID = "\([^"]*\)"@\1@'`
            appdata=`cygpath -a -d "$APPDATA"`
            talkbackdir="`cygpath -a -u $appdata`/Talkback"
            ;;
        linux)
            vendorid=`dos2unix < master.ini | grep '^VendorID = "' | sed 's@VendorID = "\([^"]*\)"@\1@'`
            productid=`dos2unix < master.ini | grep '^ProductID = "' | sed 's@ProductID = "\([^"]*\)"@\1@'`
            platformid=`dos2unix < master.ini | grep '^PlatformID = "' | sed 's@PlatformID = "\([^"]*\)"@\1@'`
            buildid=`dos2unix < master.ini | grep '^BuildID = "' | sed 's@BuildID = "\([^"]*\)"@\1@'`
            talkbackdir="$HOME/.fullcircle"
            ;;
        mac)
            # hack around Mac's use of spaces in directory names
            vendorid=`grep '^VendorID = "' master.ini | sed 's@VendorID = "\([^"]*\)"@\1@'`
            productid=`grep '^ProductID = "' master.ini | sed 's@ProductID = "\([^"]*\)"@\1@'`
            platformid=`grep '^PlatformID = "' master.ini | sed 's@PlatformID = "\([^"]*\)"@\1@'`
            buildid=`grep '^BuildID = "' master.ini | sed 's@BuildID = "\([^"]*\)"@\1@'`
            talkbackdir="$HOME/Library/Application Support/FullCircle"
            IFS=:
            ;;
        *)
            error "unknown os $OSID"
            ;;
    esac

    if [[ -z "$talkbackdir" ]]; then
        error "empty talkback directory"
    fi

    mkdir -p "$talkbackdir"
    
    case $OSID in
        win32)
            talkbackinidir="$talkbackdir/$vendorid/$productid/$platformid/$buildid"
            ;;
        linux | mac )
            talkbackinidir="$talkbackdir/$vendorid$productid$platformid$buildid"
            ;;
    esac
    
    if [[ ! -d "$talkbackinidir" ]]; then
        create-directory.sh -d "$talkbackinidir" -n
    fi

    cd $talkbackinidir

    cp /work/mozilla/mozilla.com/test.mozilla.com/www/talkback/$OSID/Talkback.ini .

    case "$OSID" in
        win32)
            sed -i.bak "s@URLEdit .*@URLEdit = \"mozqa:$talkbackid\"@" Talkback.ini
            ;;
        linux )
            sed -i.bak "s@URLEditControl .*@URLEditControl = \"mozqa:$talkbackid\"@" Talkback.ini
            ;;
        mac )
            sed -i.bak "s@URLEditControl .*@URLEditControl = \"mozqa:$talkbackid\"@" Talkback.ini
            ;;
        *)
            error "unknown os=$OSID"
    esac
fi
