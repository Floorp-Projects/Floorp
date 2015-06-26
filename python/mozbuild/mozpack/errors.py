# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import sys
from contextlib import contextmanager


class ErrorMessage(Exception):
    '''Exception type raised from errors.error() and errors.fatal()'''


class AccumulatedErrors(Exception):
    '''Exception type raised from errors.accumulate()'''


class ErrorCollector(object):
    '''
    Error handling/logging class. A global instance, errors, is provided for
    convenience.

    Warnings, errors and fatal errors may be logged by calls to the following
    functions:
        errors.warn(message)
        errors.error(message)
        errors.fatal(message)

    Warnings only send the message on the logging output, while errors and
    fatal errors send the message and throw an ErrorMessage exception. The
    exception, however, may be deferred. See further below.

    Errors may be ignored by calling:
        errors.ignore_errors()

    After calling that function, only fatal errors throw an exception.

    The warnings, errors or fatal errors messages may be augmented with context
    information when a context is provided. Context is defined by a pair
    (filename, linenumber), and may be set with errors.context() used as a
    context manager:
        with errors.context(filename, linenumber):
            errors.warn(message)

    Arbitrary nesting is supported, both for errors.context calls:
        with errors.context(filename1, linenumber1):
            errors.warn(message)
            with errors.context(filename2, linenumber2):
                errors.warn(message)

    as well as for function calls:
        def func():
            errors.warn(message)
        with errors.context(filename, linenumber):
            func()

    Errors and fatal errors can have their exception thrown at a later time,
    allowing for several different errors to be reported at once before
    throwing. This is achieved with errors.accumulate() as a context manager:
        with errors.accumulate():
            if test1:
                errors.error(message1)
            if test2:
                errors.error(message2)

    In such cases, a single AccumulatedErrors exception is thrown, but doesn't
    contain information about the exceptions. The logged messages do.
    '''
    out = sys.stderr
    WARN = 1
    ERROR = 2
    FATAL = 3
    _level = ERROR
    _context = []
    _count = None

    def ignore_errors(self, ignore=True):
        if ignore:
            self._level = self.FATAL
        else:
            self._level = self.ERROR

    def _full_message(self, level, msg):
        if level >= self._level:
            level = 'Error'
        else:
            level = 'Warning'
        if self._context:
            file, line = self._context[-1]
            return "%s: %s:%d: %s" % (level, file, line, msg)
        return "%s: %s" % (level, msg)

    def _handle(self, level, msg):
        msg = self._full_message(level, msg)
        if level >= self._level:
            if self._count is None:
                raise ErrorMessage(msg)
            self._count += 1
        print >>self.out, msg

    def fatal(self, msg):
        self._handle(self.FATAL, msg)

    def error(self, msg):
        self._handle(self.ERROR, msg)

    def warn(self, msg):
        self._handle(self.WARN, msg)

    def get_context(self):
        if self._context:
            return self._context[-1]

    @contextmanager
    def context(self, file, line):
        if file and line:
            self._context.append((file, line))
        yield
        if file and line:
            self._context.pop()

    @contextmanager
    def accumulate(self):
        assert self._count is None
        self._count = 0
        yield
        count = self._count
        self._count = None
        if count:
            raise AccumulatedErrors()

    @property
    def count(self):
        # _count can be None.
        return self._count if self._count else 0


errors = ErrorCollector()
