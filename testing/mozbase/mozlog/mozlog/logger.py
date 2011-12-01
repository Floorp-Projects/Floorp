# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/ #
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozlog.
#
# The Initial Developer of the Original Code is
#   The Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Andrew Halberstadt <halbersa@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
from logging import getLogger as getSysLogger
from logging import *

_default_level = INFO
_LoggerClass = getLoggerClass()

# Define mozlog specific log levels
START      = _default_level + 1
END        = _default_level + 2
PASS       = _default_level + 3
KNOWN_FAIL = _default_level + 4
FAIL       = _default_level + 5
# Define associated text of log levels
addLevelName(START, 'TEST-START')
addLevelName(END, 'TEST-END')
addLevelName(PASS, 'TEST-PASS')
addLevelName(KNOWN_FAIL, 'TEST-KNOWN-FAIL')
addLevelName(FAIL, 'TEST-UNEXPECTED-FAIL')

class _MozLogger(_LoggerClass):
    """
    MozLogger class which adds three convenience log levels
    related to automated testing in Mozilla
    """
    def testStart(self, message, *args, **kwargs):
        self.log(START, message, *args, **kwargs)

    def testEnd(self, message, *args, **kwargs):
        self.log(END, message, *args, **kwargs)

    def testPass(self, message, *args, **kwargs):
        self.log(PASS, message, *args, **kwargs)

    def testFail(self, message, *args, **kwargs):
        self.log(FAIL, message, *args, **kwargs)

    def testKnownFail(self, message, *args, **kwargs):
        self.log(KNOWN_FAIL, message, *args, **kwargs)

class _MozFormatter(Formatter):
    """
    MozFormatter class used for default formatting
    This can easily be overriden with the log handler's setFormatter()
    """
    level_length = 0
    max_level_length = len('TEST-START')

    def __init__(self):
        pass

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

def getLogger(name, logfile=None):
    """
    Returns the logger with the specified name.
    If the logger doesn't exist, it is created.

    name       - The name of the logger to retrieve
    [filePath] - If specified, the logger will log to the specified filePath
                 Otherwise, the logger logs to stdout
                 This parameter only has an effect if the logger doesn't exist
    """
    setLoggerClass(_MozLogger)

    if name in Logger.manager.loggerDict:
        return getSysLogger(name)

    logger = getSysLogger(name)
    logger.setLevel(_default_level)

    if logfile:
        handler = FileHandler(logfile)
    else:
        handler = StreamHandler()
    handler.setFormatter(_MozFormatter())
    logger.addHandler(handler)
    return logger

