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

# This script contains a number of variables, functions, etc which 
# are reused across a number of scripts. It should be included in each
# script as follows:
#
# TEST_DIR=${TEST_DIR:-/work/mozilla/mozilla.com/test.mozilla.com/www}
# TEST_BIN=${TEST_BIN:-$TEST_DIR/bin}
# source ${TEST_BIN}/library.sh
#

echo "calling $0 $@"

# export variables
set -a 

# error message
# output error message end exit 2

error()
{
    echo "error in script $SCRIPT: $1"
    if [[ "$0" == "-bash" || "$0" == "bash" ]]; then
        return 0
    fi
    exit 2
} 

# get_executable product branch directory
# writes path to product executable to stdout

get_executable()
{
    get_executable_product="$1"
    get_executable_branch="$2"
    get_executable_directory="$3"

    if [[ -z "$get_executable_product" || \
        -z "$get_executable_branch" || \
        -z "$get_executable_directory" ]]; then
        error "usage: get_executable product branch directory"
    elif [[ ! -d "$get_executable_directory" ]]; then
        error "get_executable: executable directory \"$get_executable_directory\" does not exist"
    else
        # should use /u+x,g+x,a+x but mac os x uses an obsolete find
        # filter the output to remove extraneous file in dist/bin for
        # cvs builds on mac os x.
        get_executable_name="$get_executable_product${EXE_EXT}"
        if [[ "$OSID" == "mac" ]]; then
            get_executable_filter="Contents/MacOS/$get_executable_product"
            if [[ "$get_executable_product" == "thunderbird" ]]; then
                get_executable_name="$get_executable_product-bin"
            fi
        else
            get_executable_filter="$get_executable_product"
        fi
        if find "$get_executable_directory" -perm +111 -type f \
            -name "$get_executable_name" | \
            grep "$get_executable_filter"; then
            true
        fi
    fi
}


TEST_DIR=${TEST_DIR:-/work/mozilla/mozilla.com/test.mozilla.com/www}
TEST_BIN=${TEST_BIN:-$TEST_DIR/bin}
TEST_HTTP=${TEST_HTTP:-test.mozilla.com}
TEST_STARTUP_TIMEOUT=${TEST_STARTUP_TIMEOUT:-30}

# set path to make life easier
PATH=${TEST_DIR}/bin:$PATH

# make pipelines return exit code of intermediate steps
# requires bash 3.x
set -o pipefail 

if [[ "$0" == "-bash" || "$0" == "bash" ]]; then
    SCRIPT="library.sh"
else
    SCRIPT=`basename $0`
fi

# save starting directory
STARTDIR=`pwd`

# change working directory to the location of the script.
SCRIPTDIR=`dirname $0`
cd $SCRIPTDIR

# set time format for pipeline timing reports
TIMEFORMAT="Elapsed time %0R seconds, User %0U seconds, System %0S seconds, CPU %P%%"

# don't attach to running instance
MOZ_NO_REMOTE=1

# don't restart
NO_EM_RESTART=1

# bypass profile manager
MOZ_BYPASS_PROFILE_AT_STARTUP=1

# ah crap handler timeout
MOZ_GDB_SLEEP=10

# no dialogs on asserts
XPCOM_DEBUG_BREAK=warn

#leak gauge
#NSPR_LOG_MODULES=DOMLeak:5,DocumentLeak:5,nsDocShellLeak:5

MALLOC_CHECK_=2

MACHINE=`uname -n`

ulimit -c 0

if echo $OSTYPE | grep -iq cygwin; then
    OSID=win32
    EXE_EXT=".exe"
elif echo $OSTYPE | grep -iq Linux; then
    OSID=linux
    EXE_EXT=
elif echo $OSTYPE | grep -iq darwin; then
    OSID=mac
    EXE_EXT=
else
    error "Unknown OS $OSTYPE"
fi


