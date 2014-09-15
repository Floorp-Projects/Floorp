# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from collections import defaultdict
from mozlog.structured.structuredlog import log_levels

class StatusHandler(object):
    """A handler used to determine an overall status for a test run according
    to a sequence of log messages."""

    def __init__(self):
        # The count of unexpected results (includes tests and subtests)
        self.unexpected = 0
        # The count of skip results (includes tests and subtests)
        self.skipped = 0
        # The count of top level tests run (test_end messages)
        self.test_count = 0
        # The count of messages logged at each log level
        self.level_counts = defaultdict(int)


    def __call__(self, data):
        action = data['action']
        if action == 'log':
            self.level_counts[data['level']] += 1

        if action == 'test_end':
            self.test_count += 1

        if action in ('test_status', 'test_end'):
            if 'expected' in data:
                self.unexpected += 1

            if data['status'] == 'SKIP':
                self.skipped += 1


    def evaluate(self):
        status = 'OK'

        if self.unexpected:
            status = 'FAIL'

        if not self.test_count:
            status = 'ERROR'

        for level in self.level_counts:
            if log_levels[level] <= log_levels['ERROR']:
                status = 'ERROR'

        summary = {
            'status': status,
            'unexpected': self.unexpected,
            'skipped': self.skipped,
            'test_count': self.test_count,
            'level_counts': dict(self.level_counts),
        }
        return summary
