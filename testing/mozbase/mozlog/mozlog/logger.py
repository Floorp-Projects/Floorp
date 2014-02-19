# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from logging import getLogger as getSysLogger
from logging import *
# Some of the build slave environments don't see the following when doing
# 'from logging import *'
# see https://bugzilla.mozilla.org/show_bug.cgi?id=700415#c35
from logging import getLoggerClass, addLevelName, setLoggerClass, shutdown, debug, info, basicConfig
try:
    import json
except ImportError:
    import simplejson as json

_default_level = INFO
_LoggerClass = getLoggerClass()

# Define mozlog specific log levels
START      = _default_level + 1
END        = _default_level + 2
PASS       = _default_level + 3
KNOWN_FAIL = _default_level + 4
FAIL       = _default_level + 5
CRASH      = _default_level + 6
# Define associated text of log levels
addLevelName(START, 'TEST-START')
addLevelName(END, 'TEST-END')
addLevelName(PASS, 'TEST-PASS')
addLevelName(KNOWN_FAIL, 'TEST-KNOWN-FAIL')
addLevelName(FAIL, 'TEST-UNEXPECTED-FAIL')
addLevelName(CRASH, 'PROCESS-CRASH')

class MozLogger(_LoggerClass):
    """
    MozLogger class which adds some convenience log levels
    related to automated testing in Mozilla and ability to
    output structured log messages.
    """
    def testStart(self, message, *args, **kwargs):
        """Logs a test start message"""
        self.log(START, message, *args, **kwargs)

    def testEnd(self, message, *args, **kwargs):
        """Logs a test end message"""
        self.log(END, message, *args, **kwargs)

    def testPass(self, message, *args, **kwargs):
        """Logs a test pass message"""
        self.log(PASS, message, *args, **kwargs)

    def testFail(self, message, *args, **kwargs):
        """Logs a test fail message"""
        self.log(FAIL, message, *args, **kwargs)

    def testKnownFail(self, message, *args, **kwargs):
        """Logs a test known fail message"""
        self.log(KNOWN_FAIL, message, *args, **kwargs)

    def processCrash(self, message, *args, **kwargs):
        """Logs a process crash message"""
        self.log(CRASH, message, *args, **kwargs)

    def log_structured(self, action, params=None):
        """Logs a structured message object."""
        if (params is None):
            params = {}

        level = params.get('_level', _default_level)
        if isinstance(level, int):
            params['_level'] = getLevelName(level)
        else:
            params['_level'] = level
            level = getLevelName(level.upper())

            # If the logger is fed a level number unknown to the logging
            # module, getLevelName will return a string. Unfortunately,
            # the logging module will raise a type error elsewhere if
            # the level is not an integer.
            if not isinstance(level, int):
                level = _default_level

        params['_namespace'] = self.name
        params['action'] = action

        message = params.get('message', 'UNKNOWN')
        self.log(level, message, extra={'params': params})

class JSONFormatter(Formatter):
    """Log formatter for emitting structured JSON entries."""

    def format(self, record):
        params = getattr(record, 'params')
        params['_time'] = int(round(record.created * 1000, 0))

        if params.get('indent') is not None:
            return json.dumps(params, indent=params['indent'])

        return json.dumps(params)

class MozFormatter(Formatter):
    """
    MozFormatter class used to standardize formatting
    If a different format is desired, this can be explicitly
    overriden with the log handler's setFormatter() method
    """
    level_length = 0
    max_level_length = len('TEST-START')

    def __init__(self):
        """
        Formatter.__init__ has fmt and datefmt parameters that won't have
        any affect on a MozFormatter instance. Bypass it to avoid confusion.
        """

    def format(self, record):
        record.message = record.getMessage()

        # Handles padding so record levels align nicely
        if len(record.levelname) > self.level_length:
            pad = 0
            if len(record.levelname) <= self.max_level_length:
                self.level_length = len(record.levelname)
        else:
            pad = self.level_length - len(record.levelname) + 1
        sep = '|'.rjust(pad)
        fmt = '%(name)s %(levelname)s ' + sep + ' %(message)s'
        return fmt % record.__dict__

def getLogger(name, handler=None):
    """
    Returns the logger with the specified name.
    If the logger doesn't exist, it is created.
    If handler is specified, adds it to the logger. Otherwise a default handler
    that logs to standard output will be used.

    :param name: The name of the logger to retrieve
    :param handler: A handler to add to the logger. If the logger already exists,
                    and a handler is specified, an exception will be raised. To
                    add a handler to an existing logger, call that logger's
                    addHandler method.
    """
    setLoggerClass(MozLogger)

    if name in Logger.manager.loggerDict:
        if (handler):
            raise ValueError('The handler parameter requires ' + \
                             'that a logger by this name does ' + \
                             'not already exist')
        return Logger.manager.loggerDict[name]

    logger = getSysLogger(name)
    logger.setLevel(_default_level)

    if handler is None:
        handler = StreamHandler()
        handler.setFormatter(MozFormatter())

    logger.addHandler(handler)
    logger.propagate = False
    return logger

