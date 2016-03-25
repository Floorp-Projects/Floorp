# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import itertools
import logging
import os
import sys
from distutils.version import LooseVersion


class Version(LooseVersion):
    '''A simple subclass of distutils.version.LooseVersion.
    Adds attributes for `major`, `minor`, `patch` for the first three
    version components so users can easily pull out major/minor
    versions, like:

    v = Version('1.2b')
    v.major == 1
    v.minor == 2
    v.patch == 0
    '''
    def __init__(self, version):
        # Can't use super, LooseVersion's base class is not a new-style class.
        LooseVersion.__init__(self, version)
        # Take the first three integer components, stopping at the first
        # non-integer and padding the rest with zeroes.
        (self.major, self.minor, self.patch) = list(itertools.chain(
            itertools.takewhile(lambda x:isinstance(x, int), self.version),
            (0, 0, 0)))[:3]


    def __cmp__(self, other):
        # LooseVersion checks isinstance(StringType), so work around it.
        if isinstance(other, unicode):
            other = other.encode('ascii')
        return LooseVersion.__cmp__(self, other)


class ConfigureOutputHandler(logging.Handler):
    '''A logging handler class that sends info messages to stdout and other
    messages to stderr.

    Messages sent to stdout are not formatted with the attached Formatter.
    Additionally, if they end with '... ', no newline character is printed,
    making the next message printed follow the '... '.
    '''
    def __init__(self, stdout=sys.stdout, stderr=sys.stderr):
        super(ConfigureOutputHandler, self).__init__()
        self._stdout, self._stderr = stdout, stderr
        try:
            fd1 = self._stdout.fileno()
            fd2 = self._stderr.fileno()
            self._same_output = self._is_same_output(fd1, fd2)
        except AttributeError:
            self._same_output = self._stdout == self._stderr
        self._stdout_waiting = None

    @staticmethod
    def _is_same_output(fd1, fd2):
        if fd1 == fd2:
            return True
        stat1 = os.fstat(fd1)
        stat2 = os.fstat(fd2)
        return stat1.st_ino == stat2.st_ino and stat1.st_dev == stat2.st_dev

    WAITING = 1
    INTERRUPTED = 2

    def emit(self, record):
        try:
            if record.levelno == logging.INFO:
                stream = self._stdout
                msg = record.getMessage()
                if (self._stdout_waiting == self.INTERRUPTED and
                        self._same_output):
                    msg = ' ... %s' % msg
                self._stdout_waiting = msg.endswith('... ')
                if msg.endswith('... '):
                    self._stdout_waiting = self.WAITING
                else:
                    self._stdout_waiting = None
                    msg = '%s\n' % msg
            else:
                if self._stdout_waiting == self.WAITING and self._same_output:
                    self._stdout_waiting = self.INTERRUPTED
                    self._stdout.write('\n')
                    self._stdout.flush()
                stream = self._stderr
                msg = '%s\n' % self.format(record)
            stream.write(msg)
            stream.flush()
        except (KeyboardInterrupt, SystemExit):
            raise
        except:
            self.handleError(record)
