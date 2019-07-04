#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from mozlog.proxy import ProxyLogger


class RaptorLogger():

    def __init__(self, component=None):
        self.logger = ProxyLogger(component)

    def exception(self, message):
        self.critical(message)

    def debug(self, message):
        return self.logger.debug("Debug: {}".format(message))

    def info(self, message):
        return self.logger.info("Info: {}".format(message))

    def warning(self, message):
        return self.logger.warning("Warning: {}".format(message))

    def error(self, message):
        return self.logger.error("Error: {}".format(message))

    def critical(self, message):
        return self.logger.critical("Critical: {}".format(message))

    def log_raw(self, message):
        return self.logger.log_raw(message)

    def process_output(self, *args, **kwargs):
        return self.logger.process_output(*args, **kwargs)
