# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import unittest

from collections import defaultdict
from buildconfig import topsrcdir
from mozunit import main

from mozbuild.frontend.context import (
    ObjDirPath,
    Path,
)
from mozbuild.frontend.data import (
    AndroidResDirs,
    BrandingFiles,
    ChromeManifestEntry,
    ConfigFileSubstitution,
    Defines,
    DirectoryTraversal,
    Exports,
    FinalTargetPreprocessedFiles,
    GeneratedFile,
    GeneratedSources,
    HostDefines,
    HostSources,
    IPDLFile,
    JARManifest,
    LinkageMultipleRustLibrariesError,
    LocalInclude,
    Program,
    RustLibrary,
    SdkFiles,
    SharedLibrary,
    SimpleProgram,
    Sources,
    StaticLibrary,
    TestHarnessFiles,
    TestManifest,
    UnifiedSources,
    VariablePassthru,
)
from mozbuild.frontend.emitter import TreeMetadataEmitter
from mozbuild.frontend.reader import (
    BuildReader,
    BuildReaderError,
    SandboxValidationError,
)
from mozpack.chrome import manifest

from mozbuild.test.common import MockConfig

import mozpack.path as mozpath


data_path = mozpath.abspath(mozpath.dirname(__file__))
data_path = mozpath.join(data_path, 'data')


class TestEmitterBasic(unittest.TestCase):
    def setUp(self):
        self._old_env = dict(os.environ)
        os.environ.pop('MOZ_OBJDIR', None)

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self._old_env)

    def reader(self, name, enable_tests=False, extra_substs=None):
        substs = dict(
            ENABLE_TESTS='1' if enable_tests else '',
            BIN_SUFFIX='.prog',
            OS_TARGET='WINNT',
            COMPILE_ENVIRONMENT='1',
        )
        if extra_substs:
            substs.update(extra_substs)
        config = MockConfig(mozpath.join(data_path, name), extra_substs=substs)

        return BuildReader(config)

    def read_topsrcdir(self, reader, filter_common=True):
        emitter = TreeMetadataEmitter(reader.config)
        objs = list(emitter.emit(reader.read_topsrcdir()))
        self.assertGreater(len(objs), 0)

        filtered = []
        for obj in objs:
            if filter_common and isinstance(obj, DirectoryTraversal):
                continue

            filtered.append(obj)

        return filtered

    def test_dirs_traversal_simple(self):
        reader = self.reader('traversal-simple')
        objs = self.read_topsrcdir(reader, filter_common=False)
        self.assertEqual(len(objs), 4)

        for o in objs:
            self.assertIsInstance(o, DirectoryTraversal)
            self.assertTrue(os.path.isabs(o.context_main_path))
            self.assertEqual(len(o.context_all_paths), 1)

        reldirs = [o.relativedir for o in objs]
        self.assertEqual(reldirs, ['', 'foo', 'foo/biz', 'bar'])

        dirs = [[d.full_path for d in o.dirs] for o in objs]
        self.assertEqual(dirs, [
            [
                mozpath.join(reader.config.topsrcdir, 'foo'),
                mozpath.join(reader.config.topsrcdir, 'bar')
            ], [
                mozpath.join(reader.config.topsrcdir, 'foo', 'biz')
            ], [], []])

    def test_traversal_all_vars(self):
        reader = self.reader('traversal-all-vars')
        objs = self.read_topsrcdir(reader, filter_common=False)
        self.assertEqual(len(objs), 2)

        for o in objs:
            self.assertIsInstance(o, DirectoryTraversal)

        reldirs = set([o.relativedir for o in objs])
        self.assertEqual(reldirs, set(['', 'regular']))

        for o in objs:
            reldir = o.relativedir

            if reldir == '':
                self.assertEqual([d.full_path for d in o.dirs], [
                    mozpath.join(reader.config.topsrcdir, 'regular')])

    def test_traversal_all_vars_enable_tests(self):
        reader = self.reader('traversal-all-vars', enable_tests=True)
        objs = self.read_topsrcdir(reader, filter_common=False)
        self.assertEqual(len(objs), 3)

        for o in objs:
            self.assertIsInstance(o, DirectoryTraversal)

        reldirs = set([o.relativedir for o in objs])
        self.assertEqual(reldirs, set(['', 'regular', 'test']))

        for o in objs:
            reldir = o.relativedir

            if reldir == '':
                self.assertEqual([d.full_path for d in o.dirs], [
                    mozpath.join(reader.config.topsrcdir, 'regular'),
                    mozpath.join(reader.config.topsrcdir, 'test')])

    def test_config_file_substitution(self):
        reader = self.reader('config-file-substitution')
        objs = self.read_topsrcdir(reader)
        self.assertEqual(len(objs), 2)

        self.assertIsInstance(objs[0], ConfigFileSubstitution)
        self.assertIsInstance(objs[1], ConfigFileSubstitution)

        topobjdir = mozpath.abspath(reader.config.topobjdir)
        self.assertEqual(objs[0].relpath, 'foo')
        self.assertEqual(mozpath.normpath(objs[0].output_path),
            mozpath.normpath(mozpath.join(topobjdir, 'foo')))
        self.assertEqual(mozpath.normpath(objs[1].output_path),
            mozpath.normpath(mozpath.join(topobjdir, 'bar')))

    def test_variable_passthru(self):
        reader = self.reader('variable-passthru')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], VariablePassthru)

        wanted = {
            'ALLOW_COMPILER_WARNINGS': True,
            'DISABLE_STL_WRAPPING': True,
            'NO_DIST_INSTALL': True,
            'VISIBILITY_FLAGS': '',
            'RCFILE': 'foo.rc',
            'RESFILE': 'bar.res',
            'RCINCLUDE': 'bar.rc',
            'DEFFILE': 'baz.def',
            'MOZBUILD_CFLAGS': ['-fno-exceptions', '-w'],
            'MOZBUILD_CXXFLAGS': ['-fcxx-exceptions', '-include foo.h'],
            'MOZBUILD_LDFLAGS': ['-framework Foo', '-x', '-DELAYLOAD:foo.dll',
                                 '-DELAYLOAD:bar.dll'],
            'MOZBUILD_HOST_CFLAGS': ['-funroll-loops', '-wall'],
            'MOZBUILD_HOST_CXXFLAGS': ['-funroll-loops-harder',
                                       '-wall-day-everyday'],
            'WIN32_EXE_LDFLAGS': ['-subsystem:console'],
        }

        variables = objs[0].variables
        maxDiff = self.maxDiff
        self.maxDiff = None
        self.assertEqual(wanted, variables)
        self.maxDiff = maxDiff

    def test_use_yasm(self):
        # When yasm is not available, this should raise.
        reader = self.reader('use-yasm')
        with self.assertRaisesRegexp(SandboxValidationError,
            'yasm is not available'):
            self.read_topsrcdir(reader)

        # When yasm is available, this should work.
        reader = self.reader('use-yasm',
                             extra_substs=dict(
                                 YASM='yasm',
                                 YASM_ASFLAGS='-foo',
                             ))
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], VariablePassthru)
        maxDiff = self.maxDiff
        self.maxDiff = None
        self.assertEqual(objs[0].variables,
                         {'AS': 'yasm',
                          'ASFLAGS': '-foo',
                          'AS_DASH_C_FLAG': ''})
        self.maxDiff = maxDiff


    def test_generated_files(self):
        reader = self.reader('generated-files')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 3)
        for o in objs:
            self.assertIsInstance(o, GeneratedFile)

        expected = ['bar.c', 'foo.c', ('xpidllex.py', 'xpidlyacc.py'), ]
        for o, f in zip(objs, expected):
            expected_filename = f if isinstance(f, tuple) else (f,)
            self.assertEqual(o.outputs, expected_filename)
            self.assertEqual(o.script, None)
            self.assertEqual(o.method, None)
            self.assertEqual(o.inputs, [])

    def test_generated_files_method_names(self):
        reader = self.reader('generated-files-method-names')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 2)
        for o in objs:
            self.assertIsInstance(o, GeneratedFile)

        expected = ['bar.c', 'foo.c']
        expected_method_names = ['make_bar', 'main']
        for o, expected_filename, expected_method in zip(objs, expected, expected_method_names):
            self.assertEqual(o.outputs, (expected_filename,))
            self.assertEqual(o.method, expected_method)
            self.assertEqual(o.inputs, [])

    def test_generated_files_absolute_script(self):
        reader = self.reader('generated-files-absolute-script')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)

        o = objs[0]
        self.assertIsInstance(o, GeneratedFile)
        self.assertEqual(o.outputs, ('bar.c',))
        self.assertRegexpMatches(o.script, 'script.py$')
        self.assertEqual(o.method, 'make_bar')
        self.assertEqual(o.inputs, [])

    def test_generated_files_no_script(self):
        reader = self.reader('generated-files-no-script')
        with self.assertRaisesRegexp(SandboxValidationError,
            'Script for generating bar.c does not exist'):
            self.read_topsrcdir(reader)

    def test_generated_files_no_inputs(self):
        reader = self.reader('generated-files-no-inputs')
        with self.assertRaisesRegexp(SandboxValidationError,
            'Input for generating foo.c does not exist'):
            self.read_topsrcdir(reader)

    def test_generated_files_no_python_script(self):
        reader = self.reader('generated-files-no-python-script')
        with self.assertRaisesRegexp(SandboxValidationError,
            'Script for generating bar.c does not end in .py'):
            self.read_topsrcdir(reader)

    def test_exports(self):
        reader = self.reader('exports')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], Exports)

        expected = [
            ('', ['foo.h', 'bar.h', 'baz.h']),
            ('mozilla', ['mozilla1.h', 'mozilla2.h']),
            ('mozilla/dom', ['dom1.h', 'dom2.h', 'dom3.h']),
            ('mozilla/gfx', ['gfx.h']),
            ('nspr/private', ['pprio.h', 'pprthred.h']),
            ('vpx', ['mem.h', 'mem2.h']),
        ]
        for (expect_path, expect_headers), (actual_path, actual_headers) in \
                zip(expected, [(path, list(seq)) for path, seq in objs[0].files.walk()]):
            self.assertEqual(expect_path, actual_path)
            self.assertEqual(expect_headers, actual_headers)

    def test_exports_missing(self):
        '''
        Missing files in EXPORTS is an error.
        '''
        reader = self.reader('exports-missing')
        with self.assertRaisesRegexp(SandboxValidationError,
             'File listed in EXPORTS does not exist:'):
            self.read_topsrcdir(reader)

    def test_exports_missing_generated(self):
        '''
        An objdir file in EXPORTS that is not in GENERATED_FILES is an error.
        '''
        reader = self.reader('exports-missing-generated')
        with self.assertRaisesRegexp(SandboxValidationError,
             'Objdir file listed in EXPORTS not in GENERATED_FILES:'):
            self.read_topsrcdir(reader)

    def test_exports_generated(self):
        reader = self.reader('exports-generated')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 2)
        self.assertIsInstance(objs[0], GeneratedFile)
        self.assertIsInstance(objs[1], Exports)
        exports = [(path, list(seq)) for path, seq in objs[1].files.walk()]
        self.assertEqual(exports,
                         [('', ['foo.h']),
                          ('mozilla', ['mozilla1.h', '!mozilla2.h'])])
        path, files = exports[1]
        self.assertIsInstance(files[1], ObjDirPath)

    def test_test_harness_files(self):
        reader = self.reader('test-harness-files')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], TestHarnessFiles)

        expected = {
            'mochitest': ['runtests.py', 'utils.py'],
            'testing/mochitest': ['mochitest.py', 'mochitest.ini'],
        }

        for path, strings in objs[0].files.walk():
            self.assertTrue(path in expected)
            basenames = sorted(mozpath.basename(s) for s in strings)
            self.assertEqual(sorted(expected[path]), basenames)

    def test_test_harness_files_root(self):
        reader = self.reader('test-harness-files-root')
        with self.assertRaisesRegexp(SandboxValidationError,
            'Cannot install files to the root of TEST_HARNESS_FILES'):
            self.read_topsrcdir(reader)

    def test_branding_files(self):
        reader = self.reader('branding-files')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], BrandingFiles)

        files = objs[0].files

        self.assertEqual(files._strings, ['bar.ico', 'baz.png', 'foo.xpm'])

        self.assertIn('icons', files._children)
        icons = files._children['icons']

        self.assertEqual(icons._strings, ['quux.icns'])

    def test_sdk_files(self):
        reader = self.reader('sdk-files')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], SdkFiles)

        files = objs[0].files

        self.assertEqual(files._strings, ['bar.ico', 'baz.png', 'foo.xpm'])

        self.assertIn('icons', files._children)
        icons = files._children['icons']

        self.assertEqual(icons._strings, ['quux.icns'])

    def test_program(self):
        reader = self.reader('program')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 3)
        self.assertIsInstance(objs[0], Program)
        self.assertIsInstance(objs[1], SimpleProgram)
        self.assertIsInstance(objs[2], SimpleProgram)

        self.assertEqual(objs[0].program, 'test_program.prog')
        self.assertEqual(objs[1].program, 'test_program1.prog')
        self.assertEqual(objs[2].program, 'test_program2.prog')

    def test_test_manifest_missing_manifest(self):
        """A missing manifest file should result in an error."""
        reader = self.reader('test-manifest-missing-manifest')

        with self.assertRaisesRegexp(BuildReaderError, 'IOError: Missing files'):
            self.read_topsrcdir(reader)

    def test_empty_test_manifest_rejected(self):
        """A test manifest without any entries is rejected."""
        reader = self.reader('test-manifest-empty')

        with self.assertRaisesRegexp(SandboxValidationError, 'Empty test manifest'):
            self.read_topsrcdir(reader)


    def test_test_manifest_just_support_files(self):
        """A test manifest with no tests but support-files is not supported."""
        reader = self.reader('test-manifest-just-support')

        with self.assertRaisesRegexp(SandboxValidationError, 'Empty test manifest'):
            self.read_topsrcdir(reader)

    def test_test_manifest_dupe_support_files(self):
        """A test manifest with dupe support-files in a single test is not
        supported.
        """
        reader = self.reader('test-manifest-dupes')

        with self.assertRaisesRegexp(SandboxValidationError, 'bar.js appears multiple times '
            'in a test manifest under a support-files field, please omit the duplicate entry.'):
            self.read_topsrcdir(reader)

    def test_test_manifest_absolute_support_files(self):
        """Support files starting with '/' are placed relative to the install root"""
        reader = self.reader('test-manifest-absolute-support')

        objs = self.read_topsrcdir(reader)
        self.assertEqual(len(objs), 1)
        o = objs[0]
        self.assertEqual(len(o.installs), 3)
        expected = [
            mozpath.normpath(mozpath.join(o.install_prefix, "../.well-known/foo.txt")),
            mozpath.join(o.install_prefix, "absolute-support.ini"),
            mozpath.join(o.install_prefix, "test_file.js"),
        ]
        paths = sorted([v[0] for v in o.installs.values()])
        self.assertEqual(paths, expected)

    @unittest.skip('Bug 1304316 - Items in the second set but not the first')
    def test_test_manifest_shared_support_files(self):
        """Support files starting with '!' are given separate treatment, so their
        installation can be resolved when running tests.
        """
        reader = self.reader('test-manifest-shared-support')
        supported, child = self.read_topsrcdir(reader)

        expected_deferred_installs = {
            '!/child/test_sub.js',
            '!/child/another-file.sjs',
            '!/child/data/**',
        }

        self.assertEqual(len(supported.installs), 3)
        self.assertEqual(set(supported.deferred_installs),
                         expected_deferred_installs)
        self.assertEqual(len(child.installs), 3)
        self.assertEqual(len(child.pattern_installs), 1)

    def test_test_manifest_deffered_install_missing(self):
        """A non-existent shared support file reference produces an error."""
        reader = self.reader('test-manifest-shared-missing')

        with self.assertRaisesRegexp(SandboxValidationError,
                                     'entry in support-files not present in the srcdir'):
            self.read_topsrcdir(reader)

    def test_test_manifest_install_to_subdir(self):
        """ """
        reader = self.reader('test-manifest-install-subdir')

        objs = self.read_topsrcdir(reader)
        self.assertEqual(len(objs), 1)
        o = objs[0]
        self.assertEqual(len(o.installs), 3)
        self.assertEqual(o.manifest_relpath, "subdir.ini")
        self.assertEqual(o.manifest_obj_relpath, "subdir/subdir.ini")
        expected = [
            mozpath.normpath(mozpath.join(o.install_prefix, "subdir/subdir.ini")),
            mozpath.normpath(mozpath.join(o.install_prefix, "subdir/support.txt")),
            mozpath.normpath(mozpath.join(o.install_prefix, "subdir/test_foo.html")),
        ]
        paths = sorted([v[0] for v in o.installs.values()])
        self.assertEqual(paths, expected)

    def test_test_manifest_install_includes(self):
        """Ensure that any [include:foo.ini] are copied to the objdir."""
        reader = self.reader('test-manifest-install-includes')

        objs = self.read_topsrcdir(reader)
        self.assertEqual(len(objs), 1)
        o = objs[0]
        self.assertEqual(len(o.installs), 3)
        self.assertEqual(o.manifest_relpath, "mochitest.ini")
        self.assertEqual(o.manifest_obj_relpath, "subdir/mochitest.ini")
        expected = [
            mozpath.normpath(mozpath.join(o.install_prefix, "subdir/common.ini")),
            mozpath.normpath(mozpath.join(o.install_prefix, "subdir/mochitest.ini")),
            mozpath.normpath(mozpath.join(o.install_prefix, "subdir/test_foo.html")),
        ]
        paths = sorted([v[0] for v in o.installs.values()])
        self.assertEqual(paths, expected)

    def test_test_manifest_includes(self):
        """Ensure that manifest objects from the emitter list a correct manifest.
        """
        reader = self.reader('test-manifest-emitted-includes')
        [obj] = self.read_topsrcdir(reader)

        # Expected manifest leafs for our tests.
        expected_manifests = {
            'reftest1.html': 'reftest.list',
            'reftest1-ref.html': 'reftest.list',
            'reftest2.html': 'included-reftest.list',
            'reftest2-ref.html': 'included-reftest.list',
        }

        for t in obj.tests:
            self.assertTrue(t['manifest'].endswith(expected_manifests[t['name']]))

    def test_test_manifest_keys_extracted(self):
        """Ensure all metadata from test manifests is extracted."""
        reader = self.reader('test-manifest-keys-extracted')

        objs = [o for o in self.read_topsrcdir(reader)
                if isinstance(o, TestManifest)]

        self.assertEqual(len(objs), 9)

        metadata = {
            'a11y.ini': {
                'flavor': 'a11y',
                'installs': {
                    'a11y.ini': False,
                    'test_a11y.js': True,
                },
                'pattern-installs': 1,
            },
            'browser.ini': {
                'flavor': 'browser-chrome',
                'installs': {
                    'browser.ini': False,
                    'test_browser.js': True,
                    'support1': False,
                    'support2': False,
                },
            },
            'metro.ini': {
                'flavor': 'metro-chrome',
                'installs': {
                    'metro.ini': False,
                    'test_metro.js': True,
                },
            },
            'mochitest.ini': {
                'flavor': 'mochitest',
                'installs': {
                    'mochitest.ini': False,
                    'test_mochitest.js': True,
                },
                'external': {
                    'external1',
                    'external2',
                },
            },
            'chrome.ini': {
                'flavor': 'chrome',
                'installs': {
                    'chrome.ini': False,
                    'test_chrome.js': True,
                },
            },
            'xpcshell.ini': {
                'flavor': 'xpcshell',
                'dupe': True,
                'installs': {
                    'xpcshell.ini': False,
                    'test_xpcshell.js': True,
                    'head1': False,
                    'head2': False,
                    'tail1': False,
                    'tail2': False,
                },
            },
            'reftest.list': {
                'flavor': 'reftest',
                'installs': {},
            },
            'crashtest.list': {
                'flavor': 'crashtest',
                'installs': {},
            },
            'python.ini': {
                'flavor': 'python',
                'installs': {
                    'python.ini': False,
                },
            }
        }

        for o in objs:
            m = metadata[mozpath.basename(o.manifest_relpath)]

            self.assertTrue(o.path.startswith(o.directory))
            self.assertEqual(o.flavor, m['flavor'])
            self.assertEqual(o.dupe_manifest, m.get('dupe', False))

            external_normalized = set(mozpath.basename(p) for p in
                    o.external_installs)
            self.assertEqual(external_normalized, m.get('external', set()))

            self.assertEqual(len(o.installs), len(m['installs']))
            for path in o.installs.keys():
                self.assertTrue(path.startswith(o.directory))
                relpath = path[len(o.directory)+1:]

                self.assertIn(relpath, m['installs'])
                self.assertEqual(o.installs[path][1], m['installs'][relpath])

            if 'pattern-installs' in m:
                self.assertEqual(len(o.pattern_installs), m['pattern-installs'])

    def test_test_manifest_unmatched_generated(self):
        reader = self.reader('test-manifest-unmatched-generated')

        with self.assertRaisesRegexp(SandboxValidationError,
            'entry in generated-files not present elsewhere'):
            self.read_topsrcdir(reader),

    def test_test_manifest_parent_support_files_dir(self):
        """support-files referencing a file in a parent directory works."""
        reader = self.reader('test-manifest-parent-support-files-dir')

        objs = [o for o in self.read_topsrcdir(reader)
                if isinstance(o, TestManifest)]

        self.assertEqual(len(objs), 1)

        o = objs[0]

        expected = mozpath.join(o.srcdir, 'support-file.txt')
        self.assertIn(expected, o.installs)
        self.assertEqual(o.installs[expected],
            ('testing/mochitest/tests/child/support-file.txt', False))

    def test_test_manifest_missing_test_error(self):
        """Missing test files should result in error."""
        reader = self.reader('test-manifest-missing-test-file')

        with self.assertRaisesRegexp(SandboxValidationError,
            'lists test that does not exist: test_missing.html'):
            self.read_topsrcdir(reader)

    def test_test_manifest_missing_test_error_unfiltered(self):
        """Missing test files should result in error, even when the test list is not filtered."""
        reader = self.reader('test-manifest-missing-test-file-unfiltered')

        with self.assertRaisesRegexp(SandboxValidationError,
            'lists test that does not exist: missing.js'):
            self.read_topsrcdir(reader)

    def test_ipdl_sources(self):
        reader = self.reader('ipdl_sources')
        objs = self.read_topsrcdir(reader)

        ipdls = []
        for o in objs:
            if isinstance(o, IPDLFile):
                ipdls.append('%s/%s' % (o.relativedir, o.basename))

        expected = [
            'bar/bar.ipdl',
            'bar/bar2.ipdlh',
            'foo/foo.ipdl',
            'foo/foo2.ipdlh',
        ]

        self.assertEqual(ipdls, expected)

    def test_local_includes(self):
        """Test that LOCAL_INCLUDES is emitted correctly."""
        reader = self.reader('local_includes')
        objs = self.read_topsrcdir(reader)

        local_includes = [o.path for o in objs if isinstance(o, LocalInclude)]
        expected = [
            '/bar/baz',
            'foo',
        ]

        self.assertEqual(local_includes, expected)

        local_includes = [o.path.full_path
                          for o in objs if isinstance(o, LocalInclude)]
        expected = [
            mozpath.join(reader.config.topsrcdir, 'bar/baz'),
            mozpath.join(reader.config.topsrcdir, 'foo'),
        ]

        self.assertEqual(local_includes, expected)

    def test_generated_includes(self):
        """Test that GENERATED_INCLUDES is emitted correctly."""
        reader = self.reader('generated_includes')
        objs = self.read_topsrcdir(reader)

        generated_includes = [o.path for o in objs if isinstance(o, LocalInclude)]
        expected = [
            '!/bar/baz',
            '!foo',
        ]

        self.assertEqual(generated_includes, expected)

        generated_includes = [o.path.full_path
                              for o in objs if isinstance(o, LocalInclude)]
        expected = [
            mozpath.join(reader.config.topobjdir, 'bar/baz'),
            mozpath.join(reader.config.topobjdir, 'foo'),
        ]

        self.assertEqual(generated_includes, expected)

    def test_defines(self):
        reader = self.reader('defines')
        objs = self.read_topsrcdir(reader)

        defines = {}
        for o in objs:
            if isinstance(o, Defines):
                defines = o.defines

        expected = {
            'BAR': 7,
            'BAZ': '"abcd"',
            'FOO': True,
            'VALUE': 'xyz',
            'QUX': False,
        }

        self.assertEqual(defines, expected)

    def test_host_defines(self):
        reader = self.reader('host-defines')
        objs = self.read_topsrcdir(reader)

        defines = {}
        for o in objs:
            if isinstance(o, HostDefines):
                defines = o.defines

        expected = {
            'BAR': 7,
            'BAZ': '"abcd"',
            'FOO': True,
            'VALUE': 'xyz',
            'QUX': False,
        }

        self.assertEqual(defines, expected)

    def test_jar_manifests(self):
        reader = self.reader('jar-manifests')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        for obj in objs:
            self.assertIsInstance(obj, JARManifest)
            self.assertIsInstance(obj.path, Path)

    def test_jar_manifests_multiple_files(self):
        with self.assertRaisesRegexp(SandboxValidationError, 'limited to one value'):
            reader = self.reader('jar-manifests-multiple-files')
            self.read_topsrcdir(reader)

    def test_xpidl_module_no_sources(self):
        """XPIDL_MODULE without XPIDL_SOURCES should be rejected."""
        with self.assertRaisesRegexp(SandboxValidationError, 'XPIDL_MODULE '
            'cannot be defined'):
            reader = self.reader('xpidl-module-no-sources')
            self.read_topsrcdir(reader)

    def test_missing_local_includes(self):
        """LOCAL_INCLUDES containing non-existent directories should be rejected."""
        with self.assertRaisesRegexp(SandboxValidationError, 'Path specified in '
            'LOCAL_INCLUDES does not exist'):
            reader = self.reader('missing-local-includes')
            self.read_topsrcdir(reader)

    def test_library_defines(self):
        """Test that LIBRARY_DEFINES is propagated properly."""
        reader = self.reader('library-defines')
        objs = self.read_topsrcdir(reader)

        libraries = [o for o in objs if isinstance(o,StaticLibrary)]
        expected = {
            'liba': '-DIN_LIBA',
            'libb': '-DIN_LIBA -DIN_LIBB',
            'libc': '-DIN_LIBA -DIN_LIBB',
            'libd': ''
        }
        defines = {}
        for lib in libraries:
            defines[lib.basename] = ' '.join(lib.lib_defines.get_defines())
        self.assertEqual(expected, defines)

    def test_sources(self):
        """Test that SOURCES works properly."""
        reader = self.reader('sources')
        objs = self.read_topsrcdir(reader)

        # The last object is a Linkable.
        linkable = objs.pop()
        self.assertTrue(linkable.cxx_link)
        self.assertEqual(len(objs), 6)
        for o in objs:
            self.assertIsInstance(o, Sources)

        suffix_map = {obj.canonical_suffix: obj for obj in objs}
        self.assertEqual(len(suffix_map), 6)

        expected = {
            '.cpp': ['a.cpp', 'b.cc', 'c.cxx'],
            '.c': ['d.c'],
            '.m': ['e.m'],
            '.mm': ['f.mm'],
            '.S': ['g.S'],
            '.s': ['h.s', 'i.asm'],
        }
        for suffix, files in expected.items():
            sources = suffix_map[suffix]
            self.assertEqual(
                sources.files,
                [mozpath.join(reader.config.topsrcdir, f) for f in files])

    def test_sources_just_c(self):
        """Test that a linkable with no C++ sources doesn't have cxx_link set."""
        reader = self.reader('sources-just-c')
        objs = self.read_topsrcdir(reader)

        # The last object is a Linkable.
        linkable = objs.pop()
        self.assertFalse(linkable.cxx_link)

    def test_linkables_cxx_link(self):
        """Test that linkables transitively set cxx_link properly."""
        reader = self.reader('test-linkables-cxx-link')
        got_results = 0
        for obj in self.read_topsrcdir(reader):
            if isinstance(obj, SharedLibrary):
                if obj.basename == 'cxx_shared':
                    self.assertTrue(obj.cxx_link)
                    got_results += 1
                elif obj.basename == 'just_c_shared':
                    self.assertFalse(obj.cxx_link)
                    got_results += 1
        self.assertEqual(got_results, 2)

    def test_generated_sources(self):
        """Test that GENERATED_SOURCES works properly."""
        reader = self.reader('generated-sources')
        objs = self.read_topsrcdir(reader)

        # The last object is a Linkable.
        linkable = objs.pop()
        self.assertTrue(linkable.cxx_link)
        self.assertEqual(len(objs), 6)

        generated_sources = [o for o in objs if isinstance(o, GeneratedSources)]
        self.assertEqual(len(generated_sources), 6)

        suffix_map = {obj.canonical_suffix: obj for obj in generated_sources}
        self.assertEqual(len(suffix_map), 6)

        expected = {
            '.cpp': ['a.cpp', 'b.cc', 'c.cxx'],
            '.c': ['d.c'],
            '.m': ['e.m'],
            '.mm': ['f.mm'],
            '.S': ['g.S'],
            '.s': ['h.s', 'i.asm'],
        }
        for suffix, files in expected.items():
            sources = suffix_map[suffix]
            self.assertEqual(
                sources.files,
                [mozpath.join(reader.config.topobjdir, f) for f in files])

    def test_host_sources(self):
        """Test that HOST_SOURCES works properly."""
        reader = self.reader('host-sources')
        objs = self.read_topsrcdir(reader)

        # The last object is a Linkable
        linkable = objs.pop()
        self.assertTrue(linkable.cxx_link)
        self.assertEqual(len(objs), 3)
        for o in objs:
            self.assertIsInstance(o, HostSources)

        suffix_map = {obj.canonical_suffix: obj for obj in objs}
        self.assertEqual(len(suffix_map), 3)

        expected = {
            '.cpp': ['a.cpp', 'b.cc', 'c.cxx'],
            '.c': ['d.c'],
            '.mm': ['e.mm', 'f.mm'],
        }
        for suffix, files in expected.items():
            sources = suffix_map[suffix]
            self.assertEqual(
                sources.files,
                [mozpath.join(reader.config.topsrcdir, f) for f in files])

    def test_unified_sources(self):
        """Test that UNIFIED_SOURCES works properly."""
        reader = self.reader('unified-sources')
        objs = self.read_topsrcdir(reader)

        # The last object is a Linkable, ignore it
        objs = objs[:-1]
        self.assertEqual(len(objs), 3)
        for o in objs:
            self.assertIsInstance(o, UnifiedSources)

        suffix_map = {obj.canonical_suffix: obj for obj in objs}
        self.assertEqual(len(suffix_map), 3)

        expected = {
            '.cpp': ['bar.cxx', 'foo.cpp', 'quux.cc'],
            '.mm': ['objc1.mm', 'objc2.mm'],
            '.c': ['c1.c', 'c2.c'],
        }
        for suffix, files in expected.items():
            sources = suffix_map[suffix]
            self.assertEqual(
                sources.files,
                [mozpath.join(reader.config.topsrcdir, f) for f in files])
            self.assertTrue(sources.have_unified_mapping)

    def test_unified_sources_non_unified(self):
        """Test that UNIFIED_SOURCES with FILES_PER_UNIFIED_FILE=1 works properly."""
        reader = self.reader('unified-sources-non-unified')
        objs = self.read_topsrcdir(reader)

        # The last object is a Linkable, ignore it
        objs = objs[:-1]
        self.assertEqual(len(objs), 3)
        for o in objs:
            self.assertIsInstance(o, UnifiedSources)

        suffix_map = {obj.canonical_suffix: obj for obj in objs}
        self.assertEqual(len(suffix_map), 3)

        expected = {
            '.cpp': ['bar.cxx', 'foo.cpp', 'quux.cc'],
            '.mm': ['objc1.mm', 'objc2.mm'],
            '.c': ['c1.c', 'c2.c'],
        }
        for suffix, files in expected.items():
            sources = suffix_map[suffix]
            self.assertEqual(
                sources.files,
                [mozpath.join(reader.config.topsrcdir, f) for f in files])
            self.assertFalse(sources.have_unified_mapping)

    def test_final_target_pp_files(self):
        """Test that FINAL_TARGET_PP_FILES works properly."""
        reader = self.reader('dist-files')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], FinalTargetPreprocessedFiles)

        # Ideally we'd test hierarchies, but that would just be testing
        # the HierarchicalStringList class, which we test separately.
        for path, files in objs[0].files.walk():
            self.assertEqual(path, '')
            self.assertEqual(len(files), 2)

            expected = {'install.rdf', 'main.js'}
            for f in files:
                self.assertTrue(unicode(f) in expected)

    def test_missing_final_target_pp_files(self):
        """Test that FINAL_TARGET_PP_FILES with missing files throws errors."""
        with self.assertRaisesRegexp(SandboxValidationError, 'File listed in '
            'FINAL_TARGET_PP_FILES does not exist'):
            reader = self.reader('dist-files-missing')
            self.read_topsrcdir(reader)

    def test_final_target_pp_files_non_srcdir(self):
        '''Test that non-srcdir paths in FINAL_TARGET_PP_FILES throws errors.'''
        reader = self.reader('final-target-pp-files-non-srcdir')
        with self.assertRaisesRegexp(SandboxValidationError,
             'Only source directory paths allowed in FINAL_TARGET_PP_FILES:'):
            self.read_topsrcdir(reader)

    def test_rust_library_no_cargo_toml(self):
        '''Test that defining a RustLibrary without a Cargo.toml fails.'''
        reader = self.reader('rust-library-no-cargo-toml')
        with self.assertRaisesRegexp(SandboxValidationError,
             'No Cargo.toml file found'):
            self.read_topsrcdir(reader)

    def test_rust_library_name_mismatch(self):
        '''Test that defining a RustLibrary that doesn't match Cargo.toml fails.'''
        reader = self.reader('rust-library-name-mismatch')
        with self.assertRaisesRegexp(SandboxValidationError,
             'library.*does not match Cargo.toml-defined package'):
            self.read_topsrcdir(reader)

    def test_rust_library_no_lib_section(self):
        '''Test that a RustLibrary Cargo.toml with no [lib] section fails.'''
        reader = self.reader('rust-library-no-lib-section')
        with self.assertRaisesRegexp(SandboxValidationError,
             'Cargo.toml for.* has no \\[lib\\] section'):
            self.read_topsrcdir(reader)

    def test_rust_library_no_profile_section(self):
        '''Test that a RustLibrary Cargo.toml with no [profile] section fails.'''
        reader = self.reader('rust-library-no-profile-section')
        with self.assertRaisesRegexp(SandboxValidationError,
             'Cargo.toml for.* has no \\[profile\\.dev\\] section'):
            self.read_topsrcdir(reader)

    def test_rust_library_invalid_crate_type(self):
        '''Test that a RustLibrary Cargo.toml has a permitted crate-type.'''
        reader = self.reader('rust-library-invalid-crate-type')
        with self.assertRaisesRegexp(SandboxValidationError,
             'crate-type.* is not permitted'):
            self.read_topsrcdir(reader)

    def test_rust_library_non_abort_panic(self):
        '''Test that a RustLibrary Cargo.toml has `panic = "abort" set'''
        reader = self.reader('rust-library-non-abort-panic')
        with self.assertRaisesRegexp(SandboxValidationError,
             'does not specify `panic = "abort"`'):
            self.read_topsrcdir(reader)

    def test_rust_library_dash_folding(self):
        '''Test that on-disk names of RustLibrary objects convert dashes to underscores.'''
        reader = self.reader('rust-library-dash-folding',
                             extra_substs=dict(RUST_TARGET='i686-pc-windows-msvc'))
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        lib = objs[0]
        self.assertIsInstance(lib, RustLibrary)
        self.assertRegexpMatches(lib.lib_name, "random_crate")
        self.assertRegexpMatches(lib.import_name, "random_crate")
        self.assertRegexpMatches(lib.basename, "random-crate")

    def test_multiple_rust_libraries(self):
        '''Test that linking multiple Rust libraries throws an error'''
        reader = self.reader('multiple-rust-libraries',
                             extra_substs=dict(RUST_TARGET='i686-pc-windows-msvc'))
        with self.assertRaisesRegexp(LinkageMultipleRustLibrariesError,
             'Cannot link multiple Rust libraries'):
            self.read_topsrcdir(reader)

    def test_crate_dependency_path_resolution(self):
        '''Test recursive dependencies resolve with the correct paths.'''
        reader = self.reader('crate-dependency-path-resolution',
                             extra_substs=dict(RUST_TARGET='i686-pc-windows-msvc'))
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], RustLibrary)

    def test_android_res_dirs(self):
        """Test that ANDROID_RES_DIRS works properly."""
        reader = self.reader('android-res-dirs')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], AndroidResDirs)

        # Android resource directories are ordered.
        expected = [
            mozpath.join(reader.config.topsrcdir, 'dir1'),
            mozpath.join(reader.config.topobjdir, 'dir2'),
            '/dir3',
        ]
        self.assertEquals([p.full_path for p in objs[0].paths], expected)

    def test_binary_components(self):
        """Test that IS_COMPONENT/NO_COMPONENTS_MANIFEST work properly."""
        reader = self.reader('binary-components')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 3)
        self.assertIsInstance(objs[0], ChromeManifestEntry)
        self.assertEqual(objs[0].path,
                         'dist/bin/components/components.manifest')
        self.assertIsInstance(objs[0].entry, manifest.ManifestBinaryComponent)
        self.assertEqual(objs[0].entry.base, 'dist/bin/components')
        self.assertEqual(objs[0].entry.relpath, objs[1].lib_name)
        self.assertIsInstance(objs[1], SharedLibrary)
        self.assertEqual(objs[1].basename, 'foo')
        self.assertIsInstance(objs[2], SharedLibrary)
        self.assertEqual(objs[2].basename, 'bar')

    def test_install_shared_lib(self):
        """Test that we can install a shared library with TEST_HARNESS_FILES"""
        reader = self.reader('test-install-shared-lib')
        objs = self.read_topsrcdir(reader)
        self.assertIsInstance(objs[0], TestHarnessFiles)
        self.assertIsInstance(objs[1], VariablePassthru)
        self.assertIsInstance(objs[2], SharedLibrary)
        for path, files in objs[0].files.walk():
            for f in files:
                self.assertEqual(str(f), '!libfoo.so')
                self.assertEqual(path, 'foo/bar')

    def test_symbols_file(self):
        """Test that SYMBOLS_FILE works"""
        reader = self.reader('test-symbols-file')
        genfile, shlib = self.read_topsrcdir(reader)
        self.assertIsInstance(genfile, GeneratedFile)
        self.assertIsInstance(shlib, SharedLibrary)
        # This looks weird but MockConfig sets DLL_{PREFIX,SUFFIX} and
        # the reader method in this class sets OS_TARGET=WINNT.
        self.assertEqual(shlib.symbols_file, 'libfoo.so.def')

    def test_symbols_file_objdir(self):
        """Test that a SYMBOLS_FILE in the objdir works"""
        reader = self.reader('test-symbols-file-objdir')
        genfile, shlib = self.read_topsrcdir(reader)
        self.assertIsInstance(genfile, GeneratedFile)
        self.assertEqual(genfile.script,
                         mozpath.join(reader.config.topsrcdir, 'foo.py'))
        self.assertIsInstance(shlib, SharedLibrary)
        self.assertEqual(shlib.symbols_file, 'foo.symbols')

    def test_symbols_file_objdir_missing_generated(self):
        """Test that a SYMBOLS_FILE in the objdir that's missing
        from GENERATED_FILES is an error.
        """
        reader = self.reader('test-symbols-file-objdir-missing-generated')
        with self.assertRaisesRegexp(SandboxValidationError,
             'Objdir file specified in SYMBOLS_FILE not in GENERATED_FILES:'):
            self.read_topsrcdir(reader)

    def test_allowed_xpcom_glue(self):
        """Test that the ALLOWED_XPCOM_GLUE list is still relevant."""
        from mozbuild.frontend.emitter import ALLOWED_XPCOM_GLUE

        allowed = defaultdict(list)
        useless = []
        for name, path in ALLOWED_XPCOM_GLUE:
            allowed[path].append(name)

        for path, names in allowed.iteritems():
            if path.startswith(('mailnews/', 'calendar/', 'extensions/purple/purplexpcom')):
                continue
            try:
                content = open(os.path.join(topsrcdir, path, 'moz.build')).read()
            except:
                content = ''
            for name in names:
                if "'%s'" % name in content or '"%s"' % name in content:
                    continue
                useless.append((name, path))

        self.assertEqual(useless, [])


if __name__ == '__main__':
    main()
