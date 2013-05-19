# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import mozunit
import os
from mozpack.packager import (
    preprocess_manifest,
    SimplePackager,
    SimpleManifestSink,
    CallDeque,
)
from mozpack.files import GeneratedFile
from mozpack.chrome.manifest import (
    ManifestResource,
    ManifestContent,
)
from mozunit import MockedOpen
from Preprocessor import Preprocessor
from mozpack.errors import (
    errors,
    ErrorMessage,
)
import mozpack.path

MANIFEST = '''
bar/*
[foo]
foo/*
-foo/bar
chrome.manifest
; comment
#ifdef baz
[baz]
baz@SUFFIX@
#endif
'''


class TestPreprocessManifest(unittest.TestCase):
    MANIFEST_PATH = os.path.join(os.path.abspath(os.curdir), 'manifest')

    EXPECTED_LOG = [
        ((MANIFEST_PATH, 2), 'add', '', 'bar/*'),
        ((MANIFEST_PATH, 4), 'add', 'foo', 'foo/*'),
        ((MANIFEST_PATH, 5), 'remove', 'foo', 'foo/bar'),
        ((MANIFEST_PATH, 6), 'add', 'foo', 'chrome.manifest')
    ]

    def setUp(self):
        class MockSink(object):
            def __init__(self):
                self.log = []

            def add(self, section, path):
                self._log(errors.get_context(), 'add', section, path)

            def remove(self, section, path):
                self._log(errors.get_context(), 'remove', section, path)

            def _log(self, *args):
                self.log.append(args)

        self.sink = MockSink()

    def test_preprocess_manifest(self):
        with MockedOpen({'manifest': MANIFEST}):
            preprocess_manifest(self.sink, 'manifest')
        self.assertEqual(self.sink.log, self.EXPECTED_LOG)

    def test_preprocess_manifest_missing_define(self):
        with MockedOpen({'manifest': MANIFEST}):
            self.assertRaises(
                Preprocessor.Error,
                preprocess_manifest,
                self.sink,
                'manifest',
                {'baz': 1}
            )

    def test_preprocess_manifest_defines(self):
        with MockedOpen({'manifest': MANIFEST}):
            preprocess_manifest(self.sink, 'manifest',
                                {'baz': 1, 'SUFFIX': '.exe'})
        self.assertEqual(self.sink.log, self.EXPECTED_LOG +
                         [((self.MANIFEST_PATH, 10), 'add', 'baz', 'baz.exe')])


class MockFinder(object):
    def __init__(self, files):
        self.files = files
        self.log = []

    def find(self, path):
        self.log.append(path)
        for f in sorted(self.files):
            if mozpack.path.match(f, path):
                yield f, self.files[f]

    def __iter__(self):
        return self.find('')


class MockFormatter(object):
    def __init__(self):
        self.log = []

    def add_base(self, *args):
        self._log(errors.get_context(), 'add_base', *args)

    def add_manifest(self, *args):
        self._log(errors.get_context(), 'add_manifest', *args)

    def add_interfaces(self, *args):
        self._log(errors.get_context(), 'add_interfaces', *args)

    def add(self, *args):
        self._log(errors.get_context(), 'add', *args)

    def _log(self, *args):
        self.log.append(args)


class TestSimplePackager(unittest.TestCase):
    def test_simple_packager(self):
        class GeneratedFileWithPath(GeneratedFile):
            def __init__(self, path, content):
                GeneratedFile.__init__(self, content)
                self.path = path

        formatter = MockFormatter()
        packager = SimplePackager(formatter)
        curdir = os.path.abspath(os.curdir)
        file = GeneratedFileWithPath(os.path.join(curdir, 'foo',
                                                  'bar.manifest'),
                                     'resource bar bar/\ncontent bar bar/')
        with errors.context('manifest', 1):
            packager.add('foo/bar.manifest', file)

        file = GeneratedFileWithPath(os.path.join(curdir, 'foo',
                                                  'baz.manifest'),
                                     'resource baz baz/')
        with errors.context('manifest', 2):
            packager.add('bar/baz.manifest', file)

        with errors.context('manifest', 3):
            packager.add('qux/qux.manifest',
                         GeneratedFile('resource qux qux/'))
        bar_xpt = GeneratedFile('bar.xpt')
        qux_xpt = GeneratedFile('qux.xpt')
        foo_html = GeneratedFile('foo_html')
        bar_html = GeneratedFile('bar_html')
        with errors.context('manifest', 4):
            packager.add('foo/bar.xpt', bar_xpt)
        with errors.context('manifest', 5):
            packager.add('foo/bar/foo.html', foo_html)
            packager.add('foo/bar/bar.html', bar_html)

        file = GeneratedFileWithPath(os.path.join(curdir, 'foo.manifest'),
                                     ''.join([
                                         'manifest foo/bar.manifest\n',
                                         'manifest bar/baz.manifest\n',
                                     ]))
        with errors.context('manifest', 6):
            packager.add('foo.manifest', file)
        with errors.context('manifest', 7):
            packager.add('foo/qux.xpt', qux_xpt)

        self.assertEqual(formatter.log, [])

        with errors.context('dummy', 1):
            packager.close()
        self.maxDiff = None
        # The formatter is expected to reorder the manifest entries so that
        # chrome entries appear before the others.
        self.assertEqual(formatter.log, [
            (('dummy', 1), 'add_base', 'qux'),
            ((os.path.join(curdir, 'foo', 'bar.manifest'), 2),
             'add_manifest', ManifestContent('foo', 'bar', 'bar/')),
            ((os.path.join(curdir, 'foo', 'bar.manifest'), 1),
             'add_manifest', ManifestResource('foo', 'bar', 'bar/')),
            (('bar/baz.manifest', 1),
             'add_manifest', ManifestResource('bar', 'baz', 'baz/')),
            (('qux/qux.manifest', 1),
             'add_manifest', ManifestResource('qux', 'qux', 'qux/')),
            (('manifest', 4), 'add_interfaces', 'foo/bar.xpt', bar_xpt),
            (('manifest', 7), 'add_interfaces', 'foo/qux.xpt', qux_xpt),
            (('manifest', 5), 'add', 'foo/bar/foo.html', foo_html),
            (('manifest', 5), 'add', 'foo/bar/bar.html', bar_html),
        ])

        self.assertEqual(packager.get_bases(), set(['', 'qux']))


class TestSimpleManifestSink(unittest.TestCase):
    def test_simple_manifest_parser(self):
        formatter = MockFormatter()
        foobar = GeneratedFile('foobar')
        foobaz = GeneratedFile('foobaz')
        fooqux = GeneratedFile('fooqux')
        finder = MockFinder({
            'bin/foo/bar': foobar,
            'bin/foo/baz': foobaz,
            'bin/foo/qux': fooqux,
            'bin/foo/chrome.manifest': GeneratedFile('resource foo foo/'),
            'bin/chrome.manifest':
            GeneratedFile('manifest foo/chrome.manifest'),
        })
        parser = SimpleManifestSink(finder, formatter)
        parser.add('section0', 'bin/foo/b*')
        parser.add('section1', 'bin/foo/qux')
        parser.add('section1', 'bin/foo/chrome.manifest')
        self.assertRaises(ErrorMessage, parser.add, 'section1', 'bin/bar')

        self.assertEqual(formatter.log, [])
        parser.close()
        self.assertEqual(formatter.log, [
            (('foo/chrome.manifest', 1),
             'add_manifest', ManifestResource('foo', 'foo', 'foo/')),
            (None, 'add', 'foo/bar', foobar),
            (None, 'add', 'foo/baz', foobaz),
            (None, 'add', 'foo/qux', fooqux),
        ])

        self.assertEqual(finder.log, [
            'bin/foo/b*',
            'bin/foo/qux',
            'bin/foo/chrome.manifest',
            'bin/bar',
            'bin/chrome.manifest'
        ])


class TestCallDeque(unittest.TestCase):
    def test_call_deque(self):
        class Logger(object):
            def __init__(self):
                self._log = []

            def log(self, str):
                self._log.append(str)

            @staticmethod
            def staticlog(logger, str):
                logger.log(str)

        def do_log(logger, str):
            logger.log(str)

        logger = Logger()
        d = CallDeque()
        d.append(logger.log, 'foo')
        d.append(logger.log, 'bar')
        d.append(logger.staticlog, logger, 'baz')
        d.append(do_log, logger, 'qux')
        self.assertEqual(logger._log, [])
        d.execute()
        self.assertEqual(logger._log, ['foo', 'bar', 'baz', 'qux'])

if __name__ == '__main__':
    mozunit.main()
