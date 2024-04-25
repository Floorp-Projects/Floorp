# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains logging functionality for mach. It essentially provides
# support for a structured logging framework built on top of Python's built-in
# logging framework.

import codecs
import json
import logging
import os
import sys
import time

import blessed
import six
from mozbuild.buildversion import mozilla_build_version
from packaging.version import Version

IS_WINDOWS = sys.platform.startswith("win")

if IS_WINDOWS:
    import msvcrt
    from ctypes import byref, windll
    from ctypes.wintypes import DWORD

    ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004

    def enable_virtual_terminal_processing(file_descriptor):
        handle = msvcrt.get_osfhandle(file_descriptor)
        try:
            mode = DWORD()
            windll.kernel32.GetConsoleMode(handle, byref(mode))
            mode.value |= ENABLE_VIRTUAL_TERMINAL_PROCESSING
            windll.kernel32.SetConsoleMode(handle, mode.value)
        except Exception as e:
            raise e


def enable_blessed():
    # Only Windows has issues with enabling blessed
    # and interpreting ANSI escape sequences
    if not IS_WINDOWS:
        return True

    if os.environ.get("NO_ANSI"):
        return False

    if not os.environ.get("MOZILLABUILD"):
        return False

    # MozillaBuild 4.0.2 is the first Release that supports
    # ANSI escape sequences, so if we're greater than that
    # version, we can enable them (via Blessed).
    return mozilla_build_version() >= Version("4.0.2")


# stdout and stderr may not necessarily be set up to write Unicode output, so
# reconfigure them if necessary.
def _wrap_stdstream(fh):
    if fh in (sys.stderr, sys.stdout):
        encoding = sys.getdefaultencoding()
        encoding = "utf-8" if encoding in ("ascii", "charmap") else encoding
        if six.PY2:
            return codecs.getwriter(encoding)(fh, errors="replace")
        else:
            return codecs.getwriter(encoding)(fh.buffer, errors="replace")
    else:
        return fh


def format_seconds(total):
    """Format number of seconds to MM:SS.DD form."""

    minutes, seconds = divmod(total, 60)

    return "%2d:%05.2f" % (minutes, seconds)


def format_level(level, terminal=None):
    levels = {
        logging.NOTSET: ("N", "bright_white"),
        logging.DEBUG: ("D", "blue"),
        logging.INFO: (None, None),
        logging.WARNING: ("W", "yellow"),
        logging.ERROR: ("E", "red"),
        logging.CRITICAL: ("C", "black_on_red"),
    }
    try:
        s, color = levels[level]
    except KeyError:
        raise ValueError(f"unsupported log level: {level}")
    if s is None:
        return ""
    colorfunc = getattr(terminal, color) if terminal else lambda x: x
    return colorfunc(s) + " "


class ConvertToStructuredFilter(logging.Filter):
    """Filter that converts unstructured records into structured ones."""

    def filter(self, record):
        if hasattr(record, "action") and hasattr(record, "params"):
            return True

        record.action = "unstructured"
        record.params = {"msg": record.getMessage()}
        record.msg = "{msg}"

        return True


class StructuredJSONFormatter(logging.Formatter):
    """Log formatter that writes a structured JSON entry."""

    def format(self, record):
        action = getattr(record, "action", "UNKNOWN")
        params = getattr(record, "params", {})

        return json.dumps([record.created, action, params])


class StructuredHumanFormatter(logging.Formatter):
    """Log formatter that writes structured messages for humans.

    It is important that this formatter never be added to a logger that
    produces unstructured/classic log messages. If it is, the call to format()
    could fail because the string could contain things (like JSON) that look
    like formatting character sequences.

    Because of this limitation, format() will fail with a KeyError if an
    unstructured record is passed or if the structured message is malformed.
    """

    def __init__(
        self, start_time, write_interval=False, write_times=True, write_level=None
    ):
        self.start_time = start_time
        self.write_interval = write_interval
        self.write_times = write_times
        # Default to the same value as `write_times` if `write_level` is unset.
        self.write_level = write_level if write_level is not None else write_times
        self.last_time = None

    def format(self, record):
        formatted_msg = record.msg.format(**getattr(record, "params", {}))

        elapsed_time = (
            format_seconds(self._time(record)) + " " if self.write_times else ""
        )

        level = format_level(record.levelno) if self.write_level else ""

        rv = elapsed_time + level + formatted_msg
        formatted_stack_trace_result = formatted_stack_trace(record, self)

        if formatted_stack_trace_result != "":
            stack_trace = "\n" + elapsed_time + formatted_stack_trace_result
            rv += stack_trace.replace("\n", f"\n{elapsed_time}")

        return rv

    def _time(self, record):
        t = record.created - self.start_time

        if self.write_interval and self.last_time is not None:
            t = record.created - self.last_time

        self.last_time = record.created

        return t


class StructuredTerminalFormatter(StructuredHumanFormatter):
    """Log formatter for structured messages writing to a terminal."""

    def set_terminal(self, terminal):
        self.terminal = terminal
        self._sgr0 = terminal.normal if terminal else ""

    def format(self, record):
        formatted_msg = record.msg.format(**getattr(record, "params", {}))
        elapsed_time = (
            self.terminal.blue(format_seconds(self._time(record))) + " "
            if self.write_times
            else ""
        )

        level = format_level(record.levelno, self.terminal) if self.write_level else ""

        rv = elapsed_time + level + self._colorize(formatted_msg) + self._sgr0
        formatted_stack_trace_result = formatted_stack_trace(record, self)

        if formatted_stack_trace_result != "":
            stack_trace = "\n" + elapsed_time + formatted_stack_trace_result
            rv += stack_trace.replace("\n", f"\n{elapsed_time}")

        # Some processes (notably Clang) don't reset terminal attributes after
        # printing newlines. This can lead to terminal attributes getting in a
        # wonky state. Work around this by sending the sgr0 sequence after every
        # line to reset all attributes. For programs that rely on the next line
        # inheriting the same attributes, this will prevent that from happening.
        # But that's better than "corrupting" the terminal.
        return rv + self._sgr0

    def _colorize(self, s):
        if not self.terminal:
            return s

        result = s

        reftest = s.startswith("REFTEST ")
        if reftest:
            s = s[8:]

        if s.startswith("TEST-PASS"):
            result = self.terminal.green(s[0:9]) + s[9:]
        elif s.startswith("TEST-UNEXPECTED"):
            result = self.terminal.red(s[0:20]) + s[20:]
        elif s.startswith("TEST-START"):
            result = self.terminal.yellow(s[0:10]) + s[10:]
        elif s.startswith("TEST-INFO"):
            result = self.terminal.yellow(s[0:9]) + s[9:]

        if reftest:
            result = "REFTEST " + result

        return result


def formatted_stack_trace(record, formatter):
    """
    Formatting behavior here intended to mimic a portion of the
    standard library's logging.Formatter::format function
    """
    rv = ""

    if record.exc_info:
        # Cache the traceback text to avoid converting it multiple times
        # (it's constant anyway)
        if not record.exc_text:
            record.exc_text = formatter.formatException(record.exc_info)
    if record.exc_text:
        rv = record.exc_text
    if record.stack_info:
        if rv[-1:] != "\n":
            rv = rv + "\n"
        rv = rv + formatter.formatStack(record.stack_info)

    return rv


class LoggingManager(object):
    """Holds and controls global logging state.

    An application should instantiate one of these and configure it as needed.

    This class provides a mechanism to configure the output of logging data
    both from mach and from the overall logging system (e.g. from other
    modules).
    """

    def __init__(self):
        self.start_time = time.time()

        self.json_handlers = []
        self.terminal_handler = None
        self.terminal_formatter = None

        self.root_logger = logging.getLogger()
        self.root_logger.setLevel(logging.DEBUG)

        # Installing NullHandler on the root logger ensures that *all* log
        # messages have at least one handler. This prevents Python from
        # complaining about "no handlers could be found for logger XXX."
        self.root_logger.addHandler(logging.NullHandler())

        mach_logger = logging.getLogger("mach")
        mach_logger.setLevel(logging.DEBUG)

        self.structured_filter = ConvertToStructuredFilter()

        self.structured_loggers = [mach_logger]

        self._terminal = None

    def create_terminal(self):
        if enable_blessed():
            # Sometimes blessed fails to set up the terminal, in that case, silently fail.
            try:
                terminal = blessed.Terminal(stream=_wrap_stdstream(sys.stdout))

                if terminal.is_a_tty:
                    self._terminal = terminal
            except Exception:
                pass

    @property
    def terminal(self):
        return self._terminal

    def add_json_handler(self, fh):
        """Enable JSON logging on the specified file object."""

        # Configure the consumer of structured messages.
        handler = logging.StreamHandler(stream=fh)
        handler.setFormatter(StructuredJSONFormatter())
        handler.setLevel(logging.DEBUG)

        # And hook it up.
        for logger in self.structured_loggers:
            logger.addHandler(handler)

        self.json_handlers.append(handler)

    def add_terminal_logging(
        self, fh=sys.stdout, level=logging.INFO, write_interval=False, write_times=True
    ):
        """Enable logging to the terminal."""
        self.create_terminal()

        if IS_WINDOWS:
            try:
                # fileno() can raise in some cases, like unit tests.
                # so we can try to enable this but if we fail it's fine
                enable_virtual_terminal_processing(sys.stdout.fileno())
                enable_virtual_terminal_processing(sys.stderr.fileno())
            except Exception:
                pass

        fh = _wrap_stdstream(fh)
        formatter = StructuredHumanFormatter(
            self.start_time, write_interval=write_interval, write_times=write_times
        )

        if self.terminal:
            formatter = StructuredTerminalFormatter(
                self.start_time, write_interval=write_interval, write_times=write_times
            )
            formatter.set_terminal(self.terminal)

        handler = logging.StreamHandler(stream=fh)
        handler.setFormatter(formatter)
        handler.setLevel(level)

        for logger in self.structured_loggers:
            logger.addHandler(handler)

        self.terminal_handler = handler
        self.terminal_formatter = formatter

    def replace_terminal_handler(self, handler):
        """Replace the installed terminal handler.

        Returns the old handler or None if none was configured.
        If the new handler is None, removes any existing handler and disables
        logging to the terminal.
        """
        old = self.terminal_handler

        if old:
            for logger in self.structured_loggers:
                logger.removeHandler(old)

        if handler:
            for logger in self.structured_loggers:
                logger.addHandler(handler)

        self.terminal_handler = handler

        return old

    def enable_unstructured(self):
        """Enable logging of unstructured messages."""
        if self.terminal_handler:
            self.terminal_handler.addFilter(self.structured_filter)
            self.root_logger.addHandler(self.terminal_handler)

    def disable_unstructured(self):
        """Disable logging of unstructured messages."""
        if self.terminal_handler:
            self.terminal_handler.removeFilter(self.structured_filter)
            self.root_logger.removeHandler(self.terminal_handler)

    def register_structured_logger(self, logger, terminal=True, json=True):
        """Register a structured logger.

        This needs to be called for all structured loggers that don't chain up
        to the mach logger in order for their output to be captured.
        """
        self.structured_loggers.append(logger)

        if terminal and self.terminal_handler:
            logger.addHandler(self.terminal_handler)

        if json:
            for handler in self.json_handlers:
                logger.addHandler(handler)

    def enable_all_structured_loggers(self, terminal=True, json=True):
        """Enable logging of all structured messages from all loggers.

        ``terminal`` and ``json`` determine which log handlers to operate
        on. By default, all known handlers are operated on.
        """

        # Glean makes logs that we're not interested in, so we squelch them.
        logging.getLogger("glean").setLevel(logging.CRITICAL)

        # Remove current handlers from all loggers so we don't double
        # register handlers.
        for logger in self.root_logger.manager.loggerDict.values():
            # Some entries might be logging.PlaceHolder.
            if not isinstance(logger, logging.Logger):
                continue

            if terminal:
                logger.removeHandler(self.terminal_handler)

            if json:
                for handler in self.json_handlers:
                    logger.removeHandler(handler)

        # Wipe out existing registered structured loggers since they
        # all propagate to root logger.
        self.structured_loggers = []
        self.register_structured_logger(self.root_logger, terminal=terminal, json=json)
