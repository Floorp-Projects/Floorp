File: pr/tests/readme

This document describes the test cases in the NSPR directory
pr/tests.

=====================================================================
There are some sub-directories here:

dll
	sources for the .dll(.so) used by test dlltest.c

macbuild
	MacIntosh project files

server
	an empty directory. Does anyone remember why?

w16gui
	Sources for a modified version of the poppad application from
	Charles Petzold's book "Programming Windows 3.1". These
	sources were modified to test the library lib/ds/PLEvent.
	These files are obsolete and will not be maintained.
	
	This test was superceded by lib/tests/windows/winevent.c and
	lib/event.c and is now owned by CPD.

=====================================================================
The individual files are described here.

The script 'runtests.ksh' enumerates and runs test cases that are
expected to run on all platforms.


accept.c
	Tests PR_Accept() and related socket functions.

acceptread.c
	Tests PR_AcceptRead()

alarm.c
	Tests alarm functions declared in obsolete/pralarm.h.
	The alarm functions are obsolete, so is this test.

atomic.c
	Tests Atomic operations defined in pratom.h

attach.c
	Test PR_AttachThread()
	Note: This is an NSPR private function.

bigfile.c
	Test 64bit file offset functions declared in prio.h

bug1test.c
	Demonstrates a bug on NT.

cleanup.c
	Tests PR_Cleanup() declared in prinit.h

cltsrv.c
	Tests many socket functions.

concur.c
	Tests threading functions and concurrent operations.

cvar.c
	Tests condition variables.

cvar2.c
	Tests condition variables. A rather abusive test.

dbmalloc.c
	Obsolete. Originally for testing debug builds of NSPR's malloc.

dbmalloc1.c
	Obsolete. Originally for testing debug builds of NSPR's malloc.

dceemu.c
	Tests special functions for DCE emulation.

depend.c
	Obsoltet. Tests early spec for library dependency.

dlltest.c
	Tests dynamic library fucntions. Used with dll/my.c

dtoa.c
	Tests conversions of double to string.

exit.c
	Tests PR_ProcessExit() declared in prinit.h

fileio.c
	Tests NSPR semaphores a bit of file i/o and threading
	functions.

foreign.c
	Test auto-attach of a thread created by something other than
	NSPR.

forktest.c
	Limited use. Tests unix fork() and related functions.

fsync.c
	Tests use of PR_Sync() declared in prio.h

getproto.c
	Tests socket functions PR_GetProtoByName(), etc.

i2l.c
	Tests LongLong functions for converting 32bit integer to 64bit
	integer.

initclk.c
	Tests timing on minimal use of condition variable

inrval.c
	Tests interval timing functions.

instrumt.c
	Tests instrumentation functions. prcountr.h prtrace.h

intrupt.c
	Tests PR_Interrupt()

ioconthr.c
	Tests i/o continuation mechanism in pthreads.

io_timeout.c
	Test socket i/o timeouts.

io_timeoutk.c
	Obsolete. Subsumed in io_timeout.c

io_timeoutu.c
	Obsolete. Subsumed in io_timeout.c

ipv6.c
	Tests IPv6. IPv6 is not used by NSPR clients.

join.c
	Tests PR_JoinThread()

joinkk.c
	Tests PR_JoinThread()

joinku.c
	Tests PR_JoinThread()

joinuk.c
	Tests PR_JoinThread()

joinuu.c
	Tests PR_JoinThread()

layer.c
	Tests layered I/O.

lazyinit.c
	Tests implicit initialization.

lltest.c
	Tests LongLong (64bit integer) arithmentic and conversions.

lock.c
	Tests PR_Lock() in heavily threaded environment.

lockfile.c
	Test PR_Lockfile().

logger.c
	Tests PR_LOG()

makefile
	The makefile that builds all the tests

many_cv.c
	Tests aquiring a large number of condition variables.

multiwait.c
	???

nbconn.c
	Test non-blocking connect.

nblayer.c
	Tests NSPR's layered I/O capability.

nonblock.c
	Tests operations on non-blocking socket.

op_2long.c
	Tests PR_Open() where filename is too long.

op_filnf.c
	Tests PR_Open() where filename is NotFound.

op_filok.c
	Tests PR_Open() where filename is accessable.

op_noacc.c
	Tests PR_Open() where file permissions are wrong.
	Limited use. Windows has no concept of Unix style file permissions.

op_nofil.c
	Tests PR_Open() where filename does not exist.

parent.c
	Test parent/child process capability

perf.c
	Tests and measures context switch times for various thread
	syncronization functions.

pipeping.c
	Tests inter-process pipes. Run with pipepong.c

pipepong.c
	Tests inter-process pipes. Run with pipeping.c

pipeself.c
	Tests inter-thread pipes.

pollable.c
	Tests pollable events. prio.h

poll_er.c
	Tests PR_Poll() where an error is expected.

poll_nm.c
	Tests PR_Poll() where normal operation is expected.

poll_to.c
	Tests PR_Poll() where timeout is expected.

prftest.c
	Tests printf-like formatting.

prftest1.c
	Obsolete. Subsumed in prftest.c

prftest2.c
	Obsolete. Subsumed in prftest.c

priotest.c
	Limited use. Tests NSPR thread dispatching priority.

provider.c

prpoll.c
	Tests PR_Poll().

prselect.c
	Obsolete. PR_Select() is obsolete.

prttools.h
	Unused file.

ranfile.c
	Tests random file access.

readme
	This file.

runtests.ksh
	A korn shell script that runs a set of tests that should run
	on any of the NSPR supported platforms.

runtests.pl
	A perl script to run the test cases. This srcipt runs tests
	common to all platforms and runs tests applicable to specific
	platforms. Uses file runtests.txt to control execution.

runtests.txt
	Control file for perl script: runtests.pl

rwlocktest.c
	Tests Reader/Writer lock

selct_er.c
	Obsolete. PR_Select() is obsolete.

selct_nm.c
	Obsolete. PR_Select() is obsolete.

selct_to.c
	Obsolete. PR_Select() is obsolete.

select2.c
	Obsolete. PR_Select() is obsolete.

sel_spd.c
	Obsolete. PR_Select() is obsolete.

sem.c
	Obsolete. Semaphores are not supported.

server_test.c
	Tests sockets by simulating a server in loopback mode.
	Makes its own client threads.

servr_kk.c
	Tests client/server sockets, threads using system threads.

servr_ku.c
	Tests client/server sockets, threads using system and user threads.

servr_uk.c
	Tests client/server sockets, threads using system and user threads.

servr_uu.c
	Tests client/server sockets, threads user threads.

short_thread.c
	Tests short-running threads. Useful for testing for race conditions.

sigpipe.c
	Tests NSPR's SIGPIPE handler. Unix only.

sleep.c
	Limited use. Tests sleep capability of platform.

socket.c
	Tests many socket functions.

sockopt.c
	Tests setting and getting socket options.

sprintf.c
	Tests sprintf.

sproc_ch.c
	Obsolete. Tests IRIX sproc-based threads.

sproc_p.c
	Obsolete. Tests IRIX sproc-based threads.

stack.c
	Test atomic stack operations.

stat.c
	Tests performance of getfileinfo() vs. stat()

stdio.c
	Tests NSPR's handling of stdin, stdout, stderr.

strod.c
	Tests formatting of double precision floating point.

suspend.c
	Private interfaces PR_SuspendAll(), PR_ResumeAll(), etc.

switch.c
	Tests thread switching

system.c
	Tests PR_GetSystemInfo()

testbit.c
	Tests bit arrays.

testfile.c
	Tests many file I/O functions.

threads.c
	Tests thread caching.

thruput.c
	Tests socket thruput. Must be run by hand as client/server.
	Does not self terminate.

time.c
	Incomplete. Limited use.

timemac.c
	Test time and date functions. Originally for Mac.

timetest.c
	Tests time conversion over a wide range of dates.

tmoacc.c
	Server to tmocon.c and writev.c
	Do not run it by itself.

tmocon.c
	Client thread to tmoacc.c

tpd.c
	Tests thread private data.

udpsrv.c
	Tests UDP socket functions.

ut_ttools.h
	unused file.

version.c
	Extract and print library version data.

vercheck.c
	Test PR_VersionCheck().

writev.c
	Tests gather-write on a socket. Requires tmoacc.c

xnotify.c
	Tests cached monitors.

yield.c
	Limited use

y2k.c
	Test to verify NSPR's date functions as Y2K compliant.

dll\Makefile
	makefile for mygetval.c, mysetval.c

dll\mygetval.c
	Dynamic library test. See also dlltest.c

dll\mysetval.c
	Dynamic library test. See also dlltest.c

w16gui\Makefile
	Obsolete. Tests for lib/ds/PLEvent on Windows 3.1.
w16gui\popfile.c
w16gui\popfind.c
w16gui\popfont.c
w16gui\poppad.c
w16gui\poppad.h
w16gui\poppad.ico
w16gui\poppad.rc
w16gui\popprnt0.c
w16gui\readme.1st
