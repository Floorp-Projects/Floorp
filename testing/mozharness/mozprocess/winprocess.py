# A module to expose various thread/process/job related structures and
# methods from kernel32
#
# The MIT License
#
# Copyright (c) 2003-2004 by Peter Astrand <astrand@lysator.liu.se>
#
# Additions and modifications written by Benjamin Smedberg
# <benjamin@smedbergs.us> are Copyright (c) 2006 by the Mozilla Foundation
# <http://www.mozilla.org/>
#
# More Modifications
# Copyright (c) 2006-2007 by Mike Taylor <bear@code-bear.com>
# Copyright (c) 2007-2008 by Mikeal Rogers <mikeal@mozilla.com>
#
# By obtaining, using, and/or copying this software and/or its
# associated documentation, you agree that you have read, understood,
# and will comply with the following terms and conditions:
#
# Permission to use, copy, modify, and distribute this software and
# its associated documentation for any purpose and without fee is
# hereby granted, provided that the above copyright notice appears in
# all copies, and that both that copyright notice and this permission
# notice appear in supporting documentation, and that the name of the
# author not be used in advertising or publicity pertaining to
# distribution of the software without specific, written prior
# permission.
#
# THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
# INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
# OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

from __future__ import absolute_import, unicode_literals

import sys
import subprocess

from ctypes import c_void_p, POINTER, sizeof, Structure, windll, WinError, WINFUNCTYPE, c_ulong
from ctypes.wintypes import BOOL, BYTE, DWORD, HANDLE, LPCWSTR, LPWSTR, UINT, WORD
from .qijo import QueryInformationJobObject

LPVOID = c_void_p
LPBYTE = POINTER(BYTE)
LPDWORD = POINTER(DWORD)
LPBOOL = POINTER(BOOL)
LPULONG = POINTER(c_ulong)


def ErrCheckBool(result, func, args):
    """errcheck function for Windows functions that return a BOOL True
    on success"""
    if not result:
        raise WinError()
    return args


# AutoHANDLE

class AutoHANDLE(HANDLE):
    """Subclass of HANDLE which will call CloseHandle() on deletion."""

    CloseHandleProto = WINFUNCTYPE(BOOL, HANDLE)
    CloseHandle = CloseHandleProto(("CloseHandle", windll.kernel32))
    CloseHandle.errcheck = ErrCheckBool

    def Close(self):
        if self.value and self.value != HANDLE(-1).value:
            self.CloseHandle(self)
            self.value = 0

    def __del__(self):
        self.Close()

    def __int__(self):
        return self.value


def ErrCheckHandle(result, func, args):
    """errcheck function for Windows functions that return a HANDLE."""
    if not result:
        raise WinError()
    return AutoHANDLE(result)

# PROCESS_INFORMATION structure


class PROCESS_INFORMATION(Structure):
    _fields_ = [("hProcess", HANDLE),
                ("hThread", HANDLE),
                ("dwProcessID", DWORD),
                ("dwThreadID", DWORD)]

    def __init__(self):
        Structure.__init__(self)

        self.cb = sizeof(self)


LPPROCESS_INFORMATION = POINTER(PROCESS_INFORMATION)

# STARTUPINFO structure


class STARTUPINFO(Structure):
    _fields_ = [("cb", DWORD),
                ("lpReserved", LPWSTR),
                ("lpDesktop", LPWSTR),
                ("lpTitle", LPWSTR),
                ("dwX", DWORD),
                ("dwY", DWORD),
                ("dwXSize", DWORD),
                ("dwYSize", DWORD),
                ("dwXCountChars", DWORD),
                ("dwYCountChars", DWORD),
                ("dwFillAttribute", DWORD),
                ("dwFlags", DWORD),
                ("wShowWindow", WORD),
                ("cbReserved2", WORD),
                ("lpReserved2", LPBYTE),
                ("hStdInput", HANDLE),
                ("hStdOutput", HANDLE),
                ("hStdError", HANDLE)
                ]


LPSTARTUPINFO = POINTER(STARTUPINFO)

SW_HIDE = 0

STARTF_USESHOWWINDOW = 0x01
STARTF_USESIZE = 0x02
STARTF_USEPOSITION = 0x04
STARTF_USECOUNTCHARS = 0x08
STARTF_USEFILLATTRIBUTE = 0x10
STARTF_RUNFULLSCREEN = 0x20
STARTF_FORCEONFEEDBACK = 0x40
STARTF_FORCEOFFFEEDBACK = 0x80
STARTF_USESTDHANDLES = 0x100

# EnvironmentBlock


class EnvironmentBlock:
    """An object which can be passed as the lpEnv parameter of CreateProcess.
    It is initialized with a dictionary."""

    def __init__(self, env):
        if not env:
            self._as_parameter_ = None
        else:
            values = []
            fs_encoding = sys.getfilesystemencoding() or 'mbcs'
            for k, v in env.iteritems():
                if isinstance(k, bytes):
                    k = k.decode(fs_encoding, 'replace')
                if isinstance(v, bytes):
                    v = v.decode(fs_encoding, 'replace')
                values.append("{}={}".format(k, v))
            values.append("")
            self._as_parameter_ = LPCWSTR("\0".join(values))


# Error Messages we need to watch for go here
# See: http://msdn.microsoft.com/en-us/library/ms681388%28v=vs.85%29.aspx
ERROR_ABANDONED_WAIT_0 = 735

# GetLastError()
GetLastErrorProto = WINFUNCTYPE(DWORD)  # Return Type
GetLastErrorFlags = ()
GetLastError = GetLastErrorProto(("GetLastError", windll.kernel32), GetLastErrorFlags)

# CreateProcess()

CreateProcessProto = WINFUNCTYPE(BOOL,                  # Return type
                                 LPCWSTR,               # lpApplicationName
                                 LPWSTR,                # lpCommandLine
                                 LPVOID,                # lpProcessAttributes
                                 LPVOID,                # lpThreadAttributes
                                 BOOL,                  # bInheritHandles
                                 DWORD,                 # dwCreationFlags
                                 LPVOID,                # lpEnvironment
                                 LPCWSTR,               # lpCurrentDirectory
                                 LPSTARTUPINFO,         # lpStartupInfo
                                 LPPROCESS_INFORMATION  # lpProcessInformation
                                 )

CreateProcessFlags = ((1, "lpApplicationName", None),
                      (1, "lpCommandLine"),
                      (1, "lpProcessAttributes", None),
                      (1, "lpThreadAttributes", None),
                      (1, "bInheritHandles", True),
                      (1, "dwCreationFlags", 0),
                      (1, "lpEnvironment", None),
                      (1, "lpCurrentDirectory", None),
                      (1, "lpStartupInfo"),
                      (2, "lpProcessInformation"))


def ErrCheckCreateProcess(result, func, args):
    ErrCheckBool(result, func, args)
    # return a tuple (hProcess, hThread, dwProcessID, dwThreadID)
    pi = args[9]
    return AutoHANDLE(pi.hProcess), AutoHANDLE(pi.hThread), pi.dwProcessID, pi.dwThreadID


CreateProcess = CreateProcessProto(("CreateProcessW", windll.kernel32),
                                   CreateProcessFlags)
CreateProcess.errcheck = ErrCheckCreateProcess

# flags for CreateProcess
CREATE_BREAKAWAY_FROM_JOB = 0x01000000
CREATE_DEFAULT_ERROR_MODE = 0x04000000
CREATE_NEW_CONSOLE = 0x00000010
CREATE_NEW_PROCESS_GROUP = 0x00000200
CREATE_NO_WINDOW = 0x08000000
CREATE_SUSPENDED = 0x00000004
CREATE_UNICODE_ENVIRONMENT = 0x00000400

# Flags for IOCompletion ports (some of these would probably be defined if
# we used the win32 extensions for python, but we don't want to do that if we
# can help it.
INVALID_HANDLE_VALUE = HANDLE(-1)  # From winbase.h

# Self Defined Constants for IOPort <--> Job Object communication
COMPKEY_TERMINATE = c_ulong(0)
COMPKEY_JOBOBJECT = c_ulong(1)

# flags for job limit information
# see http://msdn.microsoft.com/en-us/library/ms684147%28VS.85%29.aspx
JOB_OBJECT_LIMIT_BREAKAWAY_OK = 0x00000800
JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK = 0x00001000

# Flags for Job Object Completion Port Message IDs from winnt.h
# See also: http://msdn.microsoft.com/en-us/library/ms684141%28v=vs.85%29.aspx
JOB_OBJECT_MSG_END_OF_JOB_TIME = 1
JOB_OBJECT_MSG_END_OF_PROCESS_TIME = 2
JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT = 3
JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO = 4
JOB_OBJECT_MSG_NEW_PROCESS = 6
JOB_OBJECT_MSG_EXIT_PROCESS = 7
JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS = 8
JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT = 9
JOB_OBJECT_MSG_JOB_MEMORY_LIMIT = 10

# See winbase.h
DEBUG_ONLY_THIS_PROCESS = 0x00000002
DEBUG_PROCESS = 0x00000001
DETACHED_PROCESS = 0x00000008

# GetQueuedCompletionPortStatus -
# http://msdn.microsoft.com/en-us/library/aa364986%28v=vs.85%29.aspx
GetQueuedCompletionStatusProto = WINFUNCTYPE(BOOL,         # Return Type
                                             HANDLE,       # Completion Port
                                             LPDWORD,      # Msg ID
                                             LPULONG,      # Completion Key
                                             # PID Returned from the call (may be null)
                                             LPULONG,
                                             DWORD)        # milliseconds to wait
GetQueuedCompletionStatusFlags = ((1, "CompletionPort", INVALID_HANDLE_VALUE),
                                  (1, "lpNumberOfBytes", None),
                                  (1, "lpCompletionKey", None),
                                  (1, "lpPID", None),
                                  (1, "dwMilliseconds", 0))
GetQueuedCompletionStatus = GetQueuedCompletionStatusProto(("GetQueuedCompletionStatus",
                                                            windll.kernel32),
                                                           GetQueuedCompletionStatusFlags)

# CreateIOCompletionPort
# Note that the completion key is just a number, not a pointer.
CreateIoCompletionPortProto = WINFUNCTYPE(HANDLE,      # Return Type
                                          HANDLE,      # File Handle
                                          HANDLE,      # Existing Completion Port
                                          c_ulong,     # Completion Key
                                          DWORD)       # Number of Threads

CreateIoCompletionPortFlags = ((1, "FileHandle", INVALID_HANDLE_VALUE),
                               (1, "ExistingCompletionPort", 0),
                               (1, "CompletionKey", c_ulong(0)),
                               (1, "NumberOfConcurrentThreads", 0))
CreateIoCompletionPort = CreateIoCompletionPortProto(("CreateIoCompletionPort",
                                                      windll.kernel32),
                                                     CreateIoCompletionPortFlags)
CreateIoCompletionPort.errcheck = ErrCheckHandle

# SetInformationJobObject
SetInformationJobObjectProto = WINFUNCTYPE(BOOL,      # Return Type
                                           HANDLE,    # Job Handle
                                           DWORD,     # Type of Class next param is
                                           LPVOID,    # Job Object Class
                                           DWORD)     # Job Object Class Length

SetInformationJobObjectProtoFlags = ((1, "hJob", None),
                                     (1, "JobObjectInfoClass", None),
                                     (1, "lpJobObjectInfo", None),
                                     (1, "cbJobObjectInfoLength", 0))
SetInformationJobObject = SetInformationJobObjectProto(("SetInformationJobObject",
                                                        windll.kernel32),
                                                       SetInformationJobObjectProtoFlags)
SetInformationJobObject.errcheck = ErrCheckBool

# CreateJobObject()
CreateJobObjectProto = WINFUNCTYPE(HANDLE,             # Return type
                                   LPVOID,             # lpJobAttributes
                                   LPCWSTR             # lpName
                                   )

CreateJobObjectFlags = ((1, "lpJobAttributes", None),
                        (1, "lpName", None))

CreateJobObject = CreateJobObjectProto(("CreateJobObjectW", windll.kernel32),
                                       CreateJobObjectFlags)
CreateJobObject.errcheck = ErrCheckHandle

# AssignProcessToJobObject()

AssignProcessToJobObjectProto = WINFUNCTYPE(BOOL,      # Return type
                                            HANDLE,    # hJob
                                            HANDLE     # hProcess
                                            )
AssignProcessToJobObjectFlags = ((1, "hJob"),
                                 (1, "hProcess"))
AssignProcessToJobObject = AssignProcessToJobObjectProto(
    ("AssignProcessToJobObject", windll.kernel32),
    AssignProcessToJobObjectFlags)
AssignProcessToJobObject.errcheck = ErrCheckBool

# GetCurrentProcess()
# because os.getPid() is way too easy
GetCurrentProcessProto = WINFUNCTYPE(HANDLE    # Return type
                                     )
GetCurrentProcessFlags = ()
GetCurrentProcess = GetCurrentProcessProto(
    ("GetCurrentProcess", windll.kernel32),
    GetCurrentProcessFlags)
GetCurrentProcess.errcheck = ErrCheckHandle

# IsProcessInJob()
try:
    IsProcessInJobProto = WINFUNCTYPE(BOOL,     # Return type
                                      HANDLE,   # Process Handle
                                      HANDLE,   # Job Handle
                                      LPBOOL      # Result
                                      )
    IsProcessInJobFlags = ((1, "ProcessHandle"),
                           (1, "JobHandle", HANDLE(0)),
                           (2, "Result"))
    IsProcessInJob = IsProcessInJobProto(
        ("IsProcessInJob", windll.kernel32),
        IsProcessInJobFlags)
    IsProcessInJob.errcheck = ErrCheckBool
except AttributeError:
    # windows 2k doesn't have this API
    def IsProcessInJob(process):
        return False


# ResumeThread()

def ErrCheckResumeThread(result, func, args):
    if result == -1:
        raise WinError()

    return args


ResumeThreadProto = WINFUNCTYPE(DWORD,      # Return type
                                HANDLE      # hThread
                                )
ResumeThreadFlags = ((1, "hThread"),)
ResumeThread = ResumeThreadProto(("ResumeThread", windll.kernel32),
                                 ResumeThreadFlags)
ResumeThread.errcheck = ErrCheckResumeThread

# TerminateProcess()

TerminateProcessProto = WINFUNCTYPE(BOOL,    # Return type
                                    HANDLE,  # hProcess
                                    UINT     # uExitCode
                                    )
TerminateProcessFlags = ((1, "hProcess"),
                         (1, "uExitCode", 127))
TerminateProcess = TerminateProcessProto(
    ("TerminateProcess", windll.kernel32),
    TerminateProcessFlags)
TerminateProcess.errcheck = ErrCheckBool

# TerminateJobObject()

TerminateJobObjectProto = WINFUNCTYPE(BOOL,    # Return type
                                      HANDLE,  # hJob
                                      UINT     # uExitCode
                                      )
TerminateJobObjectFlags = ((1, "hJob"),
                           (1, "uExitCode", 127))
TerminateJobObject = TerminateJobObjectProto(
    ("TerminateJobObject", windll.kernel32),
    TerminateJobObjectFlags)
TerminateJobObject.errcheck = ErrCheckBool

# WaitForSingleObject()

WaitForSingleObjectProto = WINFUNCTYPE(DWORD,   # Return type
                                       HANDLE,  # hHandle
                                       DWORD,   # dwMilliseconds
                                       )
WaitForSingleObjectFlags = ((1, "hHandle"),
                            (1, "dwMilliseconds", -1))
WaitForSingleObject = WaitForSingleObjectProto(
    ("WaitForSingleObject", windll.kernel32),
    WaitForSingleObjectFlags)

# http://msdn.microsoft.com/en-us/library/ms681381%28v=vs.85%29.aspx
INFINITE = -1
WAIT_TIMEOUT = 0x0102
WAIT_OBJECT_0 = 0x0
WAIT_ABANDONED = 0x0080

# http://msdn.microsoft.com/en-us/library/ms683189%28VS.85%29.aspx
STILL_ACTIVE = 259

# Used when we terminate a process.
ERROR_CONTROL_C_EXIT = 0x23c

# GetExitCodeProcess()

GetExitCodeProcessProto = WINFUNCTYPE(BOOL,     # Return type
                                      HANDLE,   # hProcess
                                      LPDWORD,  # lpExitCode
                                      )
GetExitCodeProcessFlags = ((1, "hProcess"),
                           (2, "lpExitCode"))
GetExitCodeProcess = GetExitCodeProcessProto(
    ("GetExitCodeProcess", windll.kernel32),
    GetExitCodeProcessFlags)
GetExitCodeProcess.errcheck = ErrCheckBool


def CanCreateJobObject():
    currentProc = GetCurrentProcess()
    if IsProcessInJob(currentProc):
        jobinfo = QueryInformationJobObject(HANDLE(0), 'JobObjectExtendedLimitInformation')
        limitflags = jobinfo['BasicLimitInformation']['LimitFlags']
        return bool(limitflags & JOB_OBJECT_LIMIT_BREAKAWAY_OK) or \
            bool(limitflags & JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK)
    else:
        return True

# testing functions


def parent():
    print 'Starting parent'
    currentProc = GetCurrentProcess()
    if IsProcessInJob(currentProc):
        print >> sys.stderr, "You should not be in a job object to test"
        sys.exit(1)
    assert CanCreateJobObject()
    print 'File: %s' % __file__
    command = [sys.executable, __file__, '-child']
    print 'Running command: %s' % command
    process = subprocess.Popen(command)
    process.kill()
    code = process.returncode
    print 'Child code: %s' % code
    assert code == 127


def child():
    print 'Starting child'
    currentProc = GetCurrentProcess()
    injob = IsProcessInJob(currentProc)
    print "Is in a job?: %s" % injob
    can_create = CanCreateJobObject()
    print 'Can create job?: %s' % can_create
    process = subprocess.Popen('c:\\windows\\notepad.exe')
    assert process._job
    jobinfo = QueryInformationJobObject(process._job, 'JobObjectExtendedLimitInformation')
    print 'Job info: %s' % jobinfo
    limitflags = jobinfo['BasicLimitInformation']['LimitFlags']
    print 'LimitFlags: %s' % limitflags
    process.kill()
