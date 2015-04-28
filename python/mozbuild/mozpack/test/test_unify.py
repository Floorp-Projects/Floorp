# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozbuild.util import ensureParentDir

from mozpack.unify import (
    UnifiedFinder,
    UnifiedBuildFinder,
)
import mozunit
from mozpack.test.test_files import TestWithTmpDir
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
        ensureParentDir(file)
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
        finder = UnifiedBuildFinder(FileFinder(self.tmppath('a')),
                                    FileFinder(self.tmppath('b')))

        # Test chrome.manifest unification
        self.create_both('chrome.manifest', 'a\nb\nc\n')
        self.create_one('a', 'chrome/chrome.manifest', 'a\nb\nc\n')
        self.create_one('b', 'chrome/chrome.manifest', 'b\nc\na\n')
        self.assertEqual(sorted([(f, c.open().read()) for f, c in
                                 finder.find('**/chrome.manifest')]),
                         [('chrome.manifest', 'a\nb\nc\n'),
                          ('chrome/chrome.manifest', 'a\nb\nc\n')])

        # Test buildconfig.html unification
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

        # Test xpi file unification
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

        # Test install.rdf unification
        x86_64 = 'Darwin_x86_64-gcc3'
        x86 = 'Darwin_x86-gcc3'
        target_tag = '<{em}targetPlatform>{platform}</{em}targetPlatform>'
        target_attr = '{em}targetPlatform="{platform}" '

        rdf_tag = ''.join([
            '<{RDF}Description {em}bar="bar" {em}qux="qux">',
              '<{em}foo>foo</{em}foo>',
              '{targets}',
              '<{em}baz>baz</{em}baz>',
            '</{RDF}Description>'
        ])
        rdf_attr = ''.join([
            '<{RDF}Description {em}bar="bar" {attr}{em}qux="qux">',
              '{targets}',
              '<{em}foo>foo</{em}foo><{em}baz>baz</{em}baz>',
            '</{RDF}Description>'
        ])

        for descr_ns, target_ns in (('RDF:', ''), ('', 'em:'), ('RDF:', 'em:')):
            # First we need to infuse the above strings with our namespaces and
            # platform values.
            ns = { 'RDF': descr_ns, 'em': target_ns }
            target_tag_x86_64 = target_tag.format(platform=x86_64, **ns)
            target_tag_x86 = target_tag.format(platform=x86, **ns)
            target_attr_x86_64 = target_attr.format(platform=x86_64, **ns)
            target_attr_x86 = target_attr.format(platform=x86, **ns)

            tag_x86_64 = rdf_tag.format(targets=target_tag_x86_64, **ns)
            tag_x86 = rdf_tag.format(targets=target_tag_x86, **ns)
            tag_merged = rdf_tag.format(targets=target_tag_x86_64 + target_tag_x86, **ns)
            tag_empty = rdf_tag.format(targets="", **ns)

            attr_x86_64 = rdf_attr.format(attr=target_attr_x86_64, targets="", **ns)
            attr_x86 = rdf_attr.format(attr=target_attr_x86, targets="", **ns)
            attr_merged = rdf_attr.format(attr="", targets=target_tag_x86_64 + target_tag_x86, **ns)

            # This table defines the test cases, columns "a" and "b" being the
            # contents of the install.rdf of the respective platform and
            # "result" the exepected merged content after unification.
            testcases = (
              #_____a_____  _____b_____  ___result___#
              (tag_x86_64,  tag_x86,     tag_merged  ),
              (tag_x86_64,  tag_empty,   tag_empty   ),
              (tag_empty,   tag_x86,     tag_empty   ),
              (tag_empty,   tag_empty,   tag_empty   ),

              (attr_x86_64, attr_x86,    attr_merged ),
              (tag_x86_64,  attr_x86,    tag_merged  ),
              (attr_x86_64, tag_x86,     attr_merged ),

              (attr_x86_64, tag_empty,   tag_empty   ),
              (tag_empty,   attr_x86,    tag_empty   )
            )

            # Now create the files from the above table and compare
            results = []
            for emid, (rdf_a, rdf_b, result) in enumerate(testcases):
                filename = 'ext/id{0}/install.rdf'.format(emid)
                self.create_one('a', filename, rdf_a)
                self.create_one('b', filename, rdf_b)
                results.append((filename, result))

            self.assertEqual(sorted([(f, c.open().read()) for f, c in
                                     finder.find('**/install.rdf')]), results)


if __name__ == '__main__':
    mozunit.main()
