#!/bin/sh
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#

## 
## Usage:
##
## $ mozilla-apprunner.sh [apprunner args]
##
## This script is meant to run the gecko apprunner from either 
## mozilla/xpfe/bootstrap or mozilla/dist/bin.
##
## The script will setup all the environment voodoo needed to make
## the apprunner work.
##

#uncomment for debugging
#set -x

dist_bin=""

# Running from dist/bin
if [ -d components -a -d res ]
then
	dist_bin="./"
else
	# Running from source dir
	if [ -f Makefile.in ]
	then
		dist_bin=`grep -w DEPTH Makefile.in  | grep -e "\.\." | awk -F"=" '{ print $2; }'`/dist/bin
	fi
fi

script_args=""
moreargs=""
debugging=0

while [ $# -gt 0 ]
do
  case $1 in
    -h | --help)
      script_args="$script_args -h"
      shift
      ;;
    -g | --debug)
      script_args="$script_args -g"
      debugging=1
      shift
      ;;
    -d | --debugger)
      script_args="$script_args -d $2"
      shift 2
      ;;
    -*)
	case $1 in
		-ProfileManager | -ProfileWizard | -installer | -edit | -mail | -news | -pref | -compose | -editor | -addressbook | -chrome )
		if [ "x$moreargs" != "x" ]
		then
			echo "You can't have $1 and $moreargs"
			exit 1
		else
			moreargs=$1
		fi
		shift
		;;
	*)
		echo "Unknown option: $1" 1>&2
		exit 1
		;;
	esac
      ;;
    *)
      break
      ;;
 esac
done

# if you are debugging, you can't have moreargs
# for example, "./mozilla-apprunner.sh -g -mail" makes no sense.
if [ $debugging -eq 1 -a "x$moreargs" != "x" ]
then
	echo "You can't have -g and $moreargs at the same time"
	exit 1
fi
	
$dist_bin/run-mozilla.sh $script_args ./apprunner $moreargs ${1+"$@"}
