#!/bin/sh
#
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
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

## 
## Usage:
##
## $ mozilla-apprunner.sh [apprunner args]
##
## This script is meant to run the gecko apprunner from either 
## mozilla/xpfe/bootstrap or mozilla/dist/bin.
##
## The script will setup all the environment voodoo needed to make
## the mozilla-bin binary work.
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
    -h | --help )
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
		# keep this in synch with 
		# mozilla/xpfe/bootstrap/nsAppRunner.cpp  
		# and mozilla/profile/src/nsProfile.cpp
		-v | --version | -h | --help | -P | -SelectProfile | -CreateProfile | -ProfileManager | -ProfileWizard | -installer | -edit | -mail | -news | -pref | -compose | -editor | -addressbook | -chrome )
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
	
$dist_bin/run-mozilla.sh $script_args ./mozilla-bin $moreargs ${1+"$@"}
