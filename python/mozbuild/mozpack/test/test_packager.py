# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import mozunit
import os
from mozpack.packager import (
    preprocess_manifest,
    CallDeque,
    Component,
    SimplePackager,
    SimpleManifestSink,
)
from mozpack.files import GeneratedFile
from mozpack.chrome.manifest import (
    ManifestResource,
    ManifestContent,
)
from mozunit import MockedOpen
from mozbuild.preprocessor import Preprocessor
from mozpack.errors import (
    errors,
    ErrorMessage,
)
import mozpack.path as mozpath

MANIFEST = '''
bar/*
[foo]
foo/*
-foo/bar
chrome.manifest
[zot destdir="destdir"]
foo/zot
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
        ((MANIFEST_PATH, 6), 'add', 'foo', 'chrome.manifest'),
        ((MANIFEST_PATH, 8), 'add', 'zot destdir="destdir"', 'foo/zot'),
    ]

    def setUp(self):
        class MockSink(object):
            def __init__(self):
                self.log = []

            def add(self, component, path):
                self._log(errors.get_context(), 'add', repr(component), path)

            def remove(self, component, path):
                self._log(errors.get_context(), 'remove', repr(component), path)

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
                         [((self.MANIFEST_PATH, 12), 'add', 'baz', 'baz.exe')])


class MockFinder(object):
    def __init__(self, files):
        self.files = files
        self.log = []

    def find(self, path):
        self.log.append(path)
        for f in sorted(self.files):
            if mozpath.match(f, path):
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

        file = GeneratedFileWithPath(os.path.join(curdir, 'addon',
                                                  'chrome.manifest'),
                                     'resource hoge hoge/')
        with errors.context('manifest', 8):
            packager.add('addon/chrome.manifest', file)

        install_rdf = GeneratedFile('<RDF></RDF>')
        with errors.context('manifest', 9):
            packager.add('addon/install.rdf', install_rdf)

        self.assertEqual(formatter.log, [])

        with errors.context('dummy', 1):
            packager.close()
        self.maxDiff = None
        # The formatter is expected to reorder the manifest entries so that
        # chrome entries appear before the others.
        self.assertEqual(formatter.log, [
            (('dummy', 1), 'add_base', '', False),
            (('dummy', 1), 'add_base', 'qux', False),
            (('dummy', 1), 'add_base', 'addon', True),
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
            ((os.path.join(curdir, 'addon', 'chrome.manifest'), 1),
             'add_manifest', ManifestResource('addon', 'hoge', 'hoge/')),
            (('manifest', 5), 'add', 'foo/bar/foo.html', foo_html),
            (('manifest', 5), 'add', 'foo/bar/bar.html', bar_html),
            (('manifest', 9), 'add', 'addon/install.rdf', install_rdf),
        ])

        self.assertEqual(packager.get_bases(), set(['', 'addon', 'qux']))
        self.assertEqual(packager.get_bases(addons=False), set(['', 'qux']))

    def test_simple_packager_manifest_consistency(self):
        formatter = MockFormatter()
        # bar/ is detected as an addon because of install.rdf, but top-level
        # includes a manifest inside bar/.
        packager = SimplePackager(formatter)
        packager.add('base.manifest', GeneratedFile(
            'manifest foo/bar.manifest\n'
            'manifest bar/baz.manifest\n'
        ))
        packager.add('foo/bar.manifest', GeneratedFile('resource bar bar'))
        packager.add('bar/baz.manifest', GeneratedFile('resource baz baz'))
        packager.add('bar/install.rdf', GeneratedFile(''))

        with self.assertRaises(ErrorMessage) as e:
            packager.close()

        self.assertEqual(e.exception.message,
            'Error: "bar/baz.manifest" is included from "base.manifest", '
            'which is outside "bar"')

        # bar/ is detected as a separate base because of chrome.manifest that
        # is included nowhere, but top-level includes another manifest inside
        # bar/.
        packager = SimplePackager(formatter)
        packager.add('base.manifest', GeneratedFile(
            'manifest foo/bar.manifest\n'
            'manifest bar/baz.manifest\n'
        ))
        packager.add('foo/bar.manifest', GeneratedFile('resource bar bar'))
        packager.add('bar/baz.manifest', GeneratedFile('resource baz baz'))
        packager.add('bar/chrome.manifest', GeneratedFile('resource baz baz'))

        with self.assertRaises(ErrorMessage) as e:
            packager.close()

        self.assertEqual(e.exception.message,
            'Error: "bar/baz.manifest" is included from "base.manifest", '
            'which is outside "bar"')

        # bar/ is detected as a separate base because of chrome.manifest that
        # is included nowhere, but chrome.manifest includes baz.manifest from
        # the same directory. This shouldn't error out.
        packager = SimplePackager(formatter)
        packager.add('base.manifest', GeneratedFile(
            'manifest foo/bar.manifest\n'
        ))
        packager.add('foo/bar.manifest', GeneratedFile('resource bar bar'))
        packager.add('bar/baz.manifest', GeneratedFile('resource baz baz'))
        packager.add('bar/chrome.manifest',
                     GeneratedFile('manifest baz.manifest'))
        packager.close()


class TestSimpleManifestSink(unittest.TestCase):
    def test_simple_manifest_parser(self):
        formatter = MockFormatter()
        foobar = GeneratedFile('foobar')
        foobaz = GeneratedFile('foobaz')
        fooqux = GeneratedFile('fooqux')
        foozot = GeneratedFile('foozot')
        finder = MockFinder({
            'bin/foo/bar': foobar,
            'bin/foo/baz': foobaz,
            'bin/foo/qux': fooqux,
            'bin/foo/zot': foozot,
            'bin/foo/chrome.manifest': GeneratedFile('resource foo foo/'),
            'bin/chrome.manifest':
            GeneratedFile('manifest foo/chrome.manifest'),
        })
        parser = SimpleManifestSink(finder, formatter)
        component0 = Component('component0')
        component1 = Component('component1')
        component2 = Component('component2', destdir='destdir')
        parser.add(component0, 'bin/foo/b*')
        parser.add(component1, 'bin/foo/qux')
        parser.add(component1, 'bin/foo/chrome.manifest')
        parser.add(component2, 'bin/foo/zot')
        self.assertRaises(ErrorMessage, parser.add, 'component1', 'bin/bar')

        self.assertEqual(formatter.log, [])
        parser.close()
        self.assertEqual(formatter.log, [
            (None, 'add_base', '', False),
            (('foo/chrome.manifest', 1),
             'add_manifest', ManifestResource('foo', 'foo', 'foo/')),
            (None, 'add', 'foo/bar', foobar),
            (None, 'add', 'foo/baz', foobaz),
            (None, 'add', 'foo/qux', fooqux),
            (None, 'add', 'destdir/foo/zot', foozot),
        ])

        self.assertEqual(finder.log, [
            'bin/foo/b*',
            'bin/foo/qux',
            'bin/foo/chrome.manifest',
            'bin/foo/zot',
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


class TestComponent(unittest.TestCase):
    def do_split(self, string, name, options):
        n, o = Component._split_component_and_options(string)
        self.assertEqual(name, n)
        self.assertEqual(options, o)

    def test_component_split_component_and_options(self):
        self.do_split('component', 'component', {})
        self.do_split('trailingspace ', 'trailingspace', {})
        self.do_split(' leadingspace', 'leadingspace', {})
        self.do_split(' trim ', 'trim', {})
        self.do_split(' trim key="value"', 'trim', {'key':'value'})
        self.do_split(' trim empty=""', 'trim', {'empty':''})
        self.do_split(' trim space=" "', 'trim', {'space':' '})
        self.do_split('component key="value"  key2="second" ',
                      'component', {'key':'value', 'key2':'second'})
        self.do_split( 'trim key="  value with spaces   "  key2="spaces again"',
                       'trim', {'key':'  value with spaces   ', 'key2': 'spaces again'})

    def do_split_error(self, string):
        self.assertRaises(ValueError, Component._split_component_and_options, string)

    def test_component_split_component_and_options_errors(self):
        self.do_split_error('"component')
        self.do_split_error('comp"onent')
        self.do_split_error('component"')
        self.do_split_error('"component"')
        self.do_split_error('=component')
        self.do_split_error('comp=onent')
        self.do_split_error('component=')
        self.do_split_error('key="val"')
        self.do_split_error('component key=')
        self.do_split_error('component key="val')
        self.do_split_error('component key=val"')
        self.do_split_error('component key="val" x')
        self.do_split_error('component x key="val"')
        self.do_split_error('component key1="val" x key2="val"')

    def do_from_string(self, string, name, destdir=''):
        component = Component.from_string(string)
        self.assertEqual(name, component.name)
        self.assertEqual(destdir, component.destdir)

    def test_component_from_string(self):
        self.do_from_string('component', 'component')
        self.do_from_string('component-with-hyphen', 'component-with-hyphen')
        self.do_from_string('component destdir="foo/bar"', 'component', 'foo/bar')
        self.do_from_string('component destdir="bar spc"', 'component', 'bar spc')
        self.assertRaises(ErrorMessage, Component.from_string, '')
        self.assertRaises(ErrorMessage, Component.from_string, 'component novalue=')
        self.assertRaises(ErrorMessage, Component.from_string, 'component badoption=badvalue')


if __name__ == '__main__':
    mozunit.main()
