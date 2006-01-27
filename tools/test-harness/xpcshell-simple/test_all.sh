#!/bin/sh
#
# vim:set ts=2 sw=2 sts=2 et:
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
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Darin Fisher <darin@meer.net>
#  Dave Liebreich <davel@mozilla.com>
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
# ***** END LICENSE BLOCK ***** */

# allow core dumps
ulimit -c 20480 2> /dev/null

# MOZILLA_FIVE_HOME is exported by run-mozilla.sh, and is usually $(DIST)/bin
# we need to know that dir path in order to find xpcshell
bin=${MOZILLA_FIVE_HOME:-`dirname $0`}

# convert bin to absolute path, so it will work after the cd to the test dir
bin=`cd $bin; pwd`

exit_status=0

# The sample Makefile for the xpcshell-simple harness adds the directory
# where the *.js files reside as an arg.  If no arg is specified, assume 
# the current directory is where the *.js files live.
if [ "x$1" != "x" ]; then
    cd $1
fi

headfiles="-f $bin/test-harness/xpcshell-simple/head.js"

# files matching the pattern head_*.js are treated like test setup files
# - they are run after head.js but before the test file
for h in head_*.js
do
    if [ -f $h ]; then
	headfiles="$headfiles -f $h"
    fi
done

tailfiles="-f $bin/test-harness/xpcshell-simple/tail.js"
# files matching the pattern tail_*.js are treated like teardown files
# - they are run after tail.js
for t in tail_*.js
do
    if [ -f $t ]; then
	tailfiles="$tailfiles -f $t"
    fi
done

for t in test_*.js
do
    echo -n "$t: "
    $bin/xpcshell $headfiles -f $t $tailfiles 2> $t.log 1>&2
    if [ `grep -c '\*\*\* PASS' $t.log` = 0 ]
    then
        echo "FAIL (see $t.log)"
        exit_status=1
    else
        echo "PASS"
    fi
done

exit $exit_status
