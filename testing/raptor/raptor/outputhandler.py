# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# originally from talos_process.py
from __future__ import absolute_import

import json

from mozlog import get_proxy_logger


LOG = get_proxy_logger(component='raptor-output-handler')


class OutputHandler(object):
    def __init__(self):
        self.proc = None

    def __call__(self, line):
        if not line.strip():
            return
        line = line.decode('utf-8', errors='replace')

        try:
            data = json.loads(line)
        except ValueError:
            self.process_output(line)
            return

        if isinstance(data, dict) and 'action' in data:
            LOG.log_raw(data)
        else:
            self.process_output(json.dumps(data))

    def process_output(self, line):
        if "error" in line or "warning" in line or "raptor" in line:
            LOG.process_output(self.proc.pid, line)
