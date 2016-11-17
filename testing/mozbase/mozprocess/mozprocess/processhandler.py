# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import errno
import os
import signal
import subprocess
import sys
import threading
import time
import traceback
from Queue import Queue, Empty
from datetime import datetime

__all__ = ['ProcessHandlerMixin', 'ProcessHandler', 'LogOutput',
           'StoreOutput', 'StreamOutput']

# Set the MOZPROCESS_DEBUG environment variable to 1 to see some debugging output
MOZPROCESS_DEBUG = os.getenv("MOZPROCESS_DEBUG")

# We dont use mozinfo because it is expensive to import, see bug 933558.
isWin = os.name == "nt"
isPosix = os.name == "posix"  # includes MacOS X

if isWin:
    from ctypes import sizeof, addressof, c_ulong, byref, WinError, c_longlong
    from . import winprocess
    from .qijo import JobObjectAssociateCompletionPortInformation,\
        JOBOBJECT_ASSOCIATE_COMPLETION_PORT, JobObjectExtendedLimitInformation,\
        JOBOBJECT_BASIC_LIMIT_INFORMATION, JOBOBJECT_EXTENDED_LIMIT_INFORMATION, IO_COUNTERS


class ProcessHandlerMixin(object):
    """
    A class for launching and manipulating local processes.

    :param cmd: command to run. May be a string or a list. If specified as a list, the first
      element will be interpreted as the command, and all additional elements will be interpreted
      as arguments to that command.
    :param args: list of arguments to pass to the command (defaults to None). Must not be set when
      `cmd` is specified as a list.
    :param cwd: working directory for command (defaults to None).
    :param env: is the environment to use for the process (defaults to os.environ).
    :param ignore_children: causes system to ignore child processes when True,
      defaults to False (which tracks child processes).
    :param kill_on_timeout: when True, the process will be killed when a timeout is reached.
      When False, the caller is responsible for killing the process.
      Failure to do so could cause a call to wait() to hang indefinitely. (Defaults to True.)
    :param processOutputLine: function or list of functions to be called for
        each line of output produced by the process (defaults to an empty
        list).
    :param processStderrLine: function or list of functions to be called
        for each line of error output - stderr - produced by the process
        (defaults to an empty list). If this is not specified, stderr lines
        will be sent to the *processOutputLine* callbacks.
    :param onTimeout: function or list of functions to be called when the process times out.
    :param onFinish: function or list of functions to be called when the process terminates
      normally without timing out.
    :param kwargs: additional keyword args to pass directly into Popen.

    NOTE: Child processes will be tracked by default.  If for any reason
    we are unable to track child processes and ignore_children is set to False,
    then we will fall back to only tracking the root process.  The fallback
    will be logged.
    """

    class Process(subprocess.Popen):
        """
        Represents our view of a subprocess.
        It adds a kill() method which allows it to be stopped explicitly.
        """

        MAX_IOCOMPLETION_PORT_NOTIFICATION_DELAY = 180
        MAX_PROCESS_KILL_DELAY = 30
        TIMEOUT_BEFORE_SIGKILL = 1.0

        def __init__(self,
                     args,
                     bufsize=0,
                     executable=None,
                     stdin=None,
                     stdout=None,
                     stderr=None,
                     preexec_fn=None,
                     close_fds=False,
                     shell=False,
                     cwd=None,
                     env=None,
                     universal_newlines=False,
                     startupinfo=None,
                     creationflags=0,
                     ignore_children=False):

            # Parameter for whether or not we should attempt to track child processes
            self._ignore_children = ignore_children

            if not self._ignore_children and not isWin:
                # Set the process group id for linux systems
                # Sets process group id to the pid of the parent process
                # NOTE: This prevents you from using preexec_fn and managing
                #       child processes, TODO: Ideally, find a way around this
                def setpgidfn():
                    os.setpgid(0, 0)
                preexec_fn = setpgidfn

            try:
                subprocess.Popen.__init__(self, args, bufsize, executable,
                                          stdin, stdout, stderr,
                                          preexec_fn, close_fds,
                                          shell, cwd, env,
                                          universal_newlines, startupinfo, creationflags)
            except OSError:
                print >> sys.stderr, args
                raise

        def debug(self, msg):
            if not MOZPROCESS_DEBUG:
                return
            thread = threading.current_thread().name
            print("DBG::MOZPROC PID:{} ({}) | {}".format(self.pid, thread, msg))

        def __del__(self, _maxint=sys.maxint):
            if isWin:
                handle = getattr(self, '_handle', None)
                if handle:
                    if hasattr(self, '_internal_poll'):
                        self._internal_poll(_deadstate=_maxint)
                    else:
                        self.poll(_deadstate=sys.maxint)
                if handle or self._job or self._io_port:
                    self._cleanup()
            else:
                subprocess.Popen.__del__(self)

        def kill(self, sig=None):
            if isWin:
                if not self._ignore_children and self._handle and self._job:
                    self.debug("calling TerminateJobObject")
                    winprocess.TerminateJobObject(self._job, winprocess.ERROR_CONTROL_C_EXIT)
                    self.returncode = winprocess.GetExitCodeProcess(self._handle)
                elif self._handle:
                    self.debug("calling TerminateProcess")
                    try:
                        winprocess.TerminateProcess(self._handle, winprocess.ERROR_CONTROL_C_EXIT)
                    except:
                        traceback.print_exc()
                        raise OSError("Could not terminate process")
                    finally:
                        winprocess.GetExitCodeProcess(self._handle)
                        self._cleanup()
            else:
                def send_sig(sig):
                    pid = self.detached_pid or self.pid
                    if not self._ignore_children:
                        try:
                            os.killpg(pid, sig)
                        except BaseException as e:
                            # ESRCH is a "no such process" failure, which is fine because the
                            # application might already have been terminated itself. Any other
                            # error would indicate a problem in killing the process.
                            if getattr(e, "errno", None) != errno.ESRCH:
                                print >> sys.stderr, "Could not terminate process: %s" % self.pid
                                raise
                    else:
                        os.kill(pid, sig)

                if sig is None and isPosix:
                    # ask the process for termination and wait a bit
                    send_sig(signal.SIGTERM)
                    limit = time.time() + self.TIMEOUT_BEFORE_SIGKILL
                    while time.time() <= limit:
                        if self.poll() is not None:
                            # process terminated nicely
                            break
                        time.sleep(0.02)
                    else:
                        # process did not terminate - send SIGKILL to force
                        send_sig(signal.SIGKILL)
                else:
                    # a signal was explicitly set or not posix
                    send_sig(sig or signal.SIGKILL)

            self.returncode = self.wait()
            self._cleanup()
            return self.returncode

        def poll(self):
            """ Popen.poll
                Check if child process has terminated. Set and return returncode attribute.
            """
            # If we have a handle, the process is alive
            if isWin and getattr(self, '_handle', None):
                return None

            return subprocess.Popen.poll(self)

        def wait(self):
            """ Popen.wait
                Called to wait for a running process to shut down and return
                its exit code
                Returns the main process's exit code
            """
            # This call will be different for each OS
            self.returncode = self._wait()
            self._cleanup()
            return self.returncode

        """ Private Members of Process class """

        if isWin:
            # Redefine the execute child so that we can track process groups
            def _execute_child(self, *args_tuple):
                # workaround for bug 950894
                if sys.hexversion < 0x02070600:  # prior to 2.7.6
                    (args, executable, preexec_fn, close_fds,
                     cwd, env, universal_newlines, startupinfo,
                     creationflags, shell,
                     p2cread, p2cwrite,
                     c2pread, c2pwrite,
                     errread, errwrite) = args_tuple
                    to_close = set()
                else:  # 2.7.6 and later
                    (args, executable, preexec_fn, close_fds,
                     cwd, env, universal_newlines, startupinfo,
                     creationflags, shell, to_close,
                     p2cread, p2cwrite,
                     c2pread, c2pwrite,
                     errread, errwrite) = args_tuple
                if not isinstance(args, basestring):
                    args = subprocess.list2cmdline(args)

                # Always or in the create new process group
                creationflags |= winprocess.CREATE_NEW_PROCESS_GROUP

                if startupinfo is None:
                    startupinfo = winprocess.STARTUPINFO()

                if None not in (p2cread, c2pwrite, errwrite):
                    startupinfo.dwFlags |= winprocess.STARTF_USESTDHANDLES
                    startupinfo.hStdInput = int(p2cread)
                    startupinfo.hStdOutput = int(c2pwrite)
                    startupinfo.hStdError = int(errwrite)
                if shell:
                    startupinfo.dwFlags |= winprocess.STARTF_USESHOWWINDOW
                    startupinfo.wShowWindow = winprocess.SW_HIDE
                    comspec = os.environ.get("COMSPEC", "cmd.exe")
                    args = comspec + " /c " + args

                # Determine if we can create a job or create nested jobs.
                can_create_job = winprocess.CanCreateJobObject()
                can_nest_jobs = self._can_nest_jobs()

                # Ensure we write a warning message if we are falling back
                if not (can_create_job or can_nest_jobs) and not self._ignore_children:
                    # We can't create job objects AND the user wanted us to
                    # Warn the user about this.
                    print >> sys.stderr, \
                        "ProcessManager UNABLE to use job objects to manage child processes"

                # set process creation flags
                creationflags |= winprocess.CREATE_SUSPENDED
                creationflags |= winprocess.CREATE_UNICODE_ENVIRONMENT
                if can_create_job:
                    creationflags |= winprocess.CREATE_BREAKAWAY_FROM_JOB
                if not (can_create_job or can_nest_jobs):
                    # Since we've warned, we just log info here to inform you
                    # of the consequence of setting ignore_children = True
                    print "ProcessManager NOT managing child processes"

                # create the process
                hp, ht, pid, tid = winprocess.CreateProcess(
                    executable, args,
                    None, None,  # No special security
                    1,  # Must inherit handles!
                    creationflags,
                    winprocess.EnvironmentBlock(env),
                    cwd, startupinfo)
                self._child_created = True
                self._handle = hp
                self._thread = ht
                self.pid = pid
                self.tid = tid

                if not self._ignore_children and (can_create_job or can_nest_jobs):
                    try:
                        # We create a new job for this process, so that we can kill
                        # the process and any sub-processes
                        # Create the IO Completion Port
                        self._io_port = winprocess.CreateIoCompletionPort()
                        self._job = winprocess.CreateJobObject()

                        # Now associate the io comp port and the job object
                        joacp = JOBOBJECT_ASSOCIATE_COMPLETION_PORT(winprocess.COMPKEY_JOBOBJECT,
                                                                    self._io_port)
                        winprocess.SetInformationJobObject(
                            self._job,
                            JobObjectAssociateCompletionPortInformation,
                            addressof(joacp),
                            sizeof(joacp)
                        )

                        # Allow subprocesses to break away from us - necessary for
                        # flash with protected mode
                        jbli = JOBOBJECT_BASIC_LIMIT_INFORMATION(
                            c_longlong(0),  # per process time limit (ignored)
                            c_longlong(0),  # per job user time limit (ignored)
                            winprocess.JOB_OBJECT_LIMIT_BREAKAWAY_OK,
                            0,  # min working set (ignored)
                            0,  # max working set (ignored)
                            0,  # active process limit (ignored)
                            None,  # affinity (ignored)
                            0,  # Priority class (ignored)
                            0,  # Scheduling class (ignored)
                        )

                        iocntr = IO_COUNTERS()
                        jeli = JOBOBJECT_EXTENDED_LIMIT_INFORMATION(
                            jbli,  # basic limit info struct
                            iocntr,    # io_counters (ignored)
                            0,    # process mem limit (ignored)
                            0,    # job mem limit (ignored)
                            0,    # peak process limit (ignored)
                            0)    # peak job limit (ignored)

                        winprocess.SetInformationJobObject(self._job,
                                                           JobObjectExtendedLimitInformation,
                                                           addressof(jeli),
                                                           sizeof(jeli)
                                                           )

                        # Assign the job object to the process
                        winprocess.AssignProcessToJobObject(self._job, int(hp))

                        # It's overkill, but we use Queue to signal between threads
                        # because it handles errors more gracefully than event or condition.
                        self._process_events = Queue()

                        # Spin up our thread for managing the IO Completion Port
                        self._procmgrthread = threading.Thread(target=self._procmgr)
                    except:
                        print >> sys.stderr, """Exception trying to use job objects;
falling back to not using job objects for managing child processes"""
                        tb = traceback.format_exc()
                        print >> sys.stderr, tb
                        # Ensure no dangling handles left behind
                        self._cleanup_job_io_port()
                else:
                    self._job = None

                winprocess.ResumeThread(int(ht))
                if getattr(self, '_procmgrthread', None):
                    self._procmgrthread.start()
                ht.Close()

                for i in (p2cread, c2pwrite, errwrite):
                    if i is not None:
                        i.Close()

            # Per:
            # https://msdn.microsoft.com/en-us/library/windows/desktop/hh448388%28v=vs.85%29.aspx
            # Nesting jobs came in with windows versions starting with 6.2 according to the table
            # on this page:
            # https://msdn.microsoft.com/en-us/library/ms724834%28v=vs.85%29.aspx
            def _can_nest_jobs(self):
                winver = sys.getwindowsversion()
                return (winver.major > 6 or
                        winver.major == 6 and winver.minor >= 2)

            # Windows Process Manager - watches the IO Completion Port and
            # keeps track of child processes
            def _procmgr(self):
                if not (self._io_port) or not (self._job):
                    return

                try:
                    self._poll_iocompletion_port()
                except KeyboardInterrupt:
                    raise KeyboardInterrupt

            def _poll_iocompletion_port(self):
                # Watch the IO Completion port for status
                self._spawned_procs = {}
                countdowntokill = 0

                self.debug("start polling IO completion port")

                while True:
                    msgid = c_ulong(0)
                    compkey = c_ulong(0)
                    pid = c_ulong(0)
                    portstatus = winprocess.GetQueuedCompletionStatus(self._io_port,
                                                                      byref(msgid),
                                                                      byref(compkey),
                                                                      byref(pid),
                                                                      5000)

                    # If the countdowntokill has been activated, we need to check
                    # if we should start killing the children or not.
                    if countdowntokill != 0:
                        diff = datetime.now() - countdowntokill
                        # Arbitrarily wait 3 minutes for windows to get its act together
                        # Windows sometimes takes a small nap between notifying the
                        # IO Completion port and actually killing the children, and we
                        # don't want to mistake that situation for the situation of an unexpected
                        # parent abort (which is what we're looking for here).
                        if diff.seconds > self.MAX_IOCOMPLETION_PORT_NOTIFICATION_DELAY:
                            print >> sys.stderr, \
                                "WARNING | IO Completion Port failed to signal process shutdown"
                            print >> sys.stderr, \
                                "Parent process %s exited with children alive:" % self.pid
                            print >> sys.stderr, \
                                "PIDS: %s" % ', '.join([str(i) for i in self._spawned_procs])
                            print >> sys.stderr, \
                                "Attempting to kill them, but no guarantee of success"

                            self.kill()
                            self._process_events.put({self.pid: 'FINISHED'})
                            break

                    if not portstatus:
                        # Check to see what happened
                        errcode = winprocess.GetLastError()
                        if errcode == winprocess.ERROR_ABANDONED_WAIT_0:
                            # Then something has killed the port, break the loop
                            print >> sys.stderr, "IO Completion Port unexpectedly closed"
                            self._process_events.put({self.pid: 'FINISHED'})
                            break
                        elif errcode == winprocess.WAIT_TIMEOUT:
                            # Timeouts are expected, just keep on polling
                            continue
                        else:
                            print >> sys.stderr, \
                                "Error Code %s trying to query IO Completion Port, " \
                                "exiting" % errcode
                            raise WinError(errcode)
                            break

                    if compkey.value == winprocess.COMPKEY_TERMINATE.value:
                        self.debug("compkeyterminate detected")
                        # Then we're done
                        break

                    # Check the status of the IO Port and do things based on it
                    if compkey.value == winprocess.COMPKEY_JOBOBJECT.value:
                        if msgid.value == winprocess.JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO:
                            # No processes left, time to shut down
                            # Signal anyone waiting on us that it is safe to shut down
                            self.debug("job object msg active processes zero")
                            self._process_events.put({self.pid: 'FINISHED'})
                            break
                        elif msgid.value == winprocess.JOB_OBJECT_MSG_NEW_PROCESS:
                            # New Process started
                            # Add the child proc to our list in case our parent flakes out on us
                            # without killing everything.
                            if pid.value != self.pid:
                                self._spawned_procs[pid.value] = 1
                                self.debug("new process detected with pid value: %s" % pid.value)
                        elif msgid.value == winprocess.JOB_OBJECT_MSG_EXIT_PROCESS:
                            self.debug("process id %s exited normally" % pid.value)
                            # One process exited normally
                            if pid.value == self.pid and len(self._spawned_procs) > 0:
                                # Parent process dying, start countdown timer
                                countdowntokill = datetime.now()
                            elif pid.value in self._spawned_procs:
                                # Child Process died remove from list
                                del(self._spawned_procs[pid.value])
                        elif msgid.value == winprocess.JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS:
                            # One process existed abnormally
                            self.debug("process id %s exited abnormally" % pid.value)
                            if pid.value == self.pid and len(self._spawned_procs) > 0:
                                # Parent process dying, start countdown timer
                                countdowntokill = datetime.now()
                            elif pid.value in self._spawned_procs:
                                # Child Process died remove from list
                                del self._spawned_procs[pid.value]
                        else:
                            # We don't care about anything else
                            self.debug("We got a message %s" % msgid.value)
                            pass

            def _wait(self):
                # First, check to see if the process is still running
                if self._handle:
                    self.returncode = winprocess.GetExitCodeProcess(self._handle)
                else:
                    # Dude, the process is like totally dead!
                    return self.returncode

                threadalive = False
                if hasattr(self, "_procmgrthread"):
                    threadalive = self._procmgrthread.is_alive()
                if self._job and threadalive and threading.current_thread() != self._procmgrthread:
                    self.debug("waiting with IO completion port")
                    # Then we are managing with IO Completion Ports
                    # wait on a signal so we know when we have seen the last
                    # process come through.
                    # We use queues to synchronize between the thread and this
                    # function because events just didn't have robust enough error
                    # handling on pre-2.7 versions
                    try:
                        # timeout is the max amount of time the procmgr thread will wait for
                        # child processes to shutdown before killing them with extreme prejudice.
                        item = self._process_events.get(
                            timeout=self.MAX_IOCOMPLETION_PORT_NOTIFICATION_DELAY +
                            self.MAX_PROCESS_KILL_DELAY)
                        if item[self.pid] == 'FINISHED':
                            self.debug("received 'FINISHED' from _procmgrthread")
                            self._process_events.task_done()
                    except:
                        traceback.print_exc()
                        raise OSError("IO Completion Port failed to signal process shutdown")
                    finally:
                        if self._handle:
                            self.returncode = winprocess.GetExitCodeProcess(self._handle)
                        self._cleanup()

                else:
                    # Not managing with job objects, so all we can reasonably do
                    # is call waitforsingleobject and hope for the best
                    self.debug("waiting without IO completion port")

                    if not self._ignore_children:
                        self.debug("NOT USING JOB OBJECTS!!!")
                    # First, make sure we have not already ended
                    if self.returncode != winprocess.STILL_ACTIVE:
                        self._cleanup()
                        return self.returncode

                    rc = None
                    if self._handle:
                        rc = winprocess.WaitForSingleObject(self._handle, -1)

                    if rc == winprocess.WAIT_TIMEOUT:
                        # The process isn't dead, so kill it
                        print "Timed out waiting for process to close, attempting TerminateProcess"
                        self.kill()
                    elif rc == winprocess.WAIT_OBJECT_0:
                        # We caught WAIT_OBJECT_0, which indicates all is well
                        print "Single process terminated successfully"
                        self.returncode = winprocess.GetExitCodeProcess(self._handle)
                    else:
                        # An error occured we should probably throw
                        rc = winprocess.GetLastError()
                        if rc:
                            raise WinError(rc)

                    self._cleanup()

                return self.returncode

            def _cleanup_job_io_port(self):
                """ Do the job and IO port cleanup separately because there are
                    cases where we want to clean these without killing _handle
                    (i.e. if we fail to create the job object in the first place)
                """
                if getattr(self, '_job') and self._job != winprocess.INVALID_HANDLE_VALUE:
                    self._job.Close()
                    self._job = None
                else:
                    # If windows already freed our handle just set it to none
                    # (saw this intermittently while testing)
                    self._job = None

                if getattr(self, '_io_port', None) and \
                   self._io_port != winprocess.INVALID_HANDLE_VALUE:
                    self._io_port.Close()
                    self._io_port = None
                else:
                    self._io_port = None

                if getattr(self, '_procmgrthread', None):
                    self._procmgrthread = None

            def _cleanup(self):
                self._cleanup_job_io_port()
                if self._thread and self._thread != winprocess.INVALID_HANDLE_VALUE:
                    self._thread.Close()
                    self._thread = None
                else:
                    self._thread = None

                if self._handle and self._handle != winprocess.INVALID_HANDLE_VALUE:
                    self._handle.Close()
                    self._handle = None
                else:
                    self._handle = None

        elif isPosix:

            def _wait(self):
                """ Haven't found any reason to differentiate between these platforms
                    so they all use the same wait callback.  If it is necessary to
                    craft different styles of wait, then a new _wait method
                    could be easily implemented.
                """

                if not self._ignore_children:
                    try:
                        # os.waitpid return value:
                        # > [...] a tuple containing its pid and exit status
                        # > indication: a 16-bit number, whose low byte is the
                        # > signal number that killed the process, and whose
                        # > high byte is the exit status (if the signal number
                        # > is zero)
                        # - http://docs.python.org/2/library/os.html#os.wait
                        status = os.waitpid(self.pid, 0)[1]

                        # For consistency, format status the same as subprocess'
                        # returncode attribute
                        if status > 255:
                            return status >> 8
                        return -status
                    except OSError as e:
                        if getattr(e, "errno", None) != 10:
                            # Error 10 is "no child process", which could indicate normal
                            # close
                            print >> sys.stderr, \
                                "Encountered error waiting for pid to close: %s" % e
                            raise

                        return self.returncode

                else:
                    # For non-group wait, call base class
                    subprocess.Popen.wait(self)
                    return self.returncode

            def _cleanup(self):
                pass

        else:
            # An unrecognized platform, we will call the base class for everything
            print >> sys.stderr, \
                "Unrecognized platform, process groups may not be managed properly"

            def _wait(self):
                self.returncode = subprocess.Popen.wait(self)
                return self.returncode

            def _cleanup(self):
                pass

    def __init__(self,
                 cmd,
                 args=None,
                 cwd=None,
                 env=None,
                 ignore_children=False,
                 kill_on_timeout=True,
                 processOutputLine=(),
                 processStderrLine=(),
                 onTimeout=(),
                 onFinish=(),
                 **kwargs):
        self.cmd = cmd
        self.args = args
        self.cwd = cwd
        self.didTimeout = False
        self._ignore_children = ignore_children
        self.keywordargs = kwargs
        self.read_buffer = ''

        if env is None:
            env = os.environ.copy()
        self.env = env

        # handlers
        def to_callable_list(arg):
            if callable(arg):
                arg = [arg]
            return CallableList(arg)

        processOutputLine = to_callable_list(processOutputLine)
        processStderrLine = to_callable_list(processStderrLine)
        onTimeout = to_callable_list(onTimeout)
        onFinish = to_callable_list(onFinish)

        def on_timeout():
            self.didTimeout = True
            if kill_on_timeout:
                self.kill()
        onTimeout.insert(0, on_timeout)

        self._stderr = subprocess.STDOUT
        if processStderrLine:
            self._stderr = subprocess.PIPE
        self.reader = ProcessReader(stdout_callback=processOutputLine,
                                    stderr_callback=processStderrLine,
                                    finished_callback=onFinish,
                                    timeout_callback=onTimeout)

        # It is common for people to pass in the entire array with the cmd and
        # the args together since this is how Popen uses it.  Allow for that.
        if isinstance(self.cmd, list):
            if self.args is not None:
                raise TypeError("cmd and args must not both be lists")
            (self.cmd, self.args) = (self.cmd[0], self.cmd[1:])
        elif self.args is None:
            self.args = []

    @property
    def timedOut(self):
        """True if the process has timed out."""
        return self.didTimeout

    @property
    def commandline(self):
        """the string value of the command line (command + args)"""
        return subprocess.list2cmdline([self.cmd] + self.args)

    def run(self, timeout=None, outputTimeout=None):
        """
        Starts the process.

        If timeout is not None, the process will be allowed to continue for
        that number of seconds before being killed. If the process is killed
        due to a timeout, the onTimeout handler will be called.

        If outputTimeout is not None, the process will be allowed to continue
        for that number of seconds without producing any output before
        being killed.
        """
        self.didTimeout = False

        # default arguments
        args = dict(stdout=subprocess.PIPE,
                    stderr=self._stderr,
                    cwd=self.cwd,
                    env=self.env,
                    ignore_children=self._ignore_children)

        # build process arguments
        args.update(self.keywordargs)

        # launch the process
        self.proc = self.Process([self.cmd] + self.args, **args)

        if isPosix:
            # Keep track of the initial process group in case the process detaches itself
            self.proc.pgid = self._getpgid(self.proc.pid)
            self.proc.detached_pid = None

        self.processOutput(timeout=timeout, outputTimeout=outputTimeout)

    def kill(self, sig=None):
        """
        Kills the managed process.

        If you created the process with 'ignore_children=False' (the
        default) then it will also also kill all child processes spawned by
        it. If you specified 'ignore_children=True' when creating the
        process, only the root process will be killed.

        Note that this does not manage any state, save any output etc,
        it immediately kills the process.

        :param sig: Signal used to kill the process, defaults to SIGKILL
                    (has no effect on Windows)
        """
        if not hasattr(self, 'proc'):
            raise RuntimeError("Calling kill() on a non started process is not"
                               " allowed.")
        self.proc.kill(sig=sig)

        # When we kill the the managed process we also have to wait for the
        # reader thread to be finished. Otherwise consumers would have to assume
        # that it still has not completely shutdown.
        return self.wait()

    def poll(self):
        """Check if child process has terminated

        Returns the current returncode value:
        - None if the process hasn't terminated yet
        - A negative number if the process was killed by signal N (Unix only)
        - '0' if the process ended without failures

        """
        # Ensure that we first check for the reader status. Otherwise
        # we might mark the process as finished while output is still getting
        # processed.
        if not hasattr(self, 'proc'):
            raise RuntimeError("Calling poll() on a non started process is not"
                               " allowed.")
        elif self.reader.is_alive():
            return None
        elif hasattr(self.proc, "returncode"):
            return self.proc.returncode
        else:
            return self.proc.poll()

    def processOutput(self, timeout=None, outputTimeout=None):
        """
        Handle process output until the process terminates or times out.

        If timeout is not None, the process will be allowed to continue for
        that number of seconds before being killed.

        If outputTimeout is not None, the process will be allowed to continue
        for that number of seconds without producing any output before
        being killed.
        """
        # this method is kept for backward compatibility
        if not hasattr(self, 'proc'):
            self.run(timeout=timeout, outputTimeout=outputTimeout)
            # self.run will call this again
            return
        if not self.reader.is_alive():
            self.reader.timeout = timeout
            self.reader.output_timeout = outputTimeout
            self.reader.start(self.proc)

    def wait(self, timeout=None):
        """
        Waits until all output has been read and the process is
        terminated.

        If timeout is not None, will return after timeout seconds.
        This timeout only causes the wait function to return and
        does not kill the process.

        Returns the process exit code value:
        - None if the process hasn't terminated yet
        - A negative number if the process was killed by signal N (Unix only)
        - '0' if the process ended without failures

        """
        if self.reader.thread and self.reader.thread is not threading.current_thread():
            # Thread.join() blocks the main thread until the reader thread is finished
            # wake up once a second in case a keyboard interrupt is sent
            count = 0
            while self.reader.is_alive():
                self.reader.thread.join(timeout=1)
                count += 1
                if timeout and count > timeout:
                    return None

        self.returncode = self.proc.wait()
        return self.returncode

    # TODO Remove this method when consumers have been fixed
    def waitForFinish(self, timeout=None):
        print >> sys.stderr, "MOZPROCESS WARNING: ProcessHandler.waitForFinish() is deprecated, " \
                             "use ProcessHandler.wait() instead"
        return self.wait(timeout=timeout)

    @property
    def pid(self):
        return self.proc.pid

    @classmethod
    def _getpgid(cls, pid):
        try:
            return os.getpgid(pid)
        except OSError as e:
            # Do not raise for "No such process"
            if e.errno != errno.ESRCH:
                raise

    def check_for_detached(self, new_pid):
        """Check if the current process has been detached and mark it appropriately.

        In case of application restarts the process can spawn itself into a new process group.
        From now on the process can no longer be tracked by mozprocess anymore and has to be
        marked as detached. If the consumer of mozprocess still knows the new process id it could
        check for the detached state.

        new_pid is the new process id of the child process.
        """
        if not self.proc:
            return

        if isPosix:
            new_pgid = self._getpgid(new_pid)

            if new_pgid and new_pgid != self.proc.pgid:
                self.proc.detached_pid = new_pid
                print >> sys.stdout, \
                    'Child process with id "%s" has been marked as detached because it is no ' \
                    'longer in the managed process group. Keeping reference to the process id ' \
                    '"%s" which is the new child process.' % (self.pid, new_pid)


class CallableList(list):

    def __call__(self, *args, **kwargs):
        for e in self:
            e(*args, **kwargs)

    def __add__(self, lst):
        return CallableList(list.__add__(self, lst))


class ProcessReader(object):

    def __init__(self, stdout_callback=None, stderr_callback=None,
                 finished_callback=None, timeout_callback=None,
                 timeout=None, output_timeout=None):
        self.stdout_callback = stdout_callback or (lambda line: True)
        self.stderr_callback = stderr_callback or (lambda line: True)
        self.finished_callback = finished_callback or (lambda: True)
        self.timeout_callback = timeout_callback or (lambda: True)
        self.timeout = timeout
        self.output_timeout = output_timeout
        self.thread = None

    def _create_stream_reader(self, name, stream, queue, callback):
        thread = threading.Thread(name=name,
                                  target=self._read_stream,
                                  args=(stream, queue, callback))
        thread.daemon = True
        thread.start()
        return thread

    def _read_stream(self, stream, queue, callback):
        while True:
            line = stream.readline()
            if not line:
                break
            queue.put((line, callback))
        stream.close()

    def start(self, proc):
        queue = Queue()
        stdout_reader = None
        if proc.stdout:
            stdout_reader = self._create_stream_reader('ProcessReaderStdout',
                                                       proc.stdout,
                                                       queue,
                                                       self.stdout_callback)
        stderr_reader = None
        if proc.stderr and proc.stderr != proc.stdout:
            stderr_reader = self._create_stream_reader('ProcessReaderStderr',
                                                       proc.stderr,
                                                       queue,
                                                       self.stderr_callback)
        self.thread = threading.Thread(name='ProcessReader',
                                       target=self._read,
                                       args=(stdout_reader,
                                             stderr_reader,
                                             queue))
        self.thread.daemon = True
        self.thread.start()

    def _read(self, stdout_reader, stderr_reader, queue):
        start_time = time.time()
        timed_out = False
        timeout = self.timeout
        if timeout is not None:
            timeout += start_time
        output_timeout = self.output_timeout
        if output_timeout is not None:
            output_timeout += start_time

        while (stdout_reader and stdout_reader.is_alive()) \
                or (stderr_reader and stderr_reader.is_alive()):
            has_line = True
            try:
                line, callback = queue.get(True, 0.02)
            except Empty:
                has_line = False
            now = time.time()
            if not has_line:
                if output_timeout is not None and now > output_timeout:
                    timed_out = True
                    break
            else:
                if output_timeout is not None:
                    output_timeout = now + self.output_timeout
                callback(line.rstrip())
            if timeout is not None and now > timeout:
                timed_out = True
                break
        # process remaining lines to read
        while not queue.empty():
            line, callback = queue.get(False)
            callback(line.rstrip())
        if timed_out:
            self.timeout_callback()
        if stdout_reader:
            stdout_reader.join()
        if stderr_reader:
            stderr_reader.join()
        if not timed_out:
            self.finished_callback()

    def is_alive(self):
        if self.thread:
            return self.thread.is_alive()
        return False

# default output handlers
# these should be callables that take the output line


class StoreOutput(object):
    """accumulate stdout"""

    def __init__(self):
        self.output = []

    def __call__(self, line):
        self.output.append(line)


class StreamOutput(object):
    """pass output to a stream and flush"""

    def __init__(self, stream):
        self.stream = stream

    def __call__(self, line):
        try:
            self.stream.write(line + '\n')
        except UnicodeDecodeError:
            # TODO: Workaround for bug #991866 to make sure we can display when
            # when normal UTF-8 display is failing
            self.stream.write(line.decode('iso8859-1') + '\n')
        self.stream.flush()


class LogOutput(StreamOutput):
    """pass output to a file"""

    def __init__(self, filename):
        self.file_obj = open(filename, 'a')
        StreamOutput.__init__(self, self.file_obj)

    def __del__(self):
        if self.file_obj is not None:
            self.file_obj.close()


# front end class with the default handlers


class ProcessHandler(ProcessHandlerMixin):
    """
    Convenience class for handling processes with default output handlers.

    By default, all output is sent to stdout. This can be disabled by setting
    the *stream* argument to None.

    If processOutputLine keyword argument is specified the function or the
    list of functions specified by this argument will be called for each line
    of output; the output will not be written to stdout automatically then
    if stream is True (the default).

    If storeOutput==True, the output produced by the process will be saved
    as self.output.

    If logfile is not None, the output produced by the process will be
    appended to the given file.
    """

    def __init__(self, cmd, logfile=None, stream=True, storeOutput=True,
                 **kwargs):
        kwargs.setdefault('processOutputLine', [])
        if callable(kwargs['processOutputLine']):
            kwargs['processOutputLine'] = [kwargs['processOutputLine']]

        if logfile:
            logoutput = LogOutput(logfile)
            kwargs['processOutputLine'].append(logoutput)

        if stream is True:
            # Print to standard output only if no outputline provided
            if not kwargs['processOutputLine']:
                kwargs['processOutputLine'].append(StreamOutput(sys.stdout))
        elif stream:
            streamoutput = StreamOutput(stream)
            kwargs['processOutputLine'].append(streamoutput)

        self.output = None
        if storeOutput:
            storeoutput = StoreOutput()
            self.output = storeoutput.output
            kwargs['processOutputLine'].append(storeoutput)

        ProcessHandlerMixin.__init__(self, cmd, **kwargs)
