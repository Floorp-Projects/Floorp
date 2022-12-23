#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozlog.proxy import ProxyLogger


class RaptorLogger:
    app = ""

    def __init__(self, component=None):
        self.logger = ProxyLogger(component)

    def set_app(self, app_name):
        """Sets the app name to prefix messages with."""
        RaptorLogger.app = " [" + app_name + "]"

    def exception(self, message, **kwargs):
        self.critical(message, **kwargs)

    def debug(self, message, **kwargs):
        return self.logger.debug("Debug: {}".format(message), **kwargs)

    def info(self, message, **kwargs):
        return self.logger.info("Info: {}".format(message), **kwargs)

    def warning(self, message, **kwargs):
        return self.logger.warning(
            "Warning:{} {}".format(RaptorLogger.app, message), **kwargs
        )

    def error(self, message, **kwargs):
        return self.logger.error(
            "Error:{} {}".format(RaptorLogger.app, message), **kwargs
        )

    def critical(self, message, **kwargs):
        return self.logger.critical(
            "Critical:{} {}".format(RaptorLogger.app, message), **kwargs
        )

    def log_raw(self, message, **kwargs):
        return self.logger.log_raw(message, **kwargs)

    def process_output(self, *args, **kwargs):
        return self.logger.process_output(*args, **kwargs)

    def crash(self, *args, **kwargs):
        return self.logger.crash(*args, **kwargs)
