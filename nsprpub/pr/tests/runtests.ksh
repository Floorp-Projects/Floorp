#!/bin/ksh
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
# The Original Code is the Netscape Portable Runtime (NSPR).
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

#
# tests.ksh
#	korn shell script for nspr tests
#

SYSTEM_INFO=`uname -a`
OS_ARCH=`uname -s`
if [ $OS_ARCH = "Windows_NT" ] || [ $OS_ARCH = "OS/2" ]
then
	NULL_DEVICE=nul
else
	NULL_DEVICE=/dev/null
fi

#
# Irrevelant tests
#
#bug1test 	- used to demonstrate a bug on NT
#bigfile2   - requires 4Gig file creation. See BugZilla #5451
#bigfile3   - requires 4Gig file creation. See BugZilla #5451
#dbmalloc	- obsolete; originally for testing debug version of nspr's malloc
#dbmalloc1	- obsolete; originally for testing debug version of nspr's malloc
#depend		- obsolete; used to test a initial spec for library dependencies
#dceemu		- used to tests special functions in NSPR for DCE emulation
#ipv6		- IPV6 not in use by NSPR clients
#mbcs       - tests use of multi-byte charset for filenames. See BugZilla #25140
#sproc_ch	- obsolete; sproc-based tests for Irix
#sproc_p	- obsolete; sproc-based tests for Irix
#io_timeoutk - obsolete; subsumed in io_timeout
#io_timeoutu - obsolete; subsumed in io_timeout
#prftest1	- obsolete; subsumed by prftest
#prftest2	- obsolete; subsumed by prftest
#prselect	- obsolete; PR_Select is obsolete
#select2	- obsolete; PR_Select is obsolete
#sem		- obsolete; PRSemaphore is obsolete
#stat		- for OS2?
#suspend	- private interfaces PR_SuspendAll, PR_ResumeAll, etc..
#thruput	- needs to be run manually as client/server
#time		- used to measure time with native calls and nspr calls
#tmoacc		- should be run with tmocon
#tmocon		- should be run with tmoacc
#op_noacc	- limited use
#yield		- limited use for PR_Yield

#
# Tests not run (but should)
#

#forktest (failed on IRIX)
#nbconn - fails on some platforms 
#poll_er - fails on some platforms? limited use?
#prpoll -  the bad-FD test needs to be moved to a different test
#sleep	-  specific to OS/2

LOGFILE=${NSPR_TEST_LOGFILE:-$NULL_DEVICE}

#
# Tests run on all platforms
#

TESTS="
accept
acceptread
acceptreademu
affinity
alarm
anonfm
atomic
attach
bigfile
cleanup
cltsrv
concur
cvar
cvar2
dlltest
dtoa
errcodes
exit
fdcach
fileio
foreign
formattm
fsync
gethost
getproto
i2l
initclk
inrval
instrumt
intrio
intrupt
io_timeout
ioconthr
join
joinkk
joinku
joinuk
joinuu
layer
lazyinit
libfilename
lltest
lock
lockfile
logger
many_cv
multiwait
nameshm1
nblayer
nonblock
ntioto
ntoh
op_2long
op_excl
op_filnf
op_filok
op_nofil
parent
peek
perf
pipeping
pipeping2
pipeself
poll_nm
poll_to
pollable
prftest
primblok
provider
prpollml
ranfile
randseed
rwlocktest
sel_spd
selct_er
selct_nm
selct_to
selintr
sema
semaerr
semaping
sendzlf
server_test
servr_kk
servr_uk
servr_ku
servr_uu
short_thread
sigpipe
socket
sockopt
sockping
sprintf
stack
stdio
str2addr
strod
switch
system
testbit
testfile
threads
timemac
timetest
tpd
udpsrv
vercheck
version
writev
xnotify
zerolen"

rval=0


#
# When set, value of the environment variable TEST_TIMEOUT is the maximum
# time (secs) allowed for a test program beyond which it is terminated.
# If TEST_TIMEOUT is not set or if it's value is 0, then test programs
# don't timeout.
#
# Running runtests.ksh under MKS toolkit on NT, 95, 98 does not cause
# timeout detection correctly. For these platforms, do not attempt timeout
# test. (lth).
#
#

OS_PLATFORM=`uname`
OBJDIR=`basename $PWD`
echo "\nNSPR Test Results - $OBJDIR\n"
echo "BEGIN\t\t\t`date`"
echo "NSPR_TEST_LOGFILE\t${LOGFILE}\n"
echo "Test\t\t\tResult\n"
if [ $OS_PLATFORM = "Windows_95" ] || [ $OS_PLATFORM = "Windows_98" ] || [ $OS_PLATFORM = "Windows_NT" ] || [ $OS_PLATFORM = "OS/2" ] ; then
	for prog in $TESTS
	do
		echo "$prog\c"
		echo "\nBEGIN TEST: $prog\n" >> ${LOGFILE} 2>&1
		./$prog >> ${LOGFILE} 2>&1
		if [ 0 = $? ] ; then
			echo "\t\t\tPassed";
		else
			echo "\t\t\tFAILED";
			rval=1
		fi;
		echo "\nEND TEST: $prog\n" >> ${LOGFILE} 2>&1
	done
else
	for prog in $TESTS
	do
		echo "$prog\c"
		echo "\nBEGIN TEST: $prog\n" >> ${LOGFILE} 2>&1
		export test_rval
		./$prog >> ${LOGFILE} 2>&1 &
		test_pid=$!
		sleep_pid=0
		if [ "$TEST_TIMEOUT" -gt 0 ]
		then
		(sleep  $TEST_TIMEOUT; kill $test_pid >/dev/null 2>&1 ) &
		sleep_pid=$!
		fi
		wait $test_pid
		test_rval=$?
		[ $sleep_pid -eq 0 ] || kill $sleep_pid >/dev/null 2>&1
		if [ 0 = $test_rval ] ; then
			echo "\t\t\tPassed";
		else
			echo "\t\t\tFAILED";
			rval=1
		fi;
		echo "\nEND TEST: $prog\n" >> ${LOGFILE} 2>&1
	done
fi;

echo "END\t\t\t`date`"
exit $rval

















