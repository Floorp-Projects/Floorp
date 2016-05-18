# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import codecs
import itertools
import locale
import logging
import os
import sys
from collections import deque
from contextlib import contextmanager
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

    Only messages above log level INFO (included) are logged.

    Messages below that level can be kept until an ERROR message is received,
    at which point the last `maxlen` accumulated messages below INFO are
    printed out. This feature is only enabled under the `queue_debug` context
    manager.
    '''
    def __init__(self, stdout=sys.stdout, stderr=sys.stderr, maxlen=20):
        super(ConfigureOutputHandler, self).__init__()

        # Python has this feature where it sets the encoding of pipes to
        # ascii, which blatantly fails when trying to print out non-ascii.
        def fix_encoding(fh):
            try:
                isatty = fh.isatty()
            except AttributeError:
                isatty = True

            if not isatty:
                encoding = None
                try:
                    encoding = locale.getpreferredencoding()
                except ValueError:
                    # On english OSX, LC_ALL is UTF-8 (not en-US.UTF-8), and
                    # that throws off locale._parse_localename, which ends up
                    # being used on e.g. homebrew python.
                    if os.environ.get('LC_ALL', '').upper() == 'UTF-8':
                        encoding = 'utf-8'

                # locale._parse_localename makes locale.getpreferredencoding
                # return None when LC_ALL is C, instead of e.g. 'US-ASCII' or
                # 'ANSI_X3.4-1968' when it uses nl_langinfo.
                if encoding:
                    return codecs.getwriter(encoding)(fh)
            return fh

        self._stdout = fix_encoding(stdout)
        self._stderr = fix_encoding(stderr) if stdout != stderr else self._stdout
        try:
            fd1 = self._stdout.fileno()
            fd2 = self._stderr.fileno()
            self._same_output = self._is_same_output(fd1, fd2)
        except AttributeError:
            self._same_output = self._stdout == self._stderr
        self._stdout_waiting = None
        self._debug = deque(maxlen=maxlen + 1)
        self._keep_if_debug = self.THROW
        self._queue_is_active = False

    @staticmethod
    def _is_same_output(fd1, fd2):
        if fd1 == fd2:
            return True
        stat1 = os.fstat(fd1)
        stat2 = os.fstat(fd2)
        return stat1.st_ino == stat2.st_ino and stat1.st_dev == stat2.st_dev

    # possible values for _stdout_waiting
    WAITING = 1
    INTERRUPTED = 2

    # possible values for _keep_if_debug
    THROW = 0
    KEEP = 1
    PRINT = 2

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
            elif (record.levelno < logging.INFO and
                    self._keep_if_debug != self.PRINT):
                if self._keep_if_debug == self.KEEP:
                    self._debug.append(record)
                return
            else:
                if record.levelno >= logging.ERROR and len(self._debug):
                    self._emit_queue()

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

    @contextmanager
    def queue_debug(self):
        if self._queue_is_active:
            yield
            return
        self._queue_is_active = True
        self._keep_if_debug = self.KEEP
        try:
            yield
        except Exception:
            self._emit_queue()
            # The exception will be handled and very probably printed out by
            # something upper in the stack.
            raise
        finally:
            self._queue_is_active = False
        self._keep_if_debug = self.THROW
        self._debug.clear()

    def _emit_queue(self):
        self._keep_if_debug = self.PRINT
        if len(self._debug) == self._debug.maxlen:
            r = self._debug.popleft()
            self.emit(logging.LogRecord(
                r.name, r.levelno, r.pathname, r.lineno,
                '<truncated - see config.log for full output>',
                (), None))
        while True:
            try:
                self.emit(self._debug.popleft())
            except IndexError:
                break
        self._keep_if_debug = self.KEEP


class LineIO(object):
    '''File-like class that sends each line of the written data to a callback
    (without carriage returns).
    '''
    def __init__(self, callback):
        self._callback = callback
        self._buf = ''

    def write(self, buf):
        lines = buf.splitlines()
        if not lines:
            return
        if self._buf:
            lines[0] = self._buf + lines[0]
            self._buf = ''
        if not buf.endswith('\n'):
            self._buf = lines.pop()

        for line in lines:
            self._callback(line)

    def close(self):
        if self._buf:
            self._callback(self._buf)
            self._buf = ''

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()
