# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import signal
import subprocess
from abc import ABC, abstractmethod


class LintProcess(subprocess.Popen, ABC):
    def __init__(self, config, *args, **kwargs):
        self.config = config
        self.results = []

        kwargs["text"] = True
        kwargs["stdout"] = subprocess.PIPE
        kwargs["stderr"] = subprocess.STDOUT
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        subprocess.Popen.__init__(self, *args, **kwargs)
        signal.signal(signal.SIGINT, orig)

    @abstractmethod
    def process_line(self, line):
        """Process a single line of output.

        The implementation is responsible for creating one or more :class:`~mozlint.result.Issue`
        and storing them somewhere accessible.

        Args:
            line (str): The line of output to process.
        """
        pass

    def run(self):
        for line in self.stdout:
            self.process_line(line)
