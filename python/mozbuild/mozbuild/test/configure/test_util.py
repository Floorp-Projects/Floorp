# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import os
import tempfile
import textwrap
import unittest
import sys

from StringIO import StringIO

from mozunit import main
from mozpack import path as mozpath

from mozbuild.configure.util import (
    ConfigureOutputHandler,
    getpreferredencoding,
    LineIO,
    Version,
)

from mozbuild.configure import (
    ConfigureSandbox,
)

from mozbuild.util import exec_

from buildconfig import topsrcdir
from common import ConfigureTestSandbox


class TestConfigureOutputHandler(unittest.TestCase):
    def test_separation(self):
        out = StringIO()
        err = StringIO()
        name = '%s.test_separation' % self.__class__.__name__
        logger = logging.getLogger(name)
        logger.setLevel(logging.DEBUG)
        logger.addHandler(ConfigureOutputHandler(out, err))

        logger.error('foo')
        logger.warning('bar')
        logger.info('baz')
        # DEBUG level is not printed out by this handler
        logger.debug('qux')

        self.assertEqual(out.getvalue(), 'baz\n')
        self.assertEqual(err.getvalue(), 'foo\nbar\n')

    def test_format(self):
        out = StringIO()
        err = StringIO()
        name = '%s.test_format' % self.__class__.__name__
        logger = logging.getLogger(name)
        logger.setLevel(logging.DEBUG)
        handler =  ConfigureOutputHandler(out, err)
        handler.setFormatter(logging.Formatter('%(levelname)s:%(message)s'))
        logger.addHandler(handler)

        logger.error('foo')
        logger.warning('bar')
        logger.info('baz')
        # DEBUG level is not printed out by this handler
        logger.debug('qux')

        self.assertEqual(out.getvalue(), 'baz\n')
        self.assertEqual(
            err.getvalue(),
            'ERROR:foo\n'
            'WARNING:bar\n'
        )

    def test_continuation(self):
        out = StringIO()
        name = '%s.test_continuation' % self.__class__.__name__
        logger = logging.getLogger(name)
        logger.setLevel(logging.DEBUG)
        handler =  ConfigureOutputHandler(out, out)
        handler.setFormatter(logging.Formatter('%(levelname)s:%(message)s'))
        logger.addHandler(handler)

        logger.info('foo')
        logger.info('checking bar... ')
        logger.info('yes')
        logger.info('qux')

        self.assertEqual(
            out.getvalue(),
            'foo\n'
            'checking bar... yes\n'
            'qux\n'
        )

        out.seek(0)
        out.truncate()

        logger.info('foo')
        logger.info('checking bar... ')
        logger.warning('hoge')
        logger.info('no')
        logger.info('qux')

        self.assertEqual(
            out.getvalue(),
            'foo\n'
            'checking bar... \n'
            'WARNING:hoge\n'
            ' ... no\n'
            'qux\n'
        )

        out.seek(0)
        out.truncate()

        logger.info('foo')
        logger.info('checking bar... ')
        logger.warning('hoge')
        logger.warning('fuga')
        logger.info('no')
        logger.info('qux')

        self.assertEqual(
            out.getvalue(),
            'foo\n'
            'checking bar... \n'
            'WARNING:hoge\n'
            'WARNING:fuga\n'
            ' ... no\n'
            'qux\n'
        )

        out.seek(0)
        out.truncate()
        err = StringIO()

        logger.removeHandler(handler)
        handler =  ConfigureOutputHandler(out, err)
        handler.setFormatter(logging.Formatter('%(levelname)s:%(message)s'))
        logger.addHandler(handler)

        logger.info('foo')
        logger.info('checking bar... ')
        logger.warning('hoge')
        logger.warning('fuga')
        logger.info('no')
        logger.info('qux')

        self.assertEqual(
            out.getvalue(),
            'foo\n'
            'checking bar... no\n'
            'qux\n'
        )

        self.assertEqual(
            err.getvalue(),
            'WARNING:hoge\n'
            'WARNING:fuga\n'
        )

    def test_queue_debug(self):
        out = StringIO()
        name = '%s.test_queue_debug' % self.__class__.__name__
        logger = logging.getLogger(name)
        logger.setLevel(logging.DEBUG)
        handler =  ConfigureOutputHandler(out, out, maxlen=3)
        handler.setFormatter(logging.Formatter('%(levelname)s:%(message)s'))
        logger.addHandler(handler)

        with handler.queue_debug():
            logger.info('checking bar... ')
            logger.debug('do foo')
            logger.info('yes')
            logger.info('qux')

        self.assertEqual(
            out.getvalue(),
            'checking bar... yes\n'
            'qux\n'
        )

        out.seek(0)
        out.truncate()

        with handler.queue_debug():
            logger.info('checking bar... ')
            logger.debug('do foo')
            logger.info('no')
            logger.error('fail')

        self.assertEqual(
            out.getvalue(),
            'checking bar... no\n'
            'DEBUG:do foo\n'
            'ERROR:fail\n'
        )

        out.seek(0)
        out.truncate()

        with handler.queue_debug():
            logger.info('checking bar... ')
            logger.debug('do foo')
            logger.debug('do bar')
            logger.debug('do baz')
            logger.info('no')
            logger.error('fail')

        self.assertEqual(
            out.getvalue(),
            'checking bar... no\n'
            'DEBUG:do foo\n'
            'DEBUG:do bar\n'
            'DEBUG:do baz\n'
            'ERROR:fail\n'
        )

        out.seek(0)
        out.truncate()

        with handler.queue_debug():
            logger.info('checking bar... ')
            logger.debug('do foo')
            logger.debug('do bar')
            logger.debug('do baz')
            logger.debug('do qux')
            logger.debug('do hoge')
            logger.info('no')
            logger.error('fail')

        self.assertEqual(
            out.getvalue(),
            'checking bar... no\n'
            'DEBUG:<truncated - see config.log for full output>\n'
            'DEBUG:do baz\n'
            'DEBUG:do qux\n'
            'DEBUG:do hoge\n'
            'ERROR:fail\n'
        )

        out.seek(0)
        out.truncate()

        try:
            with handler.queue_debug():
                logger.info('checking bar... ')
                logger.debug('do foo')
                logger.debug('do bar')
                logger.info('no')
                e = Exception('fail')
                raise e
        except Exception as caught:
            self.assertIs(caught, e)

        self.assertEqual(
            out.getvalue(),
            'checking bar... no\n'
            'DEBUG:do foo\n'
            'DEBUG:do bar\n'
        )

    def test_queue_debug_reentrant(self):
        out = StringIO()
        name = '%s.test_queue_debug_reentrant' % self.__class__.__name__
        logger = logging.getLogger(name)
        logger.setLevel(logging.DEBUG)
        handler =  ConfigureOutputHandler(out, out, maxlen=10)
        handler.setFormatter(logging.Formatter('%(levelname)s| %(message)s'))
        logger.addHandler(handler)

        try:
            with handler.queue_debug():
                logger.info('outer info')
                logger.debug('outer debug')
                with handler.queue_debug():
                    logger.info('inner info')
                    logger.debug('inner debug')
                    e = Exception('inner exception')
                    raise e
        except Exception as caught:
            self.assertIs(caught, e)

        self.assertEqual(out.getvalue(),
                         'outer info\n'
                         'inner info\n'
                         'DEBUG| outer debug\n'
                         'DEBUG| inner debug\n')

        out.seek(0)
        out.truncate()

        try:
            with handler.queue_debug():
                logger.info('outer info')
                logger.debug('outer debug')
                with handler.queue_debug():
                    logger.info('inner info')
                    logger.debug('inner debug')
                e = Exception('outer exception')
                raise e
        except Exception as caught:
            self.assertIs(caught, e)

        self.assertEqual(out.getvalue(),
                         'outer info\n'
                         'inner info\n'
                         'DEBUG| outer debug\n'
                         'DEBUG| inner debug\n')

        out.seek(0)
        out.truncate()

        with handler.queue_debug():
            logger.info('outer info')
            logger.debug('outer debug')
            with handler.queue_debug():
                logger.info('inner info')
                logger.debug('inner debug')
                logger.error('inner error')
        self.assertEqual(out.getvalue(),
                         'outer info\n'
                         'inner info\n'
                         'DEBUG| outer debug\n'
                         'DEBUG| inner debug\n'
                         'ERROR| inner error\n')

        out.seek(0)
        out.truncate()

        with handler.queue_debug():
            logger.info('outer info')
            logger.debug('outer debug')
            with handler.queue_debug():
                logger.info('inner info')
                logger.debug('inner debug')
            logger.error('outer error')
        self.assertEqual(out.getvalue(),
                         'outer info\n'
                         'inner info\n'
                         'DEBUG| outer debug\n'
                         'DEBUG| inner debug\n'
                         'ERROR| outer error\n')

    def test_is_same_output(self):
        fd1 = sys.stderr.fileno()
        fd2 = os.dup(fd1)
        try:
            self.assertTrue(ConfigureOutputHandler._is_same_output(fd1, fd2))
        finally:
            os.close(fd2)

        fd2, path = tempfile.mkstemp()
        try:
            self.assertFalse(ConfigureOutputHandler._is_same_output(fd1, fd2))

            fd3 = os.dup(fd2)
            try:
                self.assertTrue(ConfigureOutputHandler._is_same_output(fd2, fd3))
            finally:
                os.close(fd3)

            with open(path, 'a') as fh:
                fd3 = fh.fileno()
                self.assertTrue(
                    ConfigureOutputHandler._is_same_output(fd2, fd3))

        finally:
            os.close(fd2)
            os.remove(path)


class TestLineIO(unittest.TestCase):
    def test_lineio(self):
        lines = []
        l = LineIO(lambda l: lines.append(l))

        l.write('a')
        self.assertEqual(lines, [])

        l.write('b')
        self.assertEqual(lines, [])

        l.write('\n')
        self.assertEqual(lines, ['ab'])

        l.write('cdef')
        self.assertEqual(lines, ['ab'])

        l.write('\n')
        self.assertEqual(lines, ['ab', 'cdef'])

        l.write('ghi\njklm')
        self.assertEqual(lines, ['ab', 'cdef', 'ghi'])

        l.write('nop\nqrst\nuv\n')
        self.assertEqual(lines, ['ab', 'cdef', 'ghi', 'jklmnop', 'qrst', 'uv'])

        l.write('wx\nyz')
        self.assertEqual(lines, ['ab', 'cdef', 'ghi', 'jklmnop', 'qrst', 'uv',
                                 'wx'])

        l.close()
        self.assertEqual(lines, ['ab', 'cdef', 'ghi', 'jklmnop', 'qrst', 'uv',
                                 'wx', 'yz'])

    def test_lineio_contextmanager(self):
        lines = []
        with LineIO(lambda l: lines.append(l)) as l:
            l.write('a\nb\nc')

            self.assertEqual(lines, ['a', 'b'])

        self.assertEqual(lines, ['a', 'b', 'c'])


class TestLogSubprocessOutput(unittest.TestCase):

    def test_non_ascii_subprocess_output(self):
        out = StringIO()
        sandbox = ConfigureSandbox({}, {}, [], out, out)

        sandbox.include_file(mozpath.join(topsrcdir, 'build',
                             'moz.configure', 'util.configure'))
        sandbox.include_file(mozpath.join(topsrcdir, 'python', 'mozbuild',
                                          'mozbuild', 'test', 'configure',
                                          'data', 'subprocess.configure'))
        status = 0
        try:
            sandbox.run()
        except SystemExit as e:
            status = e.code

        self.assertEquals(status, 0)
        quote_char = "'"
        if getpreferredencoding().lower() == 'utf-8':
            quote_char = '\u00B4'.encode('utf-8')
        self.assertEquals(out.getvalue().strip(), quote_char)


class TestVersion(unittest.TestCase):
    def test_version_simple(self):
        v = Version('1')
        self.assertEqual(v, '1')
        self.assertLess(v, '2')
        self.assertGreater(v, '0.5')
        self.assertEqual(v.major, 1)
        self.assertEqual(v.minor, 0)
        self.assertEqual(v.patch, 0)

    def test_version_more(self):
        v = Version('1.2.3b')
        self.assertLess(v, '2')
        self.assertEqual(v.major, 1)
        self.assertEqual(v.minor, 2)
        self.assertEqual(v.patch, 3)

    def test_version_bad(self):
        # A version with a letter in the middle doesn't really make sense,
        # so everything after it should be ignored.
        v = Version('1.2b.3')
        self.assertLess(v, '2')
        self.assertEqual(v.major, 1)
        self.assertEqual(v.minor, 2)
        self.assertEqual(v.patch, 0)

    def test_version_badder(self):
        v = Version('1b.2.3')
        self.assertLess(v, '2')
        self.assertEqual(v.major, 1)
        self.assertEqual(v.minor, 0)
        self.assertEqual(v.patch, 0)

class TestCheckCmdOutput(unittest.TestCase):

    def get_result(self, command='', paths=None):
        paths = paths or {}
        config = {}
        out = StringIO()
        sandbox = ConfigureTestSandbox(paths, config, {}, ['/bin/configure'],
                                       out, out)
        sandbox.include_file(mozpath.join(topsrcdir, 'build',
                             'moz.configure', 'util.configure'))
        status = 0
        try:
            exec_(command, sandbox)
            sandbox.run()
        except SystemExit as e:
            status = e.code
        return config, out.getvalue(), status

    def test_simple_program(self):
        def mock_simple_prog(_, args):
            if len(args) == 1 and args[0] == '--help':
                return 0, 'simple program help...', ''
            self.fail("Unexpected arguments to mock_simple_program: %s" %
                      args)
        prog_path = mozpath.abspath('/simple/prog')
        cmd = "log.info(check_cmd_output('%s', '--help'))" % prog_path
        config, out, status = self.get_result(cmd,
                                              paths={prog_path: mock_simple_prog})
        self.assertEqual(config, {})
        self.assertEqual(status, 0)
        self.assertEqual(out, 'simple program help...\n')

    def test_failing_program(self):
        def mock_error_prog(_, args):
            if len(args) == 1 and args[0] == '--error':
                return (127, 'simple program output',
                        'simple program error output')
            self.fail("Unexpected arguments to mock_error_program: %s" %
                      args)
        prog_path = mozpath.abspath('/simple/prog')
        cmd = "log.info(check_cmd_output('%s', '--error'))" % prog_path
        config, out, status = self.get_result(cmd,
                                              paths={prog_path: mock_error_prog})
        self.assertEqual(config, {})
        self.assertEqual(status, 1)
        self.assertEqual(out, textwrap.dedent('''\
            DEBUG: Executing: `%s --error`
            DEBUG: The command returned non-zero exit status 127.
            DEBUG: Its output was:
            DEBUG: | simple program output
            DEBUG: Its error output was:
            DEBUG: | simple program error output
            ERROR: Command `%s --error` failed with exit status 127.
        ''' % (prog_path, prog_path)))

    def test_error_callback(self):
        def mock_error_prog(_, args):
            if len(args) == 1 and args[0] == '--error':
                return 127, 'simple program error...', ''
            self.fail("Unexpected arguments to mock_error_program: %s" %
                      args)

        prog_path = mozpath.abspath('/simple/prog')
        cmd = textwrap.dedent('''\
            check_cmd_output('%s', '--error',
                             onerror=lambda: die('`prog` produced an error'))
        ''' % prog_path)
        config, out, status = self.get_result(cmd,
                                              paths={prog_path: mock_error_prog})
        self.assertEqual(config, {})
        self.assertEqual(status, 1)
        self.assertEqual(out, textwrap.dedent('''\
            DEBUG: Executing: `%s --error`
            DEBUG: The command returned non-zero exit status 127.
            DEBUG: Its output was:
            DEBUG: | simple program error...
            ERROR: `prog` produced an error
        ''' % prog_path))


if __name__ == '__main__':
    main()
