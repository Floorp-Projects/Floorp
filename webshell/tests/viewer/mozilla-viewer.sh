#!/bin/sh
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#

## 
## Usage:
##
## $ mozilla-viewer.sh [viewer args]
##
## This script is meant to run the gecko viewer from either 
## mozilla/webshell/tests/viewer or mozilla/dist/bin.
##
## The script will setup all the environment voodoo needed to make
## the viewer work.
##

dist_bin=`dirname $0`
script_args=""
moreargs=""
MOZILLA_BIN="viewer"
pass_all_args=0

while [ $# -gt 0 ]
do
  if [ $pass_all_args -ne 0 ]
  then
    moreargs="$moreargs \"$1\""
    shift
  else
    case $1 in
      -h | --help)
        script_args="$script_args -h"
        shift
        ;;
      -g | --debug)
        script_args="$script_args -g"
        shift
        ;;
      -d | --debugger)
        script_args="$script_args -d $2"
        shift 2
        ;;
      --)
        shift
        pass_all_args=1
        ;;
      *)
        moreargs="$moreargs \"$1\""
        shift 1
        ;;
    esac
  fi
done

eval "set -- $moreargs"
echo $dist_bin/run-mozilla.sh $script_args $dist_bin/$MOZILLA_BIN "$@"
$dist_bin/run-mozilla.sh $script_args $dist_bin/$MOZILLA_BIN "$@"
