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
options="p:b:u:f:c:B:T:x:N:D:L:U:E:d:"
function usage()
{
    cat<<EOF
usage: 
$SCRIPT -p product -b branch
       [-u url [-f filepath] [-c credentials]] 
       [-B buildcommands -T buildtype] 
       [-x executablepath]
       [-N profilename [-D profiledirectory [-L profiletemplate 
         [-U userpreferences]]]]
       [-E extensiondir]
       [-d datafiles] 

variable            description
===============     ===========================================================
-p product          required. one of firefox thunderbird
-b branch           required. one of 1.8.0 1.8.1 1.9.0
-u url              optional. url where to download build
-f filepath         optional. location to save downloaded build or to find
                    previously downloaded build. If not specified, the
                    default will be the basename of the url saved to the
                    /tmp directory. If there is no basename, then the
                    filepath will be /tmp/\$product-\$branch-file.
-B buildcommands    optional. one or more of clean checkout build
-T buildtype        optional. one of opt debug
-x executablepath   optional. directory tree containing executable with same 
                    name as product. If the build is downloaded and executable 
                    path is not specified, it will be defaulted to 
                    /tmp/\$product-\$branch. 
                    For cvs builds it will be defaulted to the appropriate 
                    directory in 
                    /work/mozilla/builds/\$branch/mozilla/\$product-\$buildtype/
-N profilename      optional. profilename. profilename is required if 
                    profiledirectory or extensiondir are specified.
-D profiledirectory optional. If profiledirectory is specified, a new profile 
                    will be created in the directory.
-L profiletemplate  optional. If a new profile is created, profiletemplate is 
                    the path to an existing profile which will be copied over 
                    the new profile.
-U userpreferences  optional. If a new profile is created, userpreferences is 
                    the path to a user.js file to be copied into the new 
                    profile.
                    If userpreferences is not specified when a new profile is 
                    created, it is defaulted to
                    /work/mozilla/mozilla.com/test.mozilla.com/www/prefs/test-user.js
-E extensiondir     optional. path to directory tree containing extensions to 
                    be installed.
-d datafiles        optional. one or more filenames of files containing 
                    environment variable definitions to be included.

note that the environment variables should have the same 
names as in the "variable" column.

EOF
    exit 1
}

unset product branch url filepath credentials buildcommands buildtype executablepath profilename profiledirectory profiletemplate userpreferences extenstiondir datafiles

while getopts $options optname ; 
  do 
  case $optname in
      p) product="$OPTARG";;
      b) branch="$OPTARG";;

      u) url="$OPTARG";;
      f) filepath="$OPTARG";;
      c) credentials="$OPTARG";;

      B) buildcommands="$OPTARG";;
      T) buildtype="$OPTARG";;

      x) executablepath="$OPTARG";;

      N) profilename="$OPTARG";;
      D) profiledirectory="$OPTARG";;
      L) profiletemplate="$OPTARG";;
      U) userpreferences="$OPTARG";;

      E) extensiondir="$OPTARG";;

      d) datafiles="$OPTARG";;
  esac
done

# include environment variables
if [[ -n "$datafiles" ]]; then
    for datafile in $datafiles; do 
        cat $datafile | sed 's|^|data: |'
        source $datafile
    done
fi

TEST_PRODUCT=$product
TEST_BRANCH=$branch
TEST_BUILDCOMMANDS=$buildcommands
TEST_BUILDTYPE=$buildtype
TEST_EXECUTABLEPATH=$executablepath
TEST_PROFILENAME=$profilename
TEST_PROFILETEMPLATE=$profiletemplate
TEST_USERPREFERENCES=$userpreferences
TEST_EXTENSIONDIR=$extensiondir
TEST_DATAFILES=$datafiles

dumpenvironment

if [[ -z "$product" || -z "$branch" ]]; then
    echo "product and branch are required"
    usage
fi

if [[ ( -n "$url" || -n "$filepath" ) && ( -n "$buildcommands" ) ]]; then
    echo "you can not both download and build cvs builds at the same time"
    usage
fi

if [[ -n "$buildcommands" && -n "$executablepath" ]]; then
    echo "You can not specify the executable path and build cvs builds at the same time"
    usage
fi

if [[ (-n "$profiledirectory" || -n "$extensiondir" ) && -z "$profilename" ]]; then
    echo "You must specify a profilename if you specify a profiledirectory or extensiondir"
    usage
fi

# if the url is specified but not the filepath
# generate a default path where to save the 
# downloaded build.
if [[ -n "$url" && -z "$filepath" ]]; then
    filepath=`basename $url`
    if [[ -z "$filepath" ]]; then
        filepath="$product-$branch-file"
    fi
    filepath="/tmp/$filepath"
fi

if [[ -n "$url" ]]; then
    download.sh -u "$url" -c "$credentials" -f "$filepath" -t "$TEST_DOWNLOAD_TIMEOUT"
fi

# install the build at the specified filepath
if [[ -n "$filepath" ]]; then
    if [[ -z "$executablepath" ]]; then
        executablepath="/tmp/$product-$branch"
    fi
    install-build.sh -p $product -b $branch -x $executablepath -f $filepath
fi

if [[ -n "$buildcommands" ]]; then

    if [[ -z "$buildtype" ]]; then
        echo "You must specify a buildtype if you are building from cvs"
        usage
    elif [[ "$buildtype" != "opt" && "$buildtype" != "debug" ]]; then
        echo "buildtype must be one of opt debug"
        usage
    fi

    case "$OSID" in
        mac)
            if [[ "$product" == "firefox" ]]; then
                App=Firefox
            elif [[ "$product" == "thunderbird" ]]; then
                App=Thunderbird
            fi
            if [[ "$buildtype" == "debug" ]]; then
                AppType=Debug
            fi
            executablepath="/work/mozilla/builds/$branch/mozilla/$product-$buildtype/dist/$App$AppType.app/Contents/MacOS"
            ;;
        *)
            executablepath="/work/mozilla/builds/$branch/mozilla/$product/$buildtype/dist/bin"
    esac

    if echo "$buildcommands" | grep -iq clean; then
        clean.sh -p $product -b $branch -t $buildtype
    fi

    if echo "$buildcommands" | grep -iq checkout; then
        checkout.sh -p $product -b $branch -t $buildtype
    fi

    if echo "$buildcommands" | grep -iq build; then
        build.sh -p $product -b $branch -t $buildtype
    fi

fi

if [[ -n "$profiledirectory" ]]; then

    if [[ -z "$userpreferences" ]]; then
        userpreferences=/work/mozilla/mozilla.com/test.mozilla.com/www/prefs/test-user.js
    fi

    unset optargs
    if [[ -n "$profiletemplate" ]]; then
        optargs="$optargs -L $profiletemplate"
    fi
    if [[ -n "$userpreferences" ]]; then
        optargs="$optargs -U $userpreferences"
    fi

    create-profile.sh -p $product -b $branch \
        -x $executablepath -D $profiledirectory -N $profilename \
        $optargs
fi

if [[ -n "$extensiondir" ]]; then

    install-extensions.sh -p $product -b $branch \
        -x $executablepath -N $profilename -E $extensiondir

    check-spider.sh -p $product -b $branch \
        -x $executablepath -N $profilename

fi




