# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
import pprint
import signal
import subprocess
import sys
import time
import traceback
from threading import Timer

import mozcrash
import psutil
import six
from mozlog import get_proxy_logger
from mozscreenshot import dump_screen

from talos.utils import TalosError

LOG = get_proxy_logger()


class ProcessContext(object):
    """
    Store useful results of the browser execution.
    """

    def __init__(self, is_launcher=False):
        self.output = None
        self.process = None
        self.is_launcher = is_launcher

    @property
    def pid(self):
        return self.process and self.process.pid

    def kill_process(self):
        """
        Kill the process, returning the exit code or None if the process
        is already finished.
        """
        parentProc = self.process
        # If we're using a launcher process, terminate that instead of us:
        kids = parentProc and parentProc.is_running() and parentProc.children()
        if self.is_launcher and kids and len(kids) == 1 and kids[0].is_running():
            LOG.debug(
                (
                    "Launcher process {} detected. Terminating parent"
                    " process {} instead."
                ).format(parentProc, kids[0])
            )
            parentProc = kids[0]

        if parentProc and parentProc.is_running():
            LOG.debug("Terminating %s" % parentProc)
            try:
                parentProc.terminate()
            except psutil.NoSuchProcess:
                procs = parentProc.children()
                for p in procs:
                    c = ProcessContext()
                    c.process = p
                    c.kill_process()
                return parentProc.returncode
            try:
                return parentProc.wait(3)
            except psutil.TimeoutExpired:
                LOG.debug("Killing %s" % parentProc)
                parentProc.kill()
                # will raise TimeoutExpired if unable to kill
                return parentProc.wait(3)


class Reader(object):
    def __init__(self):
        self.output = []
        self.got_end_timestamp = False
        self.got_timeout = False
        self.timeout_message = ""
        self.got_error = False
        self.proc = None

    def __call__(self, line):
        line = six.ensure_str(line)
        line = line.strip("\r\n")
        if line.find("__endTimestamp") != -1:
            self.got_end_timestamp = True
        elif line == "TART: TIMEOUT":
            self.got_timeout = True
            self.timeout_message = "TART"
        elif line.startswith("TEST-UNEXPECTED-FAIL | "):
            self.got_error = True

        if not (
            "JavaScript error:" in line
            or "JavaScript warning:" in line
            or "SyntaxError:" in line
            or "TypeError:" in line
        ):
            LOG.process_output(self.proc.pid, line)
            self.output.append(line)


def run_browser(
    command,
    minidump_dir,
    timeout=None,
    on_started=None,
    debug=None,
    debugger=None,
    debugger_args=None,
    utility_path=None,
    **kwargs
):
    """
    Run the browser using the given `command`.

    After the browser prints __endTimestamp, we give it 5
    seconds to quit and kill it if it's still alive at that point.

    Note that this method ensure that the process is killed at
    the end. If this is not possible, an exception will be raised.

    :param command: the commad (as a string list) to run the browser
    :param minidump_dir: a path where to extract minidumps in case the
                         browser hang. This have to be the same value
                         used in `mozcrash.check_for_crashes`.
    :param timeout: if specified, timeout to wait for the browser before
                    we raise a :class:`TalosError`
    :param on_started: a callback that can be used to do things just after
                       the browser has been started. The callback must takes
                       an argument, which is the psutil.Process instance
    :param kwargs: additional keyword arguments for the :class:`subprocess.Popen`
                   instance

    Returns a ProcessContext instance, with available output and pid used.
    """

    debugger_info = find_debugger_info(debug, debugger, debugger_args)
    if debugger_info is not None:
        return run_in_debug_mode(
            command, debugger_info, on_started=on_started, env=kwargs.get("env")
        )

    is_launcher = sys.platform.startswith("win") and "-wait-for-browser" in command
    context = ProcessContext(is_launcher)
    first_time = int(time.time()) * 1000
    wait_for_quit_timeout = 20
    reader = Reader()

    LOG.info("Using env: %s" % pprint.pformat(kwargs["env"]))

    timed_out = False

    def timeout_handler():
        nonlocal timed_out
        timed_out = True

    proc_timer = Timer(timeout, timeout_handler)
    proc_timer.start()

    proc = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=False, **kwargs
    )
    reader.proc = proc

    LOG.process_start(proc.pid, " ".join(command))
    try:
        context.process = psutil.Process(proc.pid)
        if on_started:
            on_started(context.process)

        # read output until the browser terminates or the timeout is hit
        for line in proc.stdout:
            reader(line)
            if timed_out:
                LOG.info("Timeout waiting for test completion; killing browser...")
                # try to extract the minidump stack if the browser hangs
                dump_screen_on_failure(utility_path)
                kill_and_get_minidump(context, minidump_dir)
                raise TalosError("timeout")
                break

        if reader.got_end_timestamp:
            proc.wait(wait_for_quit_timeout)
            if proc.poll() is None:
                LOG.info(
                    "Browser shutdown timed out after {0} seconds, killing"
                    " process.".format(wait_for_quit_timeout)
                )
                dump_screen_on_failure(utility_path)
                kill_and_get_minidump(context, minidump_dir)
                raise TalosError(
                    "Browser shutdown timed out after {0} seconds, killed"
                    " process.".format(wait_for_quit_timeout)
                )
        elif reader.got_timeout:
            dump_screen_on_failure(utility_path)
            raise TalosError("TIMEOUT: %s" % reader.timeout_message)
        elif reader.got_error:
            dump_screen_on_failure(utility_path)
            raise TalosError("unexpected error")
    finally:
        # this also handle KeyboardInterrupt
        # ensure early the process is really terminated
        proc_timer.cancel()
        return_code = None
        try:
            return_code = context.kill_process()
            if return_code is None:
                return_code = proc.wait(1)
        except Exception:
            # Maybe killed by kill_and_get_minidump(), maybe ended?
            LOG.info("Unable to kill process")
            LOG.info(traceback.format_exc())

    reader.output.append(
        "__startBeforeLaunchTimestamp%d__endBeforeLaunchTimestamp" % first_time
    )
    reader.output.append(
        "__startAfterTerminationTimestamp%d__endAfterTerminationTimestamp"
        % (int(time.time()) * 1000)
    )

    if return_code is not None:
        LOG.process_exit(proc.pid, return_code)
    else:
        LOG.debug("Unable to detect exit code of the process %s." % proc.pid)
    context.output = reader.output
    return context


def find_debugger_info(debug, debugger, debugger_args):
    debuggerInfo = None
    if debug or debugger or debugger_args:
        import mozdebug

        if not debugger:
            # No debugger name was provided. Look for the default ones on
            # current OS.
            debugger = mozdebug.get_default_debugger_name(
                mozdebug.DebuggerSearch.KeepLooking
            )

        debuggerInfo = None
        if debugger:
            debuggerInfo = mozdebug.get_debugger_info(debugger, debugger_args)

        if debuggerInfo is None:
            raise TalosError("Could not find a suitable debugger in your PATH.")

    return debuggerInfo


def run_in_debug_mode(command, debugger_info, on_started=None, env=None):
    signal.signal(signal.SIGINT, lambda sigid, frame: None)
    context = ProcessContext()
    command_under_dbg = [debugger_info.path] + debugger_info.args + command

    ttest_process = subprocess.Popen(command_under_dbg, env=env)

    context.process = psutil.Process(ttest_process.pid)
    if on_started:
        on_started(context.process)

    return_code = ttest_process.wait()

    if return_code is not None:
        LOG.process_exit(ttest_process.pid, return_code)
    else:
        LOG.debug("Unable to detect exit code of the process %s." % ttest_process.pid)

    return context


def kill_and_get_minidump(context, minidump_dir):
    proc = context.process
    if context.is_launcher:
        kids = context.process.children()
        if len(kids) == 1:
            LOG.debug(
                (
                    "Launcher process {} detected. Killing parent"
                    " process {} instead."
                ).format(proc, kids[0])
            )
            proc = kids[0]
    LOG.debug("Killing %s and writing a minidump file" % proc)
    mozcrash.kill_and_get_minidump(proc.pid, minidump_dir)


def dump_screen_on_failure(utility_path):
    if utility_path is not None:
        dump_screen(utility_path, LOG)
