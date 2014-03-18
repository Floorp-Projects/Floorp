#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import tempfile
import unittest
from manifestparser import convert
from manifestparser import ManifestParser
from StringIO import StringIO

here = os.path.dirname(os.path.abspath(__file__))

class TestManifestParser(unittest.TestCase):
    """
    Test the manifest parser

    You must have ManifestDestiny installed before running these tests.
    Run ``python manifestparser.py setup develop`` with setuptools installed.
    """

    def test_sanity(self):
        """Ensure basic parser is sane"""

        parser = ManifestParser()
        mozmill_example = os.path.join(here, 'mozmill-example.ini')
        parser.read(mozmill_example)
        tests = parser.tests
        self.assertEqual(len(tests), len(file(mozmill_example).read().strip().splitlines()))

        # Ensure that capitalization and order aren't an issue:
        lines = ['[%s]' % test['name'] for test in tests]
        self.assertEqual(lines, file(mozmill_example).read().strip().splitlines())

        # Show how you select subsets of tests:
        mozmill_restart_example = os.path.join(here, 'mozmill-restart-example.ini')
        parser.read(mozmill_restart_example)
        restart_tests = parser.get(type='restart')
        self.assertTrue(len(restart_tests) < len(parser.tests))
        self.assertEqual(len(restart_tests), len(parser.get(manifest=mozmill_restart_example)))
        self.assertFalse([test for test in restart_tests
                          if test['manifest'] != os.path.join(here, 'mozmill-restart-example.ini')])
        self.assertEqual(parser.get('name', tags=['foo']),
                         ['restartTests/testExtensionInstallUninstall/test2.js',
                          'restartTests/testExtensionInstallUninstall/test1.js'])
        self.assertEqual(parser.get('name', foo='bar'),
                         ['restartTests/testExtensionInstallUninstall/test2.js'])

    def test_include(self):
        """Illustrate how include works"""

        include_example = os.path.join(here, 'include-example.ini')
        parser = ManifestParser(manifests=(include_example,))

        # All of the tests should be included, in order:
        self.assertEqual(parser.get('name'),
                         ['crash-handling', 'fleem', 'flowers'])
        self.assertEqual([(test['name'], os.path.basename(test['manifest'])) for test in parser.tests],
                         [('crash-handling', 'bar.ini'), ('fleem', 'include-example.ini'), ('flowers', 'foo.ini')])


        # The manifests should be there too:
        self.assertEqual(len(parser.manifests()), 3)

        # We already have the root directory:
        self.assertEqual(here, parser.rootdir)


        # DEFAULT values should persist across includes, unless they're
        # overwritten.  In this example, include-example.ini sets foo=bar, but
        # it's overridden to fleem in bar.ini
        self.assertEqual(parser.get('name', foo='bar'),
                         ['fleem', 'flowers'])
        self.assertEqual(parser.get('name', foo='fleem'),
                         ['crash-handling'])

        # Passing parameters in the include section allows defining variables in
        #the submodule scope:
        self.assertEqual(parser.get('name', tags=['red']),
                         ['flowers'])

        # However, this should be overridable from the DEFAULT section in the
        # included file and that overridable via the key directly connected to
        # the test:
        self.assertEqual(parser.get(name='flowers')[0]['blue'],
                         'ocean')
        self.assertEqual(parser.get(name='flowers')[0]['yellow'],
                         'submarine')

        # You can query multiple times if you need to:
        flowers = parser.get(foo='bar')
        self.assertEqual(len(flowers), 2)

        # Using the inverse flag should invert the set of tests returned:
        self.assertEqual(parser.get('name', inverse=True, tags=['red']),
                         ['crash-handling', 'fleem'])

        # All of the included tests actually exist:
        self.assertEqual([i['name'] for i in parser.missing()], [])

        # Write the output to a manifest:
        buffer = StringIO()
        parser.write(fp=buffer, global_kwargs={'foo': 'bar'})
        self.assertEqual(buffer.getvalue().strip(),
                         '[DEFAULT]\nfoo = bar\n\n[fleem]\n\n[include/flowers]\nblue = ocean\nred = roses\nyellow = submarine')

    def test_copy(self):
        """Test our ability to copy a set of manifests"""

        tempdir = tempfile.mkdtemp()
        include_example = os.path.join(here, 'include-example.ini')
        manifest = ManifestParser(manifests=(include_example,))
        manifest.copy(tempdir)
        self.assertEqual(sorted(os.listdir(tempdir)),
                         ['fleem', 'include', 'include-example.ini'])
        self.assertEqual(sorted(os.listdir(os.path.join(tempdir, 'include'))),
                         ['bar.ini', 'crash-handling', 'flowers', 'foo.ini'])
        from_manifest = ManifestParser(manifests=(include_example,))
        to_manifest = os.path.join(tempdir, 'include-example.ini')
        to_manifest = ManifestParser(manifests=(to_manifest,))
        self.assertEqual(to_manifest.get('name'), from_manifest.get('name'))
        shutil.rmtree(tempdir)

    def test_path_override(self):
        """You can override the path in the section too.
        This shows that you can use a relative path"""
        path_example = os.path.join(here, 'path-example.ini')
        manifest = ManifestParser(manifests=(path_example,))
        self.assertEqual(manifest.tests[0]['path'],
                         os.path.join(here, 'fleem'))

    def test_relative_path(self):
        """
        Relative test paths are correctly calculated.
        """
        relative_path = os.path.join(here, 'relative-path.ini')
        manifest = ManifestParser(manifests=(relative_path,))
        self.assertEqual(manifest.tests[0]['path'],
                         os.path.join(os.path.dirname(here), 'fleem'))
        self.assertEqual(manifest.tests[0]['relpath'],
                         os.path.join('..', 'fleem'))
        self.assertEqual(manifest.tests[1]['relpath'],
                         os.path.join('..', 'testsSIBLING', 'example'))

    def test_path_from_fd(self):
        """
        Test paths are left untouched when manifest is a file-like object.
        """
        fp = StringIO("[section]\npath=fleem")
        manifest = ManifestParser(manifests=(fp,))
        self.assertEqual(manifest.tests[0]['path'], 'fleem')
        self.assertEqual(manifest.tests[0]['relpath'], 'fleem')
        self.assertEqual(manifest.tests[0]['manifest'], None)

    def test_comments(self):
        """
        ensure comments work, see
        https://bugzilla.mozilla.org/show_bug.cgi?id=813674
        """
        comment_example = os.path.join(here, 'comment-example.ini')
        manifest = ManifestParser(manifests=(comment_example,))
        self.assertEqual(len(manifest.tests), 8)
        names = [i['name'] for i in manifest.tests]
        self.assertFalse('test_0202_app_launch_apply_update_dirlocked.js' in names)

    def test_verifyDirectory(self):

        directory = os.path.join(here, 'verifyDirectory')

        # correct manifest
        manifest_path = os.path.join(directory, 'verifyDirectory.ini')
        manifest = ManifestParser(manifests=(manifest_path,))
        missing = manifest.verifyDirectory(directory, extensions=('.js',))
        self.assertEqual(missing, (set(), set()))

        # manifest is missing test_1.js
        test_1 = os.path.join(directory, 'test_1.js')
        manifest_path = os.path.join(directory, 'verifyDirectory_incomplete.ini')
        manifest = ManifestParser(manifests=(manifest_path,))
        missing = manifest.verifyDirectory(directory, extensions=('.js',))
        self.assertEqual(missing, (set(), set([test_1])))

        # filesystem is missing test_notappearinginthisfilm.js
        missing_test = os.path.join(directory, 'test_notappearinginthisfilm.js')
        manifest_path = os.path.join(directory, 'verifyDirectory_toocomplete.ini')
        manifest = ManifestParser(manifests=(manifest_path,))
        missing = manifest.verifyDirectory(directory, extensions=('.js',))
        self.assertEqual(missing, (set([missing_test]), set()))

    def test_just_defaults(self):
        """Ensure a manifest with just a DEFAULT section exposes that data."""

        parser = ManifestParser()
        manifest = os.path.join(here, 'just-defaults.ini')
        parser.read(manifest)
        self.assertEqual(len(parser.tests), 0)
        self.assertTrue(manifest in parser.manifest_defaults)
        self.assertEquals(parser.manifest_defaults[manifest]['foo'], 'bar')

    def test_manifest_list(self):
        """
        Ensure a manifest with just a DEFAULT section still returns
        itself from the manifests() method.
        """

        parser = ManifestParser()
        manifest = os.path.join(here, 'no-tests.ini')
        parser.read(manifest)
        self.assertEqual(len(parser.tests), 0)
        self.assertTrue(len(parser.manifests()) == 1)

if __name__ == '__main__':
    unittest.main()
