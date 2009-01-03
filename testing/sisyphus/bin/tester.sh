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

TEST_LOG=/dev/null

#
# options processing
#
options="p:b:e:T:t:v"
function usage()
{
    cat<<EOF
usage: 
$SCRIPT -t testscript [-v ] datalist1 [datalist2 [datalist3 [datalist4]]]

variable            description
===============     ===========================================================
-t testscript       required. quoted test script with required arguments.
-v                  optional. verbose - copies log file output to stdout.

executes the testscript using the input data files in 
$TEST_DIR/data constructed from each combination of the input parameters:

{item1},{item2},{item3},{item4}

EOF
    exit 1
}

unset testscript testargs

# remove script name from args
shiftargs=1

while getopts $options optname ; 
  do 
  case $optname in
      t) 
          let shiftargs=$shiftargs+1
          testscript="$OPTARG"
          if echo $testscript | grep -iq ' ' ; then
              testargs=`echo $testscript   | sed 's|^\([^ ]*\)[ ]*\(.*\)|\2|'`
              testscript=`echo $testscript | sed 's|^\([^ ]*\)[ ]*.*|\1|'`
          fi
          ;;
      v) verbose=1
          let shiftargs=$shiftargs+1
          ;;
  esac
done

if [[ -z "$testscript" ]]; then
    usage
fi

shift $shiftargs

datalist=`combo.sh "$@"`

TEST_SUITE=`dirname $testscript | sed "s|$TEST_DIR/||" | sed "s|/|_|g"`

for data in $datalist; do
    TEST_DATE=`date -u +%Y-%m-%d-%H-%M-%S``date +%z`
    TEST_LOG="${TEST_DIR}/results/${TEST_DATE},$data,$OSID,${TEST_MACHINE},$TEST_SUITE.log"

    # tell caller what the log files are
    echo "log: $TEST_LOG "

    if [[ "$verbose" == "1" ]]; then
        test-setup.sh -d $TEST_DIR/data/$data.data 2>&1 | tee -a $TEST_LOG
        $testscript $testargs -d $TEST_DIR/data/$data.data 2>&1 | tee -a $TEST_LOG
    else
        test-setup.sh -d $TEST_DIR/data/$data.data >> $TEST_LOG 2>&1
        $testscript $testargs -d $TEST_DIR/data/$data.data >> $TEST_LOG 2>&1
    fi

    if [[ "$XPCOM_DEBUG_BREAK" == "stack" ]]; then
        case $OSID in
            nt)
                ;;
            linux)
                if findprogram fix-linux-stack.pl; then
                    fix-linux-stack.pl < $TEST_LOG > $TEST_LOG.tmp
                    mv $TEST_LOG.tmp $TEST_LOG
                else
                    error "XPCOM_DEBUG_BREAK=stack specified but fix-linux-stack.pl is not available"
                fi
                ;;
            darwin)
                if findprogram fix-macosx-stack.pl; then
                    fix-macosx-stack.pl < $TEST_LOG > $TEST_LOG.tmp
                    mv $TEST_LOG.tmp $TEST_LOG
                else
                    error "XPCOM_DEBUG_BREAK=stack specified but fix-macosx-stack.pl is not available"
                fi
                ;;
        esac
    fi

done
