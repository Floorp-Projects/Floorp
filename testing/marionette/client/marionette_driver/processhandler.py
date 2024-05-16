# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# The Marionette ProcessHandler and ProcessHandlerMixin classes are only
# utilized by Marionette as an alternative to the mozprocess package.
#
# This necessity arises because Marionette supports the application to
# restart itself and, under such conditions, fork its process. To maintain
# the ability to track the process, including permissions to terminate
# the process and receive log entries via stdout and stderr, the psutil
# package is utilized. To prevent any side effects for consumers of
# mozprocess, all necessary helper classes have been duplicated for now.

import codecs
import os
import signal
import subprocess
import sys
import threading
import time
import traceback
from queue import Empty, Queue

import psutil
import six

# Set the MOZPROCESS_DEBUG environment variable to 1 to see some debugging output
MOZPROCESS_DEBUG = os.getenv("MOZPROCESS_DEBUG")

INTERVAL_PROCESS_ALIVE_CHECK = 0.02

# For not self-managed processes the returncode seems to not be available.
# Use `8` to indicate this specific situation for now.
UNKNOWN_RETURNCODE = 8

# We dont use mozinfo because it is expensive to import, see bug 933558.
isPosix = os.name == "posix"  # includes MacOS X


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

    NOTE: Child processes will be tracked by default.
    """

    def __init__(
        self,
        cmd,
        args=None,
        cwd=None,
        env=None,
        kill_on_timeout=True,
        processOutputLine=(),
        processStderrLine=(),
        onTimeout=(),
        onFinish=(),
        **kwargs,
    ):
        self.args = args
        self.cmd = cmd
        self.cwd = cwd
        self.keywordargs = kwargs

        self.didTimeout = False
        self.didOutputTimeout = False
        self.proc = None

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
            self.didOutputTimeout = self.reader.didOutputTimeout
            if kill_on_timeout:
                self.kill()

        onTimeout.insert(0, on_timeout)

        self._stderr = subprocess.STDOUT
        if processStderrLine:
            self._stderr = subprocess.PIPE
        self.reader = ProcessReader(
            stdout_callback=processOutputLine,
            stderr_callback=processStderrLine,
            finished_callback=onFinish,
            timeout_callback=onTimeout,
        )

        # It is common for people to pass in the entire array with the cmd and
        # the args together since this is how Popen uses it.  Allow for that.
        if isinstance(self.cmd, list):
            if self.args is not None:
                raise TypeError("cmd and args must not both be lists")
            (self.cmd, self.args) = (self.cmd[0], self.cmd[1:])
        elif self.args is None:
            self.args = []

    def _has_valid_proc(func):
        def wrapper(self, *args, **kwargs):
            if self.proc is None:
                raise RuntimeError("Process hasn't been started yet")
            return func(self, *args, **kwargs)

        return wrapper

    @property
    @_has_valid_proc
    def pid(self):
        return self.proc.pid

    @staticmethod
    def pid_exists(pid):
        return psutil.pid_exists(pid)

    @property
    def timedOut(self):
        """True if the process has timed out for any reason."""
        return self.didTimeout

    @property
    def outputTimedOut(self):
        """True if the process has timed out for no output."""
        return self.didOutputTimeout

    @property
    def commandline(self):
        """the string value of the command line (command + args)"""
        return subprocess.list2cmdline([self.cmd] + self.args)

    def _debug(self, msg):
        if not MOZPROCESS_DEBUG:
            return

        print(f"DBG::MARIONETTE ProcessHandler {self.pid} | {msg}", file=sys.stdout)

    @_has_valid_proc
    def kill(self, sig=None, timeout=None):
        """Kills the managed process and all its child processes.

        :param sig: Signal to use to kill the process. (Defaults to SIGKILL)

        :param timeout: If not None, wait this number of seconds for the
        process to exit.

        Note that this does not manage any state, save any output etc,
        it immediately kills the process.
        """
        if hasattr(self, "returncode"):
            return self.returncode

        if self.proc.is_running():
            processes = [self.proc] + self.proc.children(recursive=True)

            if sig is None:
                # TODO: try SIGTERM first to sanely shutdown the application
                # and to not break later when Windows support gets added.
                sig = signal.SIGKILL

            # Do we need that?
            for process in processes:
                try:
                    self._debug(f"Killing process: {process}")
                    process.send_signal(sig)
                except psutil.NoSuchProcess:
                    pass
            psutil.wait_procs(processes, timeout=timeout)

        # When we kill the the managed process we also have to wait for the
        # reader thread to be finished. Otherwise consumers would have to assume
        # that it still has not completely shutdown.
        self.returncode = self.wait(0)
        if self.returncode is None:
            self._debug("kill: wait failed -- process is still alive")

        return self.returncode

    @_has_valid_proc
    def poll(self):
        """Check if child process has terminated

        Returns the current returncode value:
        - None if the process hasn't terminated yet
        - A negative number if the process was killed by signal N (Unix only)
        - '0' if the process ended without failures

        """
        if hasattr(self, "returncode"):
            return self.returncode

        # If the process that is observed wasn't started with Popen there is
        # no `poll()` method available. Use `wait()` instead and do not wait
        # for the reader thread because it would cause extra delays.
        return self.wait(0, wait_reader=False)

    def processOutput(self, timeout=None, outputTimeout=None):
        """
        Handle process output until the process terminates or times out.

        :param timeout: If not None, the process will be allowed to continue
        for that number of seconds before being killed.

        :outputTimeout: If not None, the process will be allowed to continue
        for that number of seconds without producing any output before
        being killed.
        """
        # this method is kept for backward compatibility
        if not hasattr(self, "proc"):
            self.run(timeout=timeout, outputTimeout=outputTimeout)
            # self.run will call this again
            return
        if not self.reader.is_alive():
            self.reader.timeout = timeout
            self.reader.output_timeout = outputTimeout
            self.reader.start(self.proc)

    def run(self, timeout=None, outputTimeout=None):
        """
        Starts the process.

        :param timeout: If not None, the process will be allowed to continue for
        that number of seconds before being killed. If the process is killed
        due to a timeout, the onTimeout handler will be called.

        :outputTimeout: If not None, the process will be allowed to continue
        for that number of seconds without producing any output before
        being killed.
        """
        self.didTimeout = False
        self.didOutputTimeout = False

        # default arguments
        args = dict(
            stdout=subprocess.PIPE,
            stderr=self._stderr,
            cwd=self.cwd,
            env=self.env,
        )

        # build process arguments
        args.update(self.keywordargs)

        # launch the process
        self.proc = psutil.Popen([self.cmd] + self.args, **args)

        self.processOutput(timeout=timeout, outputTimeout=outputTimeout)

    @_has_valid_proc
    def update_process(self, new_pid, timeout=None):
        """Update the internally managed process for the provided process ID.

        When the application restarts itself, such as during an update, the new
        process is essentially a fork of itself. To continue monitoring this
        process, the process ID needs to be updated accordingly.

        :param new_pid: The ID of the new (forked) process to track.

        :timeout: If not None, the old process will be allowed to continue for
        that number of seconds before being killed.
        """
        if isPosix:
            if new_pid == self.pid:
                return

            print(
                'Child process with id "%s" has been marked as detached because it is no '
                "longer in the managed process group. Keeping reference to the process id "
                '"%s" which is the new child process.' % (self.pid, new_pid),
                file=sys.stdout,
            )

            returncode = self.wait(timeout, wait_reader=False)
            if returncode is None:
                # If the process is still running force kill it.
                returncode = self.kill()

            if hasattr(self, "returncode"):
                del self.returncode

            self.proc = psutil.Process(new_pid)
            self._debug(
                f"New process status: {self.proc} (terminal={self.proc.terminal()})"
            )

            return returncode

    @_has_valid_proc
    def wait(self, timeout=None, wait_reader=True):
        """
        Waits until the process is terminated.

        :param timeout: If not None, will return after timeout seconds.
        This timeout only causes the wait function to return and
        does not kill the process.

        :param wait_reader: If set to True, it waits not only for the process
        to exit but also for all output to be fully read. (Defaults to True).

        Returns the process exit code value:
        - None if the process hasn't terminated yet
        - A negative number if the process was killed by signal N (Unix only)
        - '0' if the process ended without failures

        """
        # Thread.join() blocks the main thread until the reader thread is finished
        # wake up once a second in case a keyboard interrupt is sent
        if (
            wait_reader
            and self.reader.thread
            and self.reader.thread is not threading.current_thread()
        ):
            count = 0
            while self.reader.is_alive():
                if timeout is not None and count > timeout:
                    self._debug("wait timeout for reader thread")
                    return None
                self.reader.join(timeout=1)
                count += 1

        try:
            self.proc.wait(timeout)
            self._debug(f"Process status after wait: {self.proc}")

            if not isinstance(self.proc, psutil.Popen):
                self._debug(
                    "Not self-managed processes do not have a returncode. "
                    f"Setting its value to {UNKNOWN_RETURNCODE}."
                )
                self.returncode = UNKNOWN_RETURNCODE

            else:
                self.returncode = self.proc.returncode

            return self.returncode
        except psutil.TimeoutExpired:
            return None


class CallableList(list):
    def __call__(self, *args, **kwargs):
        for e in self:
            e(*args, **kwargs)

    def __add__(self, lst):
        return CallableList(list.__add__(self, lst))


class ProcessReader(object):
    def __init__(
        self,
        stdout_callback=None,
        stderr_callback=None,
        finished_callback=None,
        timeout_callback=None,
        timeout=None,
        output_timeout=None,
    ):
        self.stdout_callback = stdout_callback or (lambda line: True)
        self.stderr_callback = stderr_callback or (lambda line: True)
        self.finished_callback = finished_callback or (lambda: True)
        self.timeout_callback = timeout_callback or (lambda: True)
        self.timeout = timeout
        self.output_timeout = output_timeout
        self.thread = None
        self.didOutputTimeout = False

    def debug(self, msg):
        if not MOZPROCESS_DEBUG:
            return

        print("DBG::MARIONETTE ProcessReader | {}".format(msg), file=sys.stdout)

    def _create_stream_reader(self, name, stream, queue, callback):
        thread = threading.Thread(
            name=name, target=self._read_stream, args=(stream, queue, callback)
        )
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
            stdout_reader = self._create_stream_reader(
                "ProcessReaderStdout", proc.stdout, queue, self.stdout_callback
            )
        stderr_reader = None
        if proc.stderr and proc.stderr != proc.stdout:
            stderr_reader = self._create_stream_reader(
                "ProcessReaderStderr", proc.stderr, queue, self.stderr_callback
            )
        self.thread = threading.Thread(
            name="ProcessReader",
            target=self._read,
            args=(stdout_reader, stderr_reader, queue),
        )
        self.thread.daemon = True
        self.thread.start()
        self.debug("ProcessReader started")

    def _read(self, stdout_reader, stderr_reader, queue):
        start_time = time.time()
        timed_out = False
        timeout = self.timeout
        if timeout is not None:
            timeout += start_time
        output_timeout = self.output_timeout
        if output_timeout is not None:
            output_timeout += start_time

        while (stdout_reader and stdout_reader.is_alive()) or (
            stderr_reader and stderr_reader.is_alive()
        ):
            has_line = True
            try:
                line, callback = queue.get(True, INTERVAL_PROCESS_ALIVE_CHECK)
            except Empty:
                has_line = False
            now = time.time()
            if not has_line:
                if output_timeout is not None and now > output_timeout:
                    timed_out = True
                    self.didOutputTimeout = True
                    break
            else:
                if output_timeout is not None:
                    output_timeout = now + self.output_timeout
                callback(line.rstrip())
            if timeout is not None and now > timeout:
                timed_out = True
                break
        self.debug("_read loop exited")
        # process remaining lines to read
        while not queue.empty():
            line, callback = queue.get(False)
            try:
                callback(line.rstrip())
            except Exception:
                traceback.print_exc()
        if timed_out:
            try:
                self.timeout_callback()
            except Exception:
                traceback.print_exc()
        if stdout_reader:
            stdout_reader.join()
        if stderr_reader:
            stderr_reader.join()
        if not timed_out:
            try:
                self.finished_callback()
            except Exception:
                traceback.print_exc()
        self.debug("_read exited")

    def is_alive(self):
        if self.thread:
            return self.thread.is_alive()
        return False

    def join(self, timeout=None):
        if self.thread:
            self.thread.join(timeout=timeout)


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

    def __init__(self, stream, text=True):
        self.stream = stream
        self.text = text

    def __call__(self, line):
        ensure = six.ensure_text if self.text else six.ensure_binary
        try:
            self.stream.write(ensure(line, errors="ignore") + ensure("\n"))
        except TypeError:
            print(
                "HEY! If you're reading this, you're about to encounter a "
                "type error, probably as a result of a conversion from "
                "Python 2 to Python 3. This is almost definitely because "
                "you're trying to write binary data to a text-encoded "
                "stream, or text data to a binary-encoded stream. Check how "
                "you're instantiating your ProcessHandler and if the output "
                "should be text-encoded, make sure you pass "
                "universal_newlines=True.",
                file=sys.stderr,
            )
            raise
        self.stream.flush()


class LogOutput(StreamOutput):
    """pass output to a file"""

    def __init__(self, filename):
        self.file_obj = open(filename, "a")
        StreamOutput.__init__(self, self.file_obj, True)

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

    def __init__(self, cmd, logfile=None, stream=True, storeOutput=True, **kwargs):
        kwargs.setdefault("processOutputLine", [])
        if callable(kwargs["processOutputLine"]):
            kwargs["processOutputLine"] = [kwargs["processOutputLine"]]

        if logfile:
            logoutput = LogOutput(logfile)
            kwargs["processOutputLine"].append(logoutput)

        text = kwargs.get("universal_newlines", False) or kwargs.get("text", False)

        if stream is True:
            if text:
                # The encoding of stdout isn't guaranteed to be utf-8. Fix that.
                stdout = codecs.getwriter("utf-8")(sys.stdout.buffer)
            else:
                stdout = sys.stdout.buffer

            if not kwargs["processOutputLine"]:
                kwargs["processOutputLine"].append(StreamOutput(stdout, text))
        elif stream:
            streamoutput = StreamOutput(stream, text)
            kwargs["processOutputLine"].append(streamoutput)

        self.output = None
        if storeOutput:
            storeoutput = StoreOutput()
            self.output = storeoutput.output
            kwargs["processOutputLine"].append(storeoutput)

        ProcessHandlerMixin.__init__(self, cmd, **kwargs)
