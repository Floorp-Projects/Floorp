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

dist_bin=""

# Running from dist/bin
if [ -d components -a -d res ]
then
  dist_bin="./"
else
  # Running from source dir
  if [ -f Makefile.in ]
  then
    depth=`perl -ne '/^DEPTH\s*=(.*)/ and print $1' Makefile.in`
    dist_bin=$depth/dist/bin
  fi
fi

script_args=""

while [ $# -gt 0 ]
do
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
    -*)
      echo "Unknown option: $1" 1>&2
      exit 1
      ;;
    *)
      break
      ;;
 esac
done

$dist_bin/run-mozilla.sh $script_args ./viewer ${1+"$@"}
