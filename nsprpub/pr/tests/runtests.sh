#!/bin/sh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if test -z $1
then
  echo "usage: $0 <path-to-dist>"
  exit 1
fi

cd $1/lib
ABS_LIB=$PWD
cd -

export DYLD_LIBRARY_PATH=${ABS_LIB}:${DYLD_LIBRARY_PATH}
export LD_LIBRARY_PATH=${ABS_LIB}:${LD_LIBRARY_PATH}
export PATH=${ABS_LIB}:${PATH}

#
# runtests.sh
#	Bourne shell script for nspr tests
#

SYSTEM_INFO=`uname -a`
OS_ARCH=`uname -s`

if [ $OS_ARCH = "Windows_NT" ] || [ $OS_ARCH = "OS/2" ]
then
	NULL_DEVICE=nul
else
	NULL_DEVICE=/dev/null
	FILE_D=`ulimit -n`
	if [ $FILE_D -lt 512 ]
	then
		ulimit -n 512
	fi
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
#multiwait - fails on Linux 64bit since NSPR v 4.4 from 2004.
#nbconn - fails on some platforms 
#poll_er - fails on some platforms? limited use?
#prpoll -  the bad-FD test needs to be moved to a different test
#sleep	-  specific to OS/2
#
# all of the following were disabled in 2019 when reenabling CI tests,
# because they failed on at least one of the platforms:
#
# cltsrv
# cvar
# gethost
# getproto
# layer
# logfile
# nameshm1
# nblayer
# nonblock
# ntioto
# op_2long
# parent
# provider
# ranfile
# socket
# sockopt
# vercheck

#LOGFILE=${NSPR_TEST_LOGFILE:-$NULL_DEVICE}
LOGFILE=nspr-test.log

#
# Tests run on all platforms
#

TESTS="
abstract
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
concur
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
lazyinit
libfilename
lltest
lock
lockfile
logger
many_cv
ntoh
op_excl
op_filnf
op_filok
op_nofil
parsetm
peek
perf
pipeping
pipeping2
pipeself
poll_nm
poll_to
pollable
prftest
prfz
primblok
prpollml
pushtop
randseed
reinit
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
printf "\nNSPR Test Results - $OBJDIR\n\n"
printf "BEGIN\t\t\t`date`\n"
printf "NSPR_TEST_LOGFILE\t${LOGFILE}\n\n"
printf "Test\t\t\tResult\n\n"
if [ $OS_PLATFORM = "Windows_95" ] || [ $OS_PLATFORM = "Windows_98" ] || [ $OS_PLATFORM = "Windows_NT" ] || [ $OS_PLATFORM = "OS/2" ] ; then
	for prog in $TESTS
	do
		printf "$prog (`date +%T`)"
		printf "\nBEGIN TEST: $prog\n\n" >> ${LOGFILE} 2>&1
		./$prog >> ${LOGFILE} 2>&1
		if [ 0 = $? ] ; then
			printf "\t\t\tPassed\n";
		else
			printf "\t\t\tFAILED\n";
			rval=1
		fi;
		printf "\nEND TEST: $prog\n\n" >> ${LOGFILE} 2>&1
	done
else
	for prog in $TESTS
	do
		printf "$prog (`date +%T`)"
		printf "\nBEGIN TEST: $prog\n\n" >> ${LOGFILE} 2>&1
		export test_rval
		./$prog >> ${LOGFILE} 2>&1 &
		test_pid=$!
		sleep_pid=0
		if test -n "$TEST_TIMEOUT" && test "$TEST_TIMEOUT" -gt 0
		then
		(sleep  $TEST_TIMEOUT; kill $test_pid >/dev/null 2>&1 ) &
		sleep_pid=$!
		fi
		wait $test_pid
		test_rval=$?
		[ $sleep_pid -eq 0 ] || kill $sleep_pid >/dev/null 2>&1
		if [ 0 = $test_rval ] ; then
			printf "\t\t\tPassed\n";
		else
			printf "\t\t\tFAILED\n";
			rval=1
		fi;
		printf "\nEND TEST: $prog\n\n" >> ${LOGFILE} 2>&1
	done
fi;

if [ $rval -ne 0 ]; then
  cat ${LOGFILE}
fi

printf "END\t\t\t`date`\n"
exit $rval

