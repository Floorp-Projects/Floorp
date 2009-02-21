#!/usr/bin/python -u
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
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s): Chris Cooper
#                 Jesse Ruderman
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

# Usage: timed_run timeout prefix command args
import os, signal, sys, time

#
# returns exit code as follows:
# 
exitOSError   = 66
exitSignal    = 77
exitTimeout   = 88
exitInterrupt = 99

pid = None
prefix = sys.argv[2]
elapsedtime = 0

if prefix == "-":
    prefix = ''
else:
    prefix = prefix + ':'

def getSignalName(num):
    for p in dir(signal):
        if p.startswith("SIG") and not p.startswith("SIG_"):
            if getattr(signal, p) == num:
                return p
    return "UNKNOWN"

def alarm_handler(signum, frame):
    global pid
    global prefix
    try:
	stoptime = time.time()
	elapsedtime = stoptime - starttime
        print "\n%s EXIT STATUS: TIMED OUT (%s seconds)\n" % (prefix, elapsedtime)
        flushkill(pid, signal.SIGKILL)
    except OSError, e:
        print "\ntimed_run.py: exception trying to kill process: %d (%s)\n" % (e.errno, e.strerror)
        pass
    flushexit(exitTimeout)

def forkexec(command, args):
    global prefix
    global elapsedtime
    #print command
    #print args
    try:
        pid = os.fork()
        if pid == 0:  # Child
            os.execvp(command, args)
            flushbuffers()
        else:  # Parent
            return pid
    except OSError, e:
        print "\n%s ERROR: %s %s failed: %d (%s) (%f seconds)\n" % (prefix, command, args, e.errno, e.strerror, elapsedtime)
        flushexit(exitOSError)

def flushbuffers():
        sys.stdout.flush()
        sys.stderr.flush()

def flushexit(rc):
        flushbuffers()
        sys.exit(rc)

def flushkill(pid, sig):
        flushbuffers()
        os.kill(pid, sig)

signal.signal(signal.SIGALRM, alarm_handler)
signal.alarm(int(sys.argv[1]))
starttime = time.time()
try:
	pid = forkexec(sys.argv[3], sys.argv[3:])
	status = os.waitpid(pid, 0)[1]
	signal.alarm(0) # Cancel the alarm
	stoptime = time.time()
	elapsedtime = stoptime - starttime
	# it appears that linux at least will on "occasion" return a status
	# when the process was terminated by a signal, so test signal first.
	if os.WIFSIGNALED(status):
            signum = os.WTERMSIG(status)
            if signum == 2:
                msg = 'INTERRUPT'
                rc = exitInterrupt
            else:
                msg = 'CRASHED'
                rc = exitSignal

            print "\n%s EXIT STATUS: %s signal %d %s (%f seconds)\n" % (prefix, msg, signum, getSignalName(signum), elapsedtime)
            flushexit(rc)

	elif os.WIFEXITED(status):
	    rc = os.WEXITSTATUS(status)
	    msg = ''
	    if rc == 0:
	        msg = 'NORMAL'
	    else:
	        msg = 'ABNORMAL ' + str(rc)
		rc = exitSignal

	    print "\n%s EXIT STATUS: %s (%f seconds)\n" % (prefix, msg, elapsedtime)
	    flushexit(rc)
	else:
	    print "\n%s EXIT STATUS: NONE (%f seconds)\n" % (prefix, elapsedtime)
	    flushexit(0)
except KeyboardInterrupt:
	flushkill(pid, 9)
	flushexit(exitInterrupt)

# check that the child process has terminated.
try:
    os.getpgid(pid)
    # process still exists. try to kill it and exit with OSError
    flushkill(pid, 9)
    flushexit(exitOSError)
except OSError:
    # process doesn't exist. all is well.
    1
