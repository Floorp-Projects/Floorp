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

$dist_bin/run-mozilla.sh apprunner ${1+"$@"}
