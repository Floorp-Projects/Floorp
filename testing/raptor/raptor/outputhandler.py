# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# originally from talos_process.py
from __future__ import absolute_import

import json
import time
from threading import Thread

from mozlog import get_proxy_logger


LOG = get_proxy_logger(component='raptor_process')


class OutputHandler(object):
    def __init__(self):
        self.proc = None
        self.kill_thread = Thread(target=self.wait_for_quit)
        self.kill_thread.daemon = True

    def __call__(self, line):
        if not line.strip():
            return
        line = line.decode('utf-8', errors='replace')

        try:
            data = json.loads(line)
        except ValueError:
            if line.find('__raptor_shutdownBrowser') != -1:
                self.kill_thread.start()
            self.process_output(line)
            return

        if isinstance(data, dict) and 'action' in data:
            LOG.log_raw(data)
        else:
            self.process_output(json.dumps(data))

    def process_output(self, line):
        LOG.process_output(self.proc.pid, line)

    def wait_for_quit(self, timeout=5):
        """Wait timeout seconds for the process to exit. If it hasn't
        exited by then, kill it.
        """
        time.sleep(timeout)
        if self.proc.poll() is None:
            self.proc.kill()
