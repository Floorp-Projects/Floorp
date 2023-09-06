# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import codecs
import locale
from threading import Lock

import six

from mozlog.handlers.messagehandler import MessageHandler
from mozlog.structuredlog import log_levels


class BaseHandler(object):
    """A base handler providing message handling facilities to
    derived classes.
    """

    def __init__(self, inner):
        self.message_handler = MessageHandler()
        if hasattr(inner, "message_handler"):
            self.message_handler.wrapped.append(inner)


class LogLevelFilter(BaseHandler):
    """Handler that filters out messages with action of log and a level
    lower than some specified level.

    :param inner: Handler to use for messages that pass this filter
    :param level: Minimum log level to process
    """

    def __init__(self, inner, level):
        BaseHandler.__init__(self, inner)
        self.inner = inner
        self.level = log_levels[level.upper()]

    def __call__(self, item):
        if item["action"] != "log" or log_levels[item["level"].upper()] <= self.level:
            return self.inner(item)


class StreamHandler(BaseHandler):
    """Handler for writing to a file-like object

    :param stream: File-like object to write log messages to
    :param formatter: formatter to convert messages to string format
    """

    _lock = Lock()

    def __init__(self, stream, formatter):
        BaseHandler.__init__(self, formatter)
        assert stream is not None
        if six.PY2:
            # This is a hack to deal with the case where we are passed a
            # StreamWriter (e.g. by mach for stdout). A StreamWriter requires
            # the code to handle unicode in exactly the opposite way compared
            # to a normal stream i.e. you always have to pass in a Unicode
            # object rather than a string object. Cope with that by extracting
            # the underlying raw stream.
            if isinstance(stream, codecs.StreamWriter):
                stream = stream.stream

        self.formatter = formatter
        self.stream = stream

    def __call__(self, data):
        """Write a log message.

        :param data: Structured log message dictionary."""
        formatted = self.formatter(data)
        if not formatted:
            return
        with self._lock:
            if six.PY3:
                import io

                import mozfile

                source_enc = "utf-8"
                target_enc = "utf-8"
                if isinstance(self.stream, io.BytesIO):
                    target_enc = None
                    if hasattr(self.stream, "encoding"):
                        target_enc = self.stream.encoding
                if target_enc is None:
                    target_enc = locale.getpreferredencoding()

                if isinstance(self.stream, io.StringIO) and isinstance(
                    formatted, bytes
                ):
                    formatted = formatted.decode(source_enc, "replace")
                elif (
                    isinstance(self.stream, io.BytesIO)
                    or isinstance(self.stream, mozfile.NamedTemporaryFile)
                ) and isinstance(formatted, str):
                    formatted = formatted.encode(target_enc, "replace")
                elif isinstance(formatted, str):
                    # Suppress eventual surrogates, but keep as string.
                    # TODO: It is yet unclear how we can end up with
                    # surrogates here, see comment on patch on bug 1794401.
                    formatted_bin = formatted.encode(target_enc, "replace")
                    formatted = formatted_bin.decode(target_enc, "ignore")

                # It seems that under Windows we can have cp1252 encoding
                # for the output stream and that not all unicode chars map
                # well. We just ignore those errors here (they have no
                # consequences for the executed tests, anyways).
                try:
                    self.stream.write(formatted)
                except UnicodeEncodeError:
                    return
            else:
                if isinstance(formatted, six.text_type):
                    self.stream.write(formatted.encode("utf-8", "replace"))
                elif isinstance(formatted, str):
                    self.stream.write(formatted)
                else:
                    assert False, "Got output from the formatter of an unexpected type"

            self.stream.flush()
