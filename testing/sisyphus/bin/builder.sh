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

TEST_LOG=/dev/null

trap 'echo -e "\n*** ERROR ***\n\b" && tail $TEST_LOG' ERR

#
# options processing
#
options="p:b:B:T:e:d:v"
function usage()
{
    cat<<EOF
usage: 
$SCRIPT -p products -b branches -B buildcommands -T buildtypes [-e extra] [-v]

variable            description
===============     ===========================================================
-p products         required. one or more of firefox thunderbird
-b branches         required. one or more of 1.8.0 1.8.1 1.9.0
-B buildcommands    required. one or more of clean checkout build
-T buildtypes       required. one or more of opt debug
-e extra            optional. extra qualifier to pick build tree and mozconfig.
-d datafiles        optional. one or more filenames of files containing 
                    environment variable definitions to be included.
-v                  optional. verbose - copies log file output to stdout.

note that the environment variables should have the same names as in the 
"variable" column.

EOF
    exit 1
}

unset products branches buildcommands buildtypes extra extraflag datafiles

while getopts $options optname ; 
  do 
  case $optname in
      p) products="$OPTARG";;
      b) branches="$OPTARG";;
      B) buildcommands="$OPTARG";;
      T) buildtypes="$OPTARG";;
      e) extra="-$OPTARG"
          extraflag="-e $OPTARG";;
      d) datafiles=$OPTARG;;
      v) verbose=1;;
  esac
done

# include environment variables
if [[ -n "$datafiles" ]]; then
    for datafile in $datafiles; do 
        cat $datafile | sed 's|^|data: |'
        source $datafile
    done
fi

if [[ -z "$products" || -z "$branches" || -z "$buildcommands" || \
    -z "$buildtypes" ]]; then
    usage
fi

# clean first in case checkout changes the configuration
if echo "$buildcommands" | grep -iq clean; then
    for product in $products; do
        for branch in $branches; do
            for buildtype in $buildtypes; do

                TEST_DATE=`date -u +%Y-%m-%d-%H-%M-%S``date +%z`
                TEST_LOG="${TEST_DIR}/results/${TEST_DATE},$product,$branch$extra,$buildtype,$OSID,${TEST_MACHINE},clean.log"

                echo "log: $TEST_LOG"

                if [[ "$verbose" == "1" ]]; then
                    clean.sh -p $product -b $branch -T $buildtype $extraflag 2>&1 | tee $TEST_LOG
                else
                    clean.sh -p $product -b $branch -T $buildtype $extraflag > $TEST_LOG 2>&1
                fi
            done
        done
    done
fi

# if checkout, ignore buildtypes
if echo "$buildcommands" | grep -iq checkout; then
    for product in $products; do
        for branch in $branches; do

            TEST_DATE=`date -u +%Y-%m-%d-%H-%M-%S``date +%z`
            TEST_LOG="${TEST_DIR}/results/${TEST_DATE},$product,$branch$extra,$buildtype,$OSID,${TEST_MACHINE},checkout.log"

            echo "log: $TEST_LOG"

            if [[ "$verbose" == "1" ]]; then
                checkout.sh -p $product -b $branch -T opt $extraflag 2>&1 | tee $TEST_LOG
            else
                checkout.sh -p $product -b $branch -T opt $extraflag > $TEST_LOG 2>&1
            fi

        done
    done
fi

if echo "$buildcommands" | grep -iq build; then
    for product in $products; do
        for branch in $branches; do
            for buildtype in $buildtypes; do

                TEST_DATE=`date -u +%Y-%m-%d-%H-%M-%S``date +%z`
                TEST_LOG="${TEST_DIR}/results/${TEST_DATE},$product,$branch$extra,$buildtype,$OSID,${TEST_MACHINE},build.log"

                echo "log: $TEST_LOG"

                if [[ "$verbose" == "1" ]]; then
                    build.sh -p $product -b $branch -T $buildtype $extraflag 2>&1 | tee $TEST_LOG
                else
                    build.sh -p $product -b $branch -T $buildtype $extraflag > $TEST_LOG 2>&1
                fi

            done
        done
    done
fi

