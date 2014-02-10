# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from threading import Lock

from ..structuredlog import log_levels


class BaseHandler(object):
    def __init__(self, formatter=str):
        self.formatter = formatter
        self.filters = []

    def add_filter(self, filter_func):
        self.filters.append(filter_func)

    def remove_filter(self, filter_func):
        self.filters.remove(filter_func)

    def filter(self, data):
        return all(item(data) for item in self.filters)


class LogLevelFilter(object):
    """Handler that filters out messages with action:log and a level
    lower than some specified level.

    :param inner: Handler to use for messages that pass this filter
    :param level: Minimum log level to process
    """
    def __init__(self, inner, level):
        self.inner = inner
        self.level = log_levels[level.upper()]

    def __call__(self, item):
        if (item["action"] != "log" or
            log_levels[item["level"]] <= self.level):
            return self.inner(item)


class StreamHandler(BaseHandler):
    """Handler for writing to a file-like object

    :param stream: File-like object to write log messages to
    :param formatter: formatter to convert messages to string format
    """

    _lock = Lock()

    def __init__(self,  stream, formatter):
        assert stream is not None
        self.stream = stream
        BaseHandler.__init__(self, formatter)

    def __call__(self, data):
        """Write a log message.

        :param data: Structured log message dictionary."""
        formatted = self.formatter(data)
        if not formatted:
            return
        with self._lock:
            #XXX Should encoding be the formatter's responsibility?
            try:
                self.stream.write(formatted.encode("utf8"))
            except:
                raise
            self.stream.flush()
