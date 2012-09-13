#!/usr/bin/env python

import os
import shutil
import tempfile
import unittest
from manifestparser import convert
from manifestparser import ManifestParser
from StringIO import StringIO

here = os.path.dirname(os.path.abspath(__file__))

class TestManifestparser(unittest.TestCase):
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

        # You can query multiple times if you need to::
        flowers = parser.get(foo='bar')
        self.assertEqual(len(flowers), 2)

        # Using the inverse flag should invert the set of tests returned:
        self.assertEqual(parser.get('name', inverse=True, tags=['red']),
                         ['crash-handling', 'fleem'])

        # All of the included tests actually exist::
        self.assertEqual([i['name'] for i in parser.missing()], [])

        # Write the output to a manifest:
        buffer = StringIO()
        parser.write(fp=buffer, global_kwargs={'foo': 'bar'})
        self.assertEqual(buffer.getvalue().strip(),
                         '[DEFAULT]\nfoo = bar\n\n[fleem]\n\n[include/flowers]\nblue = ocean\nred = roses\nyellow = submarine')


    def test_directory_to_manifest(self):
        """
        Test our ability to convert a static directory structure to a
        manifest.
        """

        # First, stub out a directory with files in it::
        def create_stub():
            directory = tempfile.mkdtemp()
            for i in 'foo', 'bar', 'fleem':
                file(os.path.join(directory, i), 'w').write(i)
            subdir = os.path.join(directory, 'subdir')
            os.mkdir(subdir)
            file(os.path.join(subdir, 'subfile'), 'w').write('baz')
            return directory
        stub = create_stub()
        self.assertTrue(os.path.exists(stub) and os.path.isdir(stub))

        # Make a manifest for it:
        self.assertEqual(convert([stub]),
                         """[bar]
[fleem]
[foo]
[subdir/subfile]""")
        shutil.rmtree(stub) # cleanup

        # Now do the same thing but keep the manifests in place:
        stub = create_stub()
        convert([stub], write='manifest.ini')
        self.assertEqual(sorted(os.listdir(stub)),
                         ['bar', 'fleem', 'foo', 'manifest.ini', 'subdir'])
        parser = ManifestParser()
        parser.read(os.path.join(stub, 'manifest.ini'))
        self.assertEqual([i['name'] for i in parser.tests],
                         ['subfile', 'bar', 'fleem', 'foo'])
        parser = ManifestParser()
        parser.read(os.path.join(stub, 'subdir', 'manifest.ini'))
        self.assertEqual(len(parser.tests), 1)
        self.assertEqual(parser.tests[0]['name'], 'subfile')
        shutil.rmtree(stub)


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


    def test_update(self):
        """
        Test our ability to update tests from a manifest and a directory of
        files
        """

        # boilerplate
        tempdir = tempfile.mkdtemp()
        for i in range(10):
            file(os.path.join(tempdir, str(i)), 'w').write(str(i))

        # First, make a manifest:
        manifest = convert([tempdir])
        newtempdir = tempfile.mkdtemp()
        manifest_file = os.path.join(newtempdir, 'manifest.ini')
        file(manifest_file,'w').write(manifest)
        manifest = ManifestParser(manifests=(manifest_file,))
        self.assertEqual(manifest.get('name'),
                         [str(i) for i in range(10)])

        # All of the tests are initially missing:
        self.assertEqual([i['name'] for i in manifest.missing()],
                         [str(i) for i in range(10)])

        # But then we copy one over:
        self.assertEqual(manifest.get('name', name='1'), ['1'])
        manifest.update(tempdir, name='1')
        self.assertEqual(sorted(os.listdir(newtempdir)),
                         ['1', 'manifest.ini'])

        # Update that one file and copy all the "tests":
        file(os.path.join(tempdir, '1'), 'w').write('secret door')
        manifest.update(tempdir)
        self.assertEqual(sorted(os.listdir(newtempdir)),
                         ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'manifest.ini'])
        self.assertEqual(file(os.path.join(newtempdir, '1')).read().strip(),
                         'secret door')

        # clean up:
        shutil.rmtree(tempdir)
        shutil.rmtree(newtempdir)

    def test_path_override(self):
        """You can override the path in the section too.
        This shows that you can use a relative path"""
        path_example = os.path.join(here, 'path-example.ini')
        manifest = ManifestParser(manifests=(path_example,))
        self.assertEqual(manifest.tests[0]['path'],
                         os.path.join(here, 'fleem'))


if __name__ == '__main__':
    unittest.main()
