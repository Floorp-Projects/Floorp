# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import traceback
import types


class MarionetteException(Exception):

    """Raised when a generic non-recoverable exception has occured."""

    code = (500,)
    status = "webdriver error"

    def __init__(self, message=None, cause=None, stacktrace=None):
        """Construct new MarionetteException instance.

        :param message: An optional exception message.

        :param cause: An optional tuple of three values giving
            information about the root exception cause.  Expected
            tuple values are (type, value, traceback).

        :param stacktrace: Optional string containing a stacktrace
            (typically from a failed JavaScript execution) that will
            be displayed in the exception's string representation.

        """

        self.msg = message
        self.cause = cause
        self.stacktrace = stacktrace

    def __str__(self):
        msg = str(self.msg)
        tb = None

        if self.cause:
            if type(self.cause) is tuple:
                msg += ", caused by %r" % self.cause[0]
                tb = self.cause[2]
            else:
                msg += ", caused by %s" % self.cause
        if self.stacktrace:
            st = "".join(["\t%s\n" % x for x in self.stacktrace.splitlines()])
            msg += "\nstacktrace:\n%s" % st

        return "".join(traceback.format_exception(self.__class__, msg, tb))


class InstallGeckoError(MarionetteException):
    pass


class TimeoutException(MarionetteException):
    code = (21,)
    status = "timeout"


class InvalidResponseException(MarionetteException):
    code = (53,)
    status = "invalid response"


class JavascriptException(MarionetteException):
    code = (17,)
    status = "javascript error"


class NoSuchElementException(MarionetteException):
    code = (7,)
    status = "no such element"


class XPathLookupException(MarionetteException):
    code = (19,)
    status = "invalid xpath selector"


class NoSuchWindowException(MarionetteException):
    code = (23,)
    status = "no such window"


class StaleElementException(MarionetteException):
    code = (10,)
    status = "stale element reference"


class ScriptTimeoutException(MarionetteException):
    code = (28,)
    status = "script timeout"


class ElementNotVisibleException(MarionetteException):
    code = (11,)
    status = "element not visible"

    def __init__(
        self, message="Element is not currently visible and may not be manipulated",
                 stacktrace=None, cause=None):
        super(ElementNotVisibleException, self).__init__(
            message, cause=cause, stacktrace=stacktrace)


class ElementNotAccessibleException(MarionetteException):
    code = (56,)
    status = "element not accessible"


class NoSuchFrameException(MarionetteException):
    code = (8,)
    status = "no such frame"


class InvalidElementStateException(MarionetteException):
    code = (12,)
    status = "invalid element state"


class NoAlertPresentException(MarionetteException):
    code = (27,)
    status = "no such alert"


class InvalidCookieDomainException(MarionetteException):
    code = (24,)
    status = "invalid cookie domain"


class UnableToSetCookieException(MarionetteException):
    code = (25,)
    status = "unable to set cookie"


class InvalidElementCoordinates(MarionetteException):
    code = (29,)
    status = "invalid element coordinates"


class InvalidSelectorException(MarionetteException):
    code = (32, 51, 52)
    status = "invalid selector"


class MoveTargetOutOfBoundsException(MarionetteException):
    code = (34,)
    status = "move target out of bounds"


class FrameSendNotInitializedError(MarionetteException):
    code = (54,)
    status = "frame send not initialized"


class FrameSendFailureError(MarionetteException):
    code = (55,)
    status = "frame send failure"


class UnsupportedOperationException(MarionetteException):
    code = (405,)
    status = "unsupported operation"


class SessionNotCreatedException(MarionetteException):
    code = (33, 71)
    status = "session not created"


class UnexpectedAlertOpen(MarionetteException):
    code = (26,)
    status = "unexpected alert open"

excs = [
  MarionetteException,
  TimeoutException,
  InvalidResponseException,
  JavascriptException,
  NoSuchElementException,
  XPathLookupException,
  NoSuchWindowException,
  StaleElementException,
  ScriptTimeoutException,
  ElementNotVisibleException,
  ElementNotAccessibleException,
  NoSuchFrameException,
  InvalidElementStateException,
  NoAlertPresentException,
  InvalidCookieDomainException,
  UnableToSetCookieException,
  InvalidElementCoordinates,
  InvalidSelectorException,
  MoveTargetOutOfBoundsException,
  FrameSendNotInitializedError,
  FrameSendFailureError,
  UnsupportedOperationException,
  SessionNotCreatedException,
  UnexpectedAlertOpen,
]


def lookup(identifier):
    """Finds error exception class by associated Selenium JSON wire
    protocol number code, or W3C WebDriver protocol string."""

    by_code = lambda exc: identifier in exc.code
    by_status = lambda exc: exc.status == identifier

    rv = None
    if isinstance(identifier, int):
        rv = filter(by_code, excs)
    elif isinstance(identifier, types.StringTypes):
        rv = filter(by_status, excs)

    if not rv:
        return MarionetteException
    return rv[0]


__all__ = excs + ["lookup"]
