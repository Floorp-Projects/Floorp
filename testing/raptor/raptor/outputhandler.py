# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# originally from talos_process.py
import json

from logger.logger import RaptorLogger

LOG = RaptorLogger(component="raptor-output-handler")


class OutputHandler(object):
    def __init__(self, verbose=False):
        self.proc = None
        self.verbose = verbose

    def __call__(self, line):
        if not line.strip():
            return
        line = line.decode("utf-8", errors="replace")

        try:
            data = json.loads(line)
        except ValueError:
            self.process_output(line)
            return

        if isinstance(data, dict) and "action" in data:
            LOG.log_raw(data)
        else:
            self.process_output(json.dumps(data))

    def process_output(self, line):
        if "raptor" in line or self.verbose:
            LOG.process_output(self.proc.pid, line)
