#!/bin/ksh

#
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "NPL"); you may not use this file except in
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

#
# tests.ksh
#	korn shell script for nspr tests
#

#
# Irrevelant tests
#
#bug1test 	- used to demonstrate a bug on NT
#dbmalloc	- obsolete; originally for testing debug version of nspr's malloc
#dbmalloc1	- obsolete; originally for testing debug version of nspr's malloc
#depend		- obsolete; used to test a initial spec for library dependencies
#dceemu		- used to tests special functions in NSPR for DCE emulation
#ipv6		- IPV6 not in use by NSPR clients
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

LOGFILE=${NSPR_TEST_LOGFILE:-"/dev/null"}

#
# Tests run on all platforms
#

TESTS="
acceptread
accept
alarm
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
fileio
foreign
fsync
getproto
i2l
initclk
inrval
instrumt
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
lltest
lock
lockfile
logger
many_cv
multiwait
nblayer
nonblock
op_2long
op_filnf
op_filok
op_nofil
parent
perf
pipeping
pipeself
poll_nm
poll_to
pollable
prftest
priotest
provider
ranfile
rwlocktest
sel_spd
selct_er
selct_nm
selct_to
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
xnotify"

OBJDIR=`basename $PWD`
echo "\nNSPR Test Results - $OBJDIR\n"
echo "BEGIN\t\t\t`date`"
echo "NSPR_TEST_LOGFILE\t${LOGFILE}\n"
echo "Test\t\t\tResult\n"
for prog in $TESTS
do
echo "$prog\c"
./$prog >> ${LOGFILE} 2>&1
if [ 0 = $? ] ; then
	echo "\t\t\tPassed";
else
	echo "\t\t\tFAILED";
fi;
done
echo "END\t\t\t`date`"
