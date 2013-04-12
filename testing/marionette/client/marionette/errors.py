# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

class ErrorCodes(object):
    
    SUCCESS = 0
    NO_SUCH_ELEMENT = 7
    NO_SUCH_FRAME = 8
    UNKNOWN_COMMAND = 9
    STALE_ELEMENT_REFERENCE = 10
    ELEMENT_NOT_VISIBLE = 11
    INVALID_ELEMENT_STATE = 12
    UNKNOWN_ERROR = 13
    ELEMENT_IS_NOT_SELECTABLE = 15
    JAVASCRIPT_ERROR = 17
    XPATH_LOOKUP_ERROR = 19
    TIMEOUT = 21
    NO_SUCH_WINDOW = 23
    INVALID_COOKIE_DOMAIN = 24
    UNABLE_TO_SET_COOKIE = 25
    UNEXPECTED_ALERT_OPEN = 26
    NO_ALERT_OPEN = 27
    SCRIPT_TIMEOUT = 28
    INVALID_ELEMENT_COORDINATES = 29
    INVALID_SELECTOR = 32
    MOVE_TARGET_OUT_OF_BOUNDS = 34
    INVALID_XPATH_SELECTOR = 51
    INVALID_XPATH_SELECTOR_RETURN_TYPER = 52
    INVALID_RESPONSE = 53
    MARIONETTE_ERROR = 500

class MarionetteException(Exception):

    def __init__(self, message=None, status=ErrorCodes.MARIONETTE_ERROR, stacktrace=None):
        self.msg = message
        self.status = status
        self.stacktrace = stacktrace

    def __str__(self):
        if self.stacktrace:
            return '%s\nstacktrace:\n%s' % (str(self.msg),
                ''.join(['\t%s\n' % x for x in self.stacktrace.split('\n')]))
        else:
            return str(self.msg)

class InstallGeckoError(MarionetteException):
    pass

class TimeoutException(MarionetteException):
    pass

class InvalidResponseException(MarionetteException):
    pass

class JavascriptException(MarionetteException):
    pass

class NoSuchElementException(MarionetteException):
    pass

class XPathLookupException(MarionetteException):
    pass

class NoSuchWindowException(MarionetteException):
    pass

class StaleElementException(MarionetteException):
    pass

class ScriptTimeoutException(MarionetteException):
    pass

class ElementNotVisibleException(MarionetteException):
    def __init__(self, message="Element is not currently visible and may not be manipulated",
                status=ErrorCodes.ELEMENT_NOT_VISIBLE, stacktrace=None):
        MarionetteException.__init__(self, message, status, stacktrace)

class NoSuchFrameException(MarionetteException):
    pass

class InvalidElementStateException(MarionetteException):
    pass

class NoAlertPresentException(MarionetteException):
    pass

class InvalidCookieDomainException(MarionetteException):
    pass

class UnableToSetCookieException(MarionetteException):
    pass

class InvalidSelectorException(MarionetteException):
    pass

class MoveTargetOutOfBoundsException(MarionetteException):
    pass

