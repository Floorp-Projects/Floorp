# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import traceback


class MarionetteException(Exception):

    """Raised when a generic non-recoverable exception has occured."""

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
        self.cause = cause
        self.stacktrace = stacktrace

        super(MarionetteException, self).__init__(message)

    def __str__(self):
        msg = str(self.message)
        tb = None

        if self.cause:
            if type(self.cause) is tuple:
                msg += ", caused by {0!r}".format(self.cause[0])
                tb = self.cause[2]
            else:
                msg += ", caused by {}".format(self.cause)
        if self.stacktrace:
            st = "".join(["\t{}\n".format(x)
                          for x in self.stacktrace.splitlines()])
            msg += "\nstacktrace:\n{}".format(st)

        if tb:
            msg += ': ' + "".join(traceback.format_tb(tb))

        return msg


class ElementNotSelectableException(MarionetteException):
    status = "element not selectable"


class InsecureCertificateException(MarionetteException):
    status = "insecure certificate"


class InvalidArgumentException(MarionetteException):
    status = "invalid argument"


class InvalidSessionIdException(MarionetteException):
    status = "invalid session id"


class TimeoutException(MarionetteException):
    status = "timeout"


class JavascriptException(MarionetteException):
    status = "javascript error"


class NoSuchElementException(MarionetteException):
    status = "no such element"


class NoSuchWindowException(MarionetteException):
    status = "no such window"


class StaleElementException(MarionetteException):
    status = "stale element reference"


class ScriptTimeoutException(MarionetteException):
    status = "script timeout"


class ElementNotVisibleException(MarionetteException):
    status = "element not visible"

    def __init__(self,
                 message="Element is not currently visible and may not be manipulated",
                 stacktrace=None, cause=None):
        super(ElementNotVisibleException, self).__init__(
            message, cause=cause, stacktrace=stacktrace)


class ElementNotAccessibleException(MarionetteException):
    status = "element not accessible"


class NoSuchFrameException(MarionetteException):
    status = "no such frame"


class InvalidElementStateException(MarionetteException):
    status = "invalid element state"


class NoAlertPresentException(MarionetteException):
    status = "no such alert"


class InvalidCookieDomainException(MarionetteException):
    status = "invalid cookie domain"


class UnableToSetCookieException(MarionetteException):
    status = "unable to set cookie"


class InvalidElementCoordinates(MarionetteException):
    status = "invalid element coordinates"


class InvalidSelectorException(MarionetteException):
    status = "invalid selector"


class MoveTargetOutOfBoundsException(MarionetteException):
    status = "move target out of bounds"


class SessionNotCreatedException(MarionetteException):
    status = "session not created"


class UnexpectedAlertOpen(MarionetteException):
    status = "unexpected alert open"


class UnknownCommandException(MarionetteException):
    status = "unknown command"


class UnknownException(MarionetteException):
    status = "unknown error"


class UnsupportedOperationException(MarionetteException):
    status = "unsupported operation"


es_ = [e for e in locals().values() if type(e) == type and issubclass(e, MarionetteException)]
by_string = {e.status: e for e in es_}


def lookup(identifier):
    """Finds error exception class by associated Selenium JSON wire
    protocol number code, or W3C WebDriver protocol string.

    """
    return by_string.get(identifier, MarionetteException)
