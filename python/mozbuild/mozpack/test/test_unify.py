# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozpack.unify import (
    UnifiedFinder,
    UnifiedBuildFinder,
)
import mozunit
from mozpack.test.test_files import TestWithTmpDir
from mozpack.copier import ensure_parent_dir
from mozpack.files import FileFinder
from mozpack.mozjar import JarWriter
from mozpack.test.test_files import MockDest
from cStringIO import StringIO
import os
import sys
from mozpack.errors import (
    ErrorMessage,
    AccumulatedErrors,
    errors,
)


class TestUnified(TestWithTmpDir):
    def create_one(self, which, path, content):
        file = self.tmppath(os.path.join(which, path))
        ensure_parent_dir(file)
        open(file, 'wb').write(content)

    def create_both(self, path, content):
        for p in ['a', 'b']:
            self.create_one(p, path, content)


class TestUnifiedFinder(TestUnified):
    def test_unified_finder(self):
        self.create_both('foo/bar', 'foobar')
        self.create_both('foo/baz', 'foobaz')
        self.create_one('a', 'bar', 'bar')
        self.create_one('b', 'baz', 'baz')
        self.create_one('a', 'qux', 'foobar')
        self.create_one('b', 'qux', 'baz')
        self.create_one('a', 'test/foo', 'a\nb\nc\n')
        self.create_one('b', 'test/foo', 'b\nc\na\n')
        self.create_both('test/bar', 'a\nb\nc\n')

        finder = UnifiedFinder(FileFinder(self.tmppath('a')),
                               FileFinder(self.tmppath('b')),
                               sorted=['test'])
        self.assertEqual(sorted([(f, c.open().read())
                                 for f, c in finder.find('foo')]),
                         [('foo/bar', 'foobar'), ('foo/baz', 'foobaz')])
        self.assertRaises(ErrorMessage, any, finder.find('bar'))
        self.assertRaises(ErrorMessage, any, finder.find('baz'))
        self.assertRaises(ErrorMessage, any, finder.find('qux'))
        self.assertEqual(sorted([(f, c.open().read())
                                 for f, c in finder.find('test')]),
                         [('test/bar', 'a\nb\nc\n'),
                          ('test/foo', 'a\nb\nc\n')])


class TestUnifiedBuildFinder(TestUnified):
    def test_unified_build_finder(self):
        self.create_both('chrome.manifest', 'a\nb\nc\n')
        self.create_one('a', 'chrome/chrome.manifest', 'a\nb\nc\n')
        self.create_one('b', 'chrome/chrome.manifest', 'b\nc\na\n')
        self.create_one('a', 'chrome/browser/foo/buildconfig.html',
                        '\n'.join([
                            '<html>',
                            '<body>',
                            '<h1>about:buildconfig</h1>',
                            '<div>foo</div>',
                            '</body>',
                            '</html>',
                        ]))
        self.create_one('b', 'chrome/browser/foo/buildconfig.html',
                        '\n'.join([
                            '<html>',
                            '<body>',
                            '<h1>about:buildconfig</h1>',
                            '<div>bar</div>',
                            '</body>',
                            '</html>',
                        ]))
        finder = UnifiedBuildFinder(FileFinder(self.tmppath('a')),
                                    FileFinder(self.tmppath('b')))
        self.assertEqual(sorted([(f, c.open().read()) for f, c in
                                 finder.find('**/chrome.manifest')]),
                         [('chrome.manifest', 'a\nb\nc\n'),
                          ('chrome/chrome.manifest', 'a\nb\nc\n')])

        self.assertEqual(sorted([(f, c.open().read()) for f, c in
                                 finder.find('**/buildconfig.html')]),
                         [('chrome/browser/foo/buildconfig.html', '\n'.join([
                             '<html>',
                             '<body>',
                             '<h1>about:buildconfig</h1>',
                             '<div>foo</div>',
                             '<hr> </hr>',
                             '<div>bar</div>',
                             '</body>',
                             '</html>',
                         ]))])

        xpi = MockDest()
        with JarWriter(fileobj=xpi, compress=True) as jar:
            jar.add('foo', 'foo')
            jar.add('bar', 'bar')
        foo_xpi = xpi.read()
        self.create_both('foo.xpi', foo_xpi)

        with JarWriter(fileobj=xpi, compress=True) as jar:
            jar.add('foo', 'bar')
        self.create_one('a', 'bar.xpi', foo_xpi)
        self.create_one('b', 'bar.xpi', xpi.read())

        errors.out = StringIO()
        with self.assertRaises(AccumulatedErrors), errors.accumulate():
            self.assertEqual([(f, c.open().read()) for f, c in
                              finder.find('*.xpi')],
                             [('foo.xpi', foo_xpi)])
        errors.out = sys.stderr


if __name__ == '__main__':
    mozunit.main()
