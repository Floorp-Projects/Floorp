# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import logging

logging.basicConfig(level=logging.INFO)
logger = None


class NotebookLogger(object):
    """Simple logger for perftest-notebook.
    """

    debug = False

    def __init__(self, name="perftest-notebook"):
        self._logger = logger
        if not self._logger:
            self._logger = logging.getLogger(name)

    def debug(self, msg):
        if self.debug:
            self._logger.info(msg)

    def info(self, msg):
        self._logger.info(msg)

    def warning(self, msg):
        self._logger.warning(msg)

    def error(self, msg):
        self._logger.error(msg)

    def critical(self, msg):
        self._logger.critical(msg)
