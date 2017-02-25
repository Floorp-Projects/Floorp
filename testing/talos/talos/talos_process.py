# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import time
import psutil
import mozcrash
import traceback
from mozprocess import ProcessHandler
from threading import Event

from mozlog import get_proxy_logger

from utils import TalosError

LOG = get_proxy_logger()


class ProcessContext(object):
    """
    Store useful results of the browser execution.
    """
    def __init__(self):
        self.output = None
        self.process = None

    @property
    def pid(self):
        return self.process and self.process.pid

    def kill_process(self):
        """
        Kill the process, returning the exit code or None if the process
        is already finished.
        """
        if self.process and self.process.is_running():
            LOG.debug("Terminating %s" % self.process)
            try:
                self.process.terminate()
            except psutil.NoSuchProcess:
                procs = self.process.children()
                for p in procs:
                    c = ProcessContext()
                    c.process = p
                    c.kill_process()
                return self.process.returncode
            try:
                return self.process.wait(3)
            except psutil.TimeoutExpired:
                self.process.kill()
                # will raise TimeoutExpired if unable to kill
                return self.process.wait(3)


class Reader(object):
    def __init__(self, event):
        self.output = []
        self.got_end_timestamp = False
        self.got_timeout = False
        self.timeout_message = ''
        self.event = event
        self.proc = None

    def __call__(self, line):
        if line.find('__endTimestamp') != -1:
            self.got_end_timestamp = True
            self.event.set()
        elif line == 'TART: TIMEOUT':
            self.got_timeout = True
            self.timeout_message = 'TART'
            self.event.set()

        if not (line.startswith('JavaScript error:') or
                line.startswith('JavaScript warning:')):
            LOG.process_output(self.proc.pid, line)
            self.output.append(line)


def run_browser(command, minidump_dir, timeout=None, on_started=None,
                **kwargs):
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
    :param kwargs: additional keyword arguments for the :class:`ProcessHandler`
                   instance

    Returns a ProcessContext instance, with available output and pid used.
    """
    context = ProcessContext()
    first_time = int(time.time()) * 1000
    wait_for_quit_timeout = 5
    event = Event()
    reader = Reader(event)

    kwargs['storeOutput'] = False
    kwargs['processOutputLine'] = reader
    kwargs['onFinish'] = event.set
    proc = ProcessHandler(command, **kwargs)
    reader.proc = proc
    proc.run()
    LOG.process_start(proc.pid, ' '.join(command))
    try:
        context.process = psutil.Process(proc.pid)
        if on_started:
            on_started(context.process)
        # wait until we saw __endTimestamp in the proc output,
        # or the browser just terminated - or we have a timeout
        if not event.wait(timeout):
            LOG.info("Timeout waiting for test completion; killing browser...")
            # try to extract the minidump stack if the browser hangs
            mozcrash.kill_and_get_minidump(proc.pid, minidump_dir)
            raise TalosError("timeout")
        if reader.got_end_timestamp:
            for i in range(1, wait_for_quit_timeout):
                if proc.wait(1) is not None:
                    break
            if proc.poll() is None:
                LOG.info(
                    "Browser shutdown timed out after {0} seconds, terminating"
                    " process.".format(wait_for_quit_timeout)
                )
        elif reader.got_timeout:
            raise TalosError('TIMEOUT: %s' % reader.timeout_message)
    finally:
        # this also handle KeyboardInterrupt
        # ensure early the process is really terminated
        try:
            return_code = context.kill_process()
            if return_code is None:
                return_code = proc.wait(1)
        except:
            # Maybe killed by kill_and_get_minidump(), maybe ended?
            LOG.info("Unable to kill process")
            LOG.info(traceback.format_exc())

    reader.output.append(
        "__startBeforeLaunchTimestamp%d__endBeforeLaunchTimestamp"
        % first_time)
    reader.output.append(
        "__startAfterTerminationTimestamp%d__endAfterTerminationTimestamp"
        % (int(time.time()) * 1000))

    if return_code is not None:
        LOG.process_exit(proc.pid, return_code)
    else:
        LOG.debug("Unable to detect exit code of the process %s." % proc.pid)
    context.output = reader.output
    return context
