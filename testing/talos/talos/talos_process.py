# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import time
import logging
import psutil
import mozcrash
from mozprocess import ProcessHandler
from threading import Event

from utils import TalosError


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
        if self.process and self.process.is_running():
            logging.debug("Terminating %s", self.process)
            self.process.terminate()
            try:
                self.process.wait(3)
            except psutil.TimeoutExpired:
                self.process.kill()
                # will raise TimeoutExpired if unable to kill
                self.process.wait(3)


class Reader(object):
    def __init__(self, event):
        self.output = []
        self.got_end_timestamp = False
        self.event = event

    def __call__(self, line):
        if line.find('__endTimestamp') != -1:
            self.got_end_timestamp = True
            self.event.set()

        if not (line.startswith('JavaScript error:') or
                line.startswith('JavaScript warning:')):
            logging.debug('BROWSER_OUTPUT: %s', line)
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
                       the browser has been started
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
    proc.run()
    try:
        context.process = psutil.Process(proc.pid)
        if on_started:
            on_started()
        # wait until we saw __endTimestamp in the proc output,
        # or the browser just terminated - or we have a timeout
        if not event.wait(timeout):
            # try to extract the minidump stack if the browser hangs
            mozcrash.kill_and_get_minidump(proc.pid, minidump_dir)
            raise TalosError("timeout")
        if reader.got_end_timestamp:
            for i in range(1, wait_for_quit_timeout):
                if proc.wait(1) is not None:
                    break
            if proc.poll() is None:
                logging.info(
                    "Browser shutdown timed out after {0} seconds, terminating"
                    " process.".format(wait_for_quit_timeout)
                )
    finally:
        # this also handle KeyboardInterrupt
        # ensure early the process is really terminated
        context.kill_process()
        return_code = proc.wait(1)

    reader.output.append(
        "__startBeforeLaunchTimestamp%d__endBeforeLaunchTimestamp"
        % first_time)
    reader.output.append(
        "__startAfterTerminationTimestamp%d__endAfterTerminationTimestamp"
        % (int(time.time()) * 1000))

    logging.info("Browser exited with error code: {0}".format(return_code))
    context.output = reader.output
    return context
