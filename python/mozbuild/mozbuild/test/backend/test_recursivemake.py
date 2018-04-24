# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import cPickle as pickle
import json
import os
import unittest

from mozpack.manifests import (
    InstallManifest,
)
from mozunit import main

from mozbuild.backend.recursivemake import (
    RecursiveMakeBackend,
    RecursiveMakeTraversal,
)
from mozbuild.backend.test_manifest import TestManifestBackend
from mozbuild.frontend.emitter import TreeMetadataEmitter
from mozbuild.frontend.reader import BuildReader

from mozbuild.test.backend.common import BackendTester

import mozpack.path as mozpath


class TestRecursiveMakeTraversal(unittest.TestCase):
    def test_traversal(self):
        traversal = RecursiveMakeTraversal()
        traversal.add('', dirs=['A', 'B', 'C'])
        traversal.add('', dirs=['D'])
        traversal.add('A')
        traversal.add('B', dirs=['E', 'F'])
        traversal.add('C', dirs=['G', 'H'])
        traversal.add('D', dirs=['I', 'K'])
        traversal.add('D', dirs=['J', 'L'])
        traversal.add('E')
        traversal.add('F')
        traversal.add('G')
        traversal.add('H')
        traversal.add('I', dirs=['M', 'N'])
        traversal.add('J', dirs=['O', 'P'])
        traversal.add('K', dirs=['Q', 'R'])
        traversal.add('L', dirs=['S'])
        traversal.add('M')
        traversal.add('N', dirs=['T'])
        traversal.add('O')
        traversal.add('P', dirs=['U'])
        traversal.add('Q')
        traversal.add('R', dirs=['V'])
        traversal.add('S', dirs=['W'])
        traversal.add('T')
        traversal.add('U')
        traversal.add('V')
        traversal.add('W', dirs=['X'])
        traversal.add('X')

        parallels = set(('G', 'H', 'I', 'J', 'O', 'P', 'Q', 'R', 'U'))
        def filter(current, subdirs):
            return (current, [d for d in subdirs.dirs if d in parallels],
                [d for d in subdirs.dirs if d not in parallels])

        start, deps = traversal.compute_dependencies(filter)
        self.assertEqual(start, ('X',))
        self.maxDiff = None
        self.assertEqual(deps, {
            'A': ('',),
            'B': ('A',),
            'C': ('F',),
            'D': ('G', 'H'),
            'E': ('B',),
            'F': ('E',),
            'G': ('C',),
            'H': ('C',),
            'I': ('D',),
            'J': ('D',),
            'K': ('T', 'O', 'U'),
            'L': ('Q', 'V'),
            'M': ('I',),
            'N': ('M',),
            'O': ('J',),
            'P': ('J',),
            'Q': ('K',),
            'R': ('K',),
            'S': ('L',),
            'T': ('N',),
            'U': ('P',),
            'V': ('R',),
            'W': ('S',),
            'X': ('W',),
        })

        self.assertEqual(list(traversal.traverse('', filter)),
                         ['', 'A', 'B', 'E', 'F', 'C', 'G', 'H', 'D', 'I',
                         'M', 'N', 'T', 'J', 'O', 'P', 'U', 'K', 'Q', 'R',
                         'V', 'L', 'S', 'W', 'X'])

        self.assertEqual(list(traversal.traverse('C', filter)),
                         ['C', 'G', 'H'])

    def test_traversal_2(self):
        traversal = RecursiveMakeTraversal()
        traversal.add('', dirs=['A', 'B', 'C'])
        traversal.add('A')
        traversal.add('B', dirs=['D', 'E', 'F'])
        traversal.add('C', dirs=['G', 'H', 'I'])
        traversal.add('D')
        traversal.add('E')
        traversal.add('F')
        traversal.add('G')
        traversal.add('H')
        traversal.add('I')

        start, deps = traversal.compute_dependencies()
        self.assertEqual(start, ('I',))
        self.assertEqual(deps, {
            'A': ('',),
            'B': ('A',),
            'C': ('F',),
            'D': ('B',),
            'E': ('D',),
            'F': ('E',),
            'G': ('C',),
            'H': ('G',),
            'I': ('H',),
        })

    def test_traversal_filter(self):
        traversal = RecursiveMakeTraversal()
        traversal.add('', dirs=['A', 'B', 'C'])
        traversal.add('A')
        traversal.add('B', dirs=['D', 'E', 'F'])
        traversal.add('C', dirs=['G', 'H', 'I'])
        traversal.add('D')
        traversal.add('E')
        traversal.add('F')
        traversal.add('G')
        traversal.add('H')
        traversal.add('I')

        def filter(current, subdirs):
            if current == 'B':
                current = None
            return current, [], subdirs.dirs

        start, deps = traversal.compute_dependencies(filter)
        self.assertEqual(start, ('I',))
        self.assertEqual(deps, {
            'A': ('',),
            'C': ('F',),
            'D': ('A',),
            'E': ('D',),
            'F': ('E',),
            'G': ('C',),
            'H': ('G',),
            'I': ('H',),
        })

    def test_traversal_parallel(self):
        traversal = RecursiveMakeTraversal()
        traversal.add('', dirs=['A', 'B', 'C'])
        traversal.add('A')
        traversal.add('B', dirs=['D', 'E', 'F'])
        traversal.add('C', dirs=['G', 'H', 'I'])
        traversal.add('D')
        traversal.add('E')
        traversal.add('F')
        traversal.add('G')
        traversal.add('H')
        traversal.add('I')
        traversal.add('J')

        def filter(current, subdirs):
            return current, subdirs.dirs, []

        start, deps = traversal.compute_dependencies(filter)
        self.assertEqual(start, ('A', 'D', 'E', 'F', 'G', 'H', 'I', 'J'))
        self.assertEqual(deps, {
            'A': ('',),
            'B': ('',),
            'C': ('',),
            'D': ('B',),
            'E': ('B',),
            'F': ('B',),
            'G': ('C',),
            'H': ('C',),
            'I': ('C',),
            'J': ('',),
        })

class TestRecursiveMakeBackend(BackendTester):
    def test_basic(self):
        """Ensure the RecursiveMakeBackend works without error."""
        env = self._consume('stub0', RecursiveMakeBackend)
        self.assertTrue(os.path.exists(mozpath.join(env.topobjdir,
            'backend.RecursiveMakeBackend')))
        self.assertTrue(os.path.exists(mozpath.join(env.topobjdir,
            'backend.RecursiveMakeBackend.in')))

    def test_output_files(self):
        """Ensure proper files are generated."""
        env = self._consume('stub0', RecursiveMakeBackend)

        expected = ['', 'dir1', 'dir2']

        for d in expected:
            out_makefile = mozpath.join(env.topobjdir, d, 'Makefile')
            out_backend = mozpath.join(env.topobjdir, d, 'backend.mk')

            self.assertTrue(os.path.exists(out_makefile))
            self.assertTrue(os.path.exists(out_backend))

    def test_makefile_conversion(self):
        """Ensure Makefile.in is converted properly."""
        env = self._consume('stub0', RecursiveMakeBackend)

        p = mozpath.join(env.topobjdir, 'Makefile')

        lines = [l.strip() for l in open(p, 'rt').readlines()[1:] if not l.startswith('#')]
        self.assertEqual(lines, [
            'DEPTH := .',
            'topobjdir := %s' % env.topobjdir,
            'topsrcdir := %s' % env.topsrcdir,
            'srcdir := %s' % env.topsrcdir,
            'VPATH := %s' % env.topsrcdir,
            'relativesrcdir := .',
            'include $(DEPTH)/config/autoconf.mk',
            '',
            'FOO := foo',
            '',
            'include $(topsrcdir)/config/recurse.mk',
        ])

    def test_missing_makefile_in(self):
        """Ensure missing Makefile.in results in Makefile creation."""
        env = self._consume('stub0', RecursiveMakeBackend)

        p = mozpath.join(env.topobjdir, 'dir2', 'Makefile')
        self.assertTrue(os.path.exists(p))

        lines = [l.strip() for l in open(p, 'rt').readlines()]
        self.assertEqual(len(lines), 10)

        self.assertTrue(lines[0].startswith('# THIS FILE WAS AUTOMATICALLY'))

    def test_backend_mk(self):
        """Ensure backend.mk file is written out properly."""
        env = self._consume('stub0', RecursiveMakeBackend)

        p = mozpath.join(env.topobjdir, 'backend.mk')

        lines = [l.strip() for l in open(p, 'rt').readlines()[2:]]
        self.assertEqual(lines, [
            'DIRS := dir1 dir2',
        ])

        # Make env.substs writable to add ENABLE_TESTS
        env.substs = dict(env.substs)
        env.substs['ENABLE_TESTS'] = '1'
        self._consume('stub0', RecursiveMakeBackend, env=env)
        p = mozpath.join(env.topobjdir, 'backend.mk')

        lines = [l.strip() for l in open(p, 'rt').readlines()[2:]]
        self.assertEqual(lines, [
            'DIRS := dir1 dir2 dir3',
        ])

    def test_mtime_no_change(self):
        """Ensure mtime is not updated if file content does not change."""

        env = self._consume('stub0', RecursiveMakeBackend)

        makefile_path = mozpath.join(env.topobjdir, 'Makefile')
        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        makefile_mtime = os.path.getmtime(makefile_path)
        backend_mtime = os.path.getmtime(backend_path)

        reader = BuildReader(env)
        emitter = TreeMetadataEmitter(env)
        backend = RecursiveMakeBackend(env)
        backend.consume(emitter.emit(reader.read_topsrcdir()))

        self.assertEqual(os.path.getmtime(makefile_path), makefile_mtime)
        self.assertEqual(os.path.getmtime(backend_path), backend_mtime)

    def test_substitute_config_files(self):
        """Ensure substituted config files are produced."""
        env = self._consume('substitute_config_files', RecursiveMakeBackend)

        p = mozpath.join(env.topobjdir, 'foo')
        self.assertTrue(os.path.exists(p))
        lines = [l.strip() for l in open(p, 'rt').readlines()]
        self.assertEqual(lines, [
            'TEST = foo',
        ])

    def test_install_substitute_config_files(self):
        """Ensure we recurse into the dirs that install substituted config files."""
        env = self._consume('install_substitute_config_files', RecursiveMakeBackend)

        root_deps_path = mozpath.join(env.topobjdir, 'root-deps.mk')
        lines = [l.strip() for l in open(root_deps_path, 'rt').readlines()]

        # Make sure we actually recurse into the sub directory during export to
        # install the subst file.
        self.assertTrue(any(l == 'recurse_export: sub/export' for l in lines))

    def test_variable_passthru(self):
        """Ensure variable passthru is written out correctly."""
        env = self._consume('variable_passthru', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = {
            'RCFILE': [
                'RCFILE := foo.rc',
            ],
            'RESFILE': [
                'RESFILE := bar.res',
            ],
            'RCINCLUDE': [
                'RCINCLUDE := bar.rc',
            ],
            'DEFFILE': [
                'DEFFILE := baz.def',
            ],
            'WIN32_EXE_LDFLAGS': [
                'WIN32_EXE_LDFLAGS += -subsystem:console',
            ],
        }

        for var, val in expected.items():
            # print("test_variable_passthru[%s]" % (var))
            found = [str for str in lines if str.startswith(var)]
            self.assertEqual(found, val)

    def test_sources(self):
        """Ensure SOURCES and HOST_SOURCES are handled properly."""
        env = self._consume('sources', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = {
            'ASFILES': [
                'ASFILES += bar.s',
                'ASFILES += foo.asm',
            ],
            'CMMSRCS': [
                'CMMSRCS += bar.mm',
                'CMMSRCS += foo.mm',
            ],
            'CSRCS': [
                'CSRCS += bar.c',
                'CSRCS += foo.c',
            ],
            'HOST_CPPSRCS': [
                'HOST_CPPSRCS += bar.cpp',
                'HOST_CPPSRCS += foo.cpp',
            ],
            'HOST_CSRCS': [
                'HOST_CSRCS += bar.c',
                'HOST_CSRCS += foo.c',
            ],
            'SSRCS': [
                'SSRCS += baz.S',
                'SSRCS += foo.S',
            ],
        }

        for var, val in expected.items():
            found = [str for str in lines if str.startswith(var)]
            self.assertEqual(found, val)

    def test_exports(self):
        """Ensure EXPORTS is handled properly."""
        env = self._consume('exports', RecursiveMakeBackend)

        # EXPORTS files should appear in the dist_include install manifest.
        m = InstallManifest(path=mozpath.join(env.topobjdir,
            '_build_manifests', 'install', 'dist_include'))
        self.assertEqual(len(m), 7)
        self.assertIn('foo.h', m)
        self.assertIn('mozilla/mozilla1.h', m)
        self.assertIn('mozilla/dom/dom2.h', m)

    def test_generated_files(self):
        """Ensure GENERATED_FILES is handled properly."""
        env = self._consume('generated-files', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'export:: bar.c',
            'GARBAGE += bar.c',
            'EXTRA_MDDEPEND_FILES += bar.c.pp',
            'bar.c: %s/generate-bar.py' % env.topsrcdir,
            '$(REPORT_BUILD)',
            '$(call py_action,file_generate,%s/generate-bar.py baz bar.c $(MDDEPDIR)/bar.c.pp)' % env.topsrcdir,
            '',
            'export:: foo.c',
            'GARBAGE += foo.c',
            'EXTRA_MDDEPEND_FILES += foo.c.pp',
            'foo.c: %s/generate-foo.py $(srcdir)/foo-data' % (env.topsrcdir),
            '$(REPORT_BUILD)',
            '$(call py_action,file_generate,%s/generate-foo.py main foo.c $(MDDEPDIR)/foo.c.pp $(srcdir)/foo-data)' % (env.topsrcdir),
            '',
            'export:: quux.c',
            'GARBAGE += quux.c',
            'EXTRA_MDDEPEND_FILES += quux.c.pp',
        ]

        self.maxDiff = None
        self.assertEqual(lines, expected)

    def test_generated_files_force(self):
        """Ensure GENERATED_FILES with .force is handled properly."""
        env = self._consume('generated-files-force', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'export:: bar.c',
            'GARBAGE += bar.c',
            'EXTRA_MDDEPEND_FILES += bar.c.pp',
            'bar.c: %s/generate-bar.py FORCE' % env.topsrcdir,
            '$(REPORT_BUILD)',
            '$(call py_action,file_generate,%s/generate-bar.py baz bar.c $(MDDEPDIR)/bar.c.pp)' % env.topsrcdir,
            '',
            'export:: foo.c',
            'GARBAGE += foo.c',
            'EXTRA_MDDEPEND_FILES += foo.c.pp',
            'foo.c: %s/generate-foo.py $(srcdir)/foo-data' % (env.topsrcdir),
            '$(REPORT_BUILD)',
            '$(call py_action,file_generate,%s/generate-foo.py main foo.c $(MDDEPDIR)/foo.c.pp $(srcdir)/foo-data)' % (env.topsrcdir),
            '',
            'export:: quux.c',
            'GARBAGE += quux.c',
            'EXTRA_MDDEPEND_FILES += quux.c.pp',
        ]

        self.maxDiff = None
        self.assertEqual(lines, expected)

    def test_localized_generated_files(self):
        """Ensure LOCALIZED_GENERATED_FILES is handled properly."""
        env = self._consume('localized-generated-files', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'libs:: foo.xyz',
            'GARBAGE += foo.xyz',
            'EXTRA_MDDEPEND_FILES += foo.xyz.pp',
            'foo.xyz: %s/generate-foo.py $(call MERGE_FILE,localized-input) $(srcdir)/non-localized-input $(if $(IS_LANGUAGE_REPACK),FORCE)' % env.topsrcdir,
            '$(REPORT_BUILD)',
            '$(call py_action,file_generate,--locale=$(AB_CD) %s/generate-foo.py main foo.xyz $(MDDEPDIR)/foo.xyz.pp $(call MERGE_FILE,localized-input) $(srcdir)/non-localized-input)' % env.topsrcdir,
            '',
            'LOCALIZED_FILES_0_FILES += foo.xyz',
            'LOCALIZED_FILES_0_DEST = $(FINAL_TARGET)/',
            'LOCALIZED_FILES_0_TARGET := libs',
            'INSTALL_TARGETS += LOCALIZED_FILES_0',
        ]

        self.maxDiff = None
        self.assertEqual(lines, expected)

    def test_localized_generated_files_force(self):
        """Ensure LOCALIZED_GENERATED_FILES with .force is handled properly."""
        env = self._consume('localized-generated-files-force', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'libs:: foo.xyz',
            'GARBAGE += foo.xyz',
            'EXTRA_MDDEPEND_FILES += foo.xyz.pp',
            'foo.xyz: %s/generate-foo.py $(call MERGE_FILE,localized-input) $(srcdir)/non-localized-input $(if $(IS_LANGUAGE_REPACK),FORCE)' % env.topsrcdir,
            '$(REPORT_BUILD)',
            '$(call py_action,file_generate,--locale=$(AB_CD) %s/generate-foo.py main foo.xyz $(MDDEPDIR)/foo.xyz.pp $(call MERGE_FILE,localized-input) $(srcdir)/non-localized-input)' % env.topsrcdir,
            '',
            'libs:: abc.xyz',
            'GARBAGE += abc.xyz',
            'EXTRA_MDDEPEND_FILES += abc.xyz.pp',
            'abc.xyz: %s/generate-foo.py $(call MERGE_FILE,localized-input) $(srcdir)/non-localized-input FORCE' % env.topsrcdir,
            '$(REPORT_BUILD)',
            '$(call py_action,file_generate,--locale=$(AB_CD) %s/generate-foo.py main abc.xyz $(MDDEPDIR)/abc.xyz.pp $(call MERGE_FILE,localized-input) $(srcdir)/non-localized-input)' % env.topsrcdir,
            '',
        ]

        self.maxDiff = None
        self.assertEqual(lines, expected)

    def test_localized_generated_files_AB_CD(self):
        """Ensure LOCALIZED_GENERATED_FILES is handled properly
        when {AB_CD} and {AB_rCD} are used."""
        env = self._consume('localized-generated-files-AB_CD', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'libs:: foo$(AB_CD).xyz',
            'GARBAGE += foo$(AB_CD).xyz',
            'EXTRA_MDDEPEND_FILES += foo$(AB_CD).xyz.pp',
            'foo$(AB_CD).xyz: %s/generate-foo.py $(call MERGE_FILE,localized-input) $(srcdir)/non-localized-input $(if $(IS_LANGUAGE_REPACK),FORCE)' % env.topsrcdir,
            '$(REPORT_BUILD)',
            '$(call py_action,file_generate,--locale=$(AB_CD) %s/generate-foo.py main foo$(AB_CD).xyz $(MDDEPDIR)/foo$(AB_CD).xyz.pp $(call MERGE_FILE,localized-input) $(srcdir)/non-localized-input)' % env.topsrcdir,
            '',
            'include $(topsrcdir)/config/AB_rCD.mk',
            'GARBAGE += bar$(AB_rCD).xyz',
            'EXTRA_MDDEPEND_FILES += bar$(AB_rCD).xyz.pp',
            'bar$(AB_rCD).xyz: %s/generate-foo.py $(call MERGE_RELATIVE_FILE,localized-input,inner/locales) $(srcdir)/non-localized-input $(if $(IS_LANGUAGE_REPACK),FORCE)' % env.topsrcdir,
            '$(REPORT_BUILD)',
            '$(call py_action,file_generate,--locale=$(AB_CD) %s/generate-foo.py main bar$(AB_rCD).xyz $(MDDEPDIR)/bar$(AB_rCD).xyz.pp $(call MERGE_RELATIVE_FILE,localized-input,inner/locales) $(srcdir)/non-localized-input)' % env.topsrcdir,
            '',
            'GARBAGE += zot$(AB_rCD).xyz',
            'EXTRA_MDDEPEND_FILES += zot$(AB_rCD).xyz.pp',
            'zot$(AB_rCD).xyz: %s/generate-foo.py $(call MERGE_RELATIVE_FILE,localized-input,locales) $(srcdir)/non-localized-input $(if $(IS_LANGUAGE_REPACK),FORCE)' % env.topsrcdir,
            '$(REPORT_BUILD)',
            '$(call py_action,file_generate,--locale=$(AB_CD) %s/generate-foo.py main zot$(AB_rCD).xyz $(MDDEPDIR)/zot$(AB_rCD).xyz.pp $(call MERGE_RELATIVE_FILE,localized-input,locales) $(srcdir)/non-localized-input)' % env.topsrcdir,
            '',
        ]

        self.maxDiff = None
        self.assertEqual(lines, expected)

    def test_exports_generated(self):
        """Ensure EXPORTS that are listed in GENERATED_FILES
        are handled properly."""
        env = self._consume('exports-generated', RecursiveMakeBackend)

        # EXPORTS files should appear in the dist_include install manifest.
        m = InstallManifest(path=mozpath.join(env.topobjdir,
            '_build_manifests', 'install', 'dist_include'))
        self.assertEqual(len(m), 8)
        self.assertIn('foo.h', m)
        self.assertIn('mozilla/mozilla1.h', m)
        self.assertIn('mozilla/dom/dom1.h', m)
        self.assertIn('gfx/gfx.h', m)
        self.assertIn('bar.h', m)
        self.assertIn('mozilla/mozilla2.h', m)
        self.assertIn('mozilla/dom/dom2.h', m)
        self.assertIn('mozilla/dom/dom3.h', m)
        # EXPORTS files that are also GENERATED_FILES should be handled as
        # INSTALL_TARGETS.
        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]
        expected = [
            'export:: bar.h',
            'GARBAGE += bar.h',
            'EXTRA_MDDEPEND_FILES += bar.h.pp',
            'export:: mozilla2.h',
            'GARBAGE += mozilla2.h',
            'EXTRA_MDDEPEND_FILES += mozilla2.h.pp',
            'export:: dom2.h',
            'GARBAGE += dom2.h',
            'EXTRA_MDDEPEND_FILES += dom2.h.pp',
            'export:: dom3.h',
            'GARBAGE += dom3.h',
            'EXTRA_MDDEPEND_FILES += dom3.h.pp',
            'dist_include_FILES += bar.h',
            'dist_include_DEST := $(DEPTH)/dist/include/',
            'dist_include_TARGET := export',
            'INSTALL_TARGETS += dist_include',
            'dist_include_mozilla_FILES += mozilla2.h',
            'dist_include_mozilla_DEST := $(DEPTH)/dist/include/mozilla',
            'dist_include_mozilla_TARGET := export',
            'INSTALL_TARGETS += dist_include_mozilla',
            'dist_include_mozilla_dom_FILES += dom2.h',
            'dist_include_mozilla_dom_FILES += dom3.h',
            'dist_include_mozilla_dom_DEST := $(DEPTH)/dist/include/mozilla/dom',
            'dist_include_mozilla_dom_TARGET := export',
            'INSTALL_TARGETS += dist_include_mozilla_dom',
        ]
        self.maxDiff = None
        self.assertEqual(lines, expected)

    def test_resources(self):
        """Ensure RESOURCE_FILES is handled properly."""
        env = self._consume('resources', RecursiveMakeBackend)

        # RESOURCE_FILES should appear in the dist_bin install manifest.
        m = InstallManifest(path=os.path.join(env.topobjdir,
            '_build_manifests', 'install', 'dist_bin'))
        self.assertEqual(len(m), 10)
        self.assertIn('res/foo.res', m)
        self.assertIn('res/fonts/font1.ttf', m)
        self.assertIn('res/fonts/desktop/desktop2.ttf', m)

        self.assertIn('res/bar.res.in', m)
        self.assertIn('res/tests/test.manifest', m)
        self.assertIn('res/tests/extra.manifest', m)

    def test_test_manifests_files_written(self):
        """Ensure test manifests get turned into files."""
        env = self._consume('test-manifests-written', RecursiveMakeBackend)

        tests_dir = mozpath.join(env.topobjdir, '_tests')
        m_master = mozpath.join(tests_dir, 'testing', 'mochitest', 'tests', 'mochitest.ini')
        x_master = mozpath.join(tests_dir, 'xpcshell', 'xpcshell.ini')
        self.assertTrue(os.path.exists(m_master))
        self.assertTrue(os.path.exists(x_master))

        lines = [l.strip() for l in open(x_master, 'rt').readlines()]
        self.assertEqual(lines, [
            '# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT MODIFY BY HAND.',
            '',
            '[include:dir1/xpcshell.ini]',
            '[include:xpcshell.ini]',
        ])

    def test_test_manifest_pattern_matches_recorded(self):
        """Pattern matches in test manifests' support-files should be recorded."""
        env = self._consume('test-manifests-written', RecursiveMakeBackend)
        m = InstallManifest(path=mozpath.join(env.topobjdir,
            '_build_manifests', 'install', '_test_files'))

        # This is not the most robust test in the world, but it gets the job
        # done.
        entries = [e for e in m._dests.keys() if '**' in e]
        self.assertEqual(len(entries), 1)
        self.assertIn('support/**', entries[0])

    def test_test_manifest_deffered_installs_written(self):
        """Shared support files are written to their own data file by the backend."""
        env = self._consume('test-manifest-shared-support', RecursiveMakeBackend)

        # First, read the generated for ini manifest contents.
        test_files_manifest = mozpath.join(env.topobjdir,
                                           '_build_manifests',
                                           'install',
                                           '_test_files')
        m = InstallManifest(path=test_files_manifest)

        # Then, synthesize one from the test-installs.pkl file. This should
        # allow us to re-create a subset of the above.
        env = self._consume('test-manifest-shared-support', TestManifestBackend)
        test_installs_path = mozpath.join(env.topobjdir, 'test-installs.pkl')

        with open(test_installs_path, 'r') as fh:
            test_installs = pickle.load(fh)

        self.assertEqual(set(test_installs.keys()),
                         set(['child/test_sub.js',
                              'child/data/**',
                              'child/another-file.sjs']))
        for key in test_installs.keys():
            self.assertIn(key, test_installs)

        synthesized_manifest = InstallManifest()
        for item, installs in test_installs.items():
            for install_info in installs:
                if len(install_info) == 3:
                    synthesized_manifest.add_pattern_link(*install_info)
                if len(install_info) == 2:
                    synthesized_manifest.add_link(*install_info)

        self.assertEqual(len(synthesized_manifest), 3)
        for item, info in synthesized_manifest._dests.items():
            self.assertIn(item, m)
            self.assertEqual(info, m._dests[item])

    def test_xpidl_generation(self):
        """Ensure xpidl files and directories are written out."""
        env = self._consume('xpidl', RecursiveMakeBackend)

        # Install manifests should contain entries.
        install_dir = mozpath.join(env.topobjdir, '_build_manifests',
            'install')
        self.assertTrue(os.path.isfile(mozpath.join(install_dir, 'dist_idl')))
        self.assertTrue(os.path.isfile(mozpath.join(install_dir, 'xpidl')))

        m = InstallManifest(path=mozpath.join(install_dir, 'dist_idl'))
        self.assertEqual(len(m), 2)
        self.assertIn('bar.idl', m)
        self.assertIn('foo.idl', m)

        m = InstallManifest(path=mozpath.join(install_dir, 'xpidl'))
        self.assertIn('.deps/my_module.pp', m)

        m = InstallManifest(path=mozpath.join(install_dir, 'xpidl'))
        self.assertIn('my_module.xpt', m)

        m = InstallManifest(path=mozpath.join(install_dir, 'dist_include'))
        self.assertIn('foo.h', m)

        p = mozpath.join(env.topobjdir, 'config/makefiles/xpidl')
        self.assertTrue(os.path.isdir(p))

        self.assertTrue(os.path.isfile(mozpath.join(p, 'Makefile')))

    def test_test_support_files_tracked(self):
        env = self._consume('test-support-binaries-tracked', RecursiveMakeBackend)
        m = InstallManifest(path=mozpath.join(env.topobjdir,
            '_build_manifests', 'install', '_tests'))
        self.assertEqual(len(m), 4)
        self.assertIn('xpcshell/tests/mozbuildtest/test-library.dll', m)
        self.assertIn('xpcshell/tests/mozbuildtest/test-one.exe', m)
        self.assertIn('xpcshell/tests/mozbuildtest/test-two.exe', m)
        self.assertIn('xpcshell/tests/mozbuildtest/host-test-library.dll', m)

    def test_old_install_manifest_deleted(self):
        # Simulate an install manifest from a previous backend version. Ensure
        # it is deleted.
        env = self._get_environment('stub0')
        purge_dir = mozpath.join(env.topobjdir, '_build_manifests', 'install')
        manifest_path = mozpath.join(purge_dir, 'old_manifest')
        os.makedirs(purge_dir)
        m = InstallManifest()
        m.write(path=manifest_path)
        with open(mozpath.join(
                env.topobjdir, 'backend.RecursiveMakeBackend'), 'w') as f:
            f.write('%s\n' % manifest_path)

        self.assertTrue(os.path.exists(manifest_path))
        self._consume('stub0', RecursiveMakeBackend, env)
        self.assertFalse(os.path.exists(manifest_path))

    def test_install_manifests_written(self):
        env, objs = self._emit('stub0')
        backend = RecursiveMakeBackend(env)

        m = InstallManifest()
        backend._install_manifests['testing'] = m
        m.add_link(__file__, 'self')
        backend.consume(objs)

        man_dir = mozpath.join(env.topobjdir, '_build_manifests', 'install')
        self.assertTrue(os.path.isdir(man_dir))

        expected = ['testing']
        for e in expected:
            full = mozpath.join(man_dir, e)
            self.assertTrue(os.path.exists(full))

            m2 = InstallManifest(path=full)
            self.assertEqual(m, m2)

    def test_ipdl_sources(self):
        """Test that PREPROCESSED_IPDL_SOURCES and IPDL_SOURCES are written to ipdlsrcs.mk correctly."""
        env = self._get_environment('ipdl_sources')

        # Make substs writable so we can set the value of IPDL_ROOT to reflect
        # the correct objdir.
        env.substs = dict(env.substs)
        env.substs['IPDL_ROOT'] = env.topobjdir

        self._consume('ipdl_sources', RecursiveMakeBackend, env)

        manifest_path = mozpath.join(env.topobjdir, 'ipdlsrcs.mk')
        lines = [l.strip() for l in open(manifest_path, 'rt').readlines()]

        # Handle Windows paths correctly
        topsrcdir = env.topsrcdir.replace(os.sep, '/')

        expected = [
            "ALL_IPDLSRCS := bar1.ipdl foo1.ipdl %s/bar/bar.ipdl %s/bar/bar2.ipdlh %s/foo/foo.ipdl %s/foo/foo2.ipdlh" % tuple([topsrcdir] * 4),
            "CPPSRCS := UnifiedProtocols0.cpp",
            "IPDLDIRS := %s %s/bar %s/foo" % (env.topobjdir, topsrcdir, topsrcdir),
        ]

        found = [str for str in lines if str.startswith(('ALL_IPDLSRCS',
                                                         'CPPSRCS',
                                                         'IPDLDIRS'))]
        self.assertEqual(found, expected)

    def test_defines(self):
        """Test that DEFINES are written to backend.mk correctly."""
        env = self._consume('defines', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        var = 'DEFINES'
        defines = [val for val in lines if val.startswith(var)]

        expected = ['DEFINES += -DFOO \'-DBAZ="ab\'\\\'\'cd"\' -UQUX -DBAR=7 -DVALUE=xyz']
        self.assertEqual(defines, expected)

    def test_local_includes(self):
        """Test that LOCAL_INCLUDES are written to backend.mk correctly."""
        env = self._consume('local_includes', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'LOCAL_INCLUDES += -I$(srcdir)/bar/baz',
            'LOCAL_INCLUDES += -I$(srcdir)/foo',
        ]

        found = [str for str in lines if str.startswith('LOCAL_INCLUDES')]
        self.assertEqual(found, expected)

    def test_generated_includes(self):
        """Test that GENERATED_INCLUDES are written to backend.mk correctly."""
        env = self._consume('generated_includes', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        topobjdir = env.topobjdir.replace('\\', '/')

        expected = [
            'LOCAL_INCLUDES += -I$(CURDIR)/bar/baz',
            'LOCAL_INCLUDES += -I$(CURDIR)/foo',
        ]

        found = [str for str in lines if str.startswith('LOCAL_INCLUDES')]
        self.assertEqual(found, expected)

    def test_rust_library(self):
        """Test that a Rust library is written to backend.mk correctly."""
        env = self._consume('rust-library', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'RUST_LIBRARY_FILE := ./x86_64-unknown-linux-gnu/release/libtest_library.a',
            'CARGO_FILE := $(srcdir)/Cargo.toml',
            'CARGO_TARGET_DIR := %s' % env.topobjdir,
        ]

        self.assertEqual(lines, expected)

    def test_host_rust_library(self):
        """Test that a Rust library is written to backend.mk correctly."""
        env = self._consume('host-rust-library', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'HOST_RUST_LIBRARY_FILE := ./x86_64-unknown-linux-gnu/release/libhostrusttool.a',
            'CARGO_FILE := $(srcdir)/Cargo.toml',
            'CARGO_TARGET_DIR := %s' % env.topobjdir,
        ]

        self.assertEqual(lines, expected)

    def test_host_rust_library_with_features(self):
        """Test that a host Rust library with features is written to backend.mk correctly."""
        env = self._consume('host-rust-library-features', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'HOST_RUST_LIBRARY_FILE := ./x86_64-unknown-linux-gnu/release/libhostrusttool.a',
            'CARGO_FILE := $(srcdir)/Cargo.toml',
            'CARGO_TARGET_DIR := %s' % env.topobjdir,
            'HOST_RUST_LIBRARY_FEATURES := musthave cantlivewithout',
        ]

        self.assertEqual(lines, expected)

    def test_rust_library_with_features(self):
        """Test that a Rust library with features is written to backend.mk correctly."""
        env = self._consume('rust-library-features', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'RUST_LIBRARY_FILE := ./x86_64-unknown-linux-gnu/release/libfeature_library.a',
            'CARGO_FILE := $(srcdir)/Cargo.toml',
            'CARGO_TARGET_DIR := %s' % env.topobjdir,
            'RUST_LIBRARY_FEATURES := musthave cantlivewithout',
        ]

        self.assertEqual(lines, expected)

    def test_rust_programs(self):
        """Test that {HOST_,}RUST_PROGRAMS are written to backend.mk correctly."""
        env = self._consume('rust-programs', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'code/backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'CARGO_FILE := %s/code/Cargo.toml' % env.topsrcdir,
            'CARGO_TARGET_DIR := .',
            'RUST_PROGRAMS += i686-pc-windows-msvc/release/target.exe',
            'RUST_CARGO_PROGRAMS += target',
            'HOST_RUST_PROGRAMS += i686-pc-windows-msvc/release/host.exe',
            'HOST_RUST_CARGO_PROGRAMS += host',
        ]

        self.assertEqual(lines, expected)

        root_deps_path = mozpath.join(env.topobjdir, 'root-deps.mk')
        lines = [l.strip() for l in open(root_deps_path, 'rt').readlines()]

        self.assertTrue(any(l == 'recurse_compile: code/target code/host' for l in lines))

    def test_final_target(self):
        """Test that FINAL_TARGET is written to backend.mk correctly."""
        env = self._consume('final_target', RecursiveMakeBackend)

        final_target_rule = "FINAL_TARGET = $(if $(XPI_NAME),$(DIST)/xpi-stage/$(XPI_NAME),$(DIST)/bin)$(DIST_SUBDIR:%=/%)"
        expected = dict()
        expected[env.topobjdir] = []
        expected[mozpath.join(env.topobjdir, 'both')] = [
            'XPI_NAME = mycrazyxpi',
            'DIST_SUBDIR = asubdir',
            final_target_rule
        ]
        expected[mozpath.join(env.topobjdir, 'dist-subdir')] = [
            'DIST_SUBDIR = asubdir',
            final_target_rule
        ]
        expected[mozpath.join(env.topobjdir, 'xpi-name')] = [
            'XPI_NAME = mycrazyxpi',
            final_target_rule
        ]
        expected[mozpath.join(env.topobjdir, 'final-target')] = [
            'FINAL_TARGET = $(DEPTH)/random-final-target'
        ]
        for key, expected_rules in expected.iteritems():
            backend_path = mozpath.join(key, 'backend.mk')
            lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]
            found = [str for str in lines if
                str.startswith('FINAL_TARGET') or str.startswith('XPI_NAME') or
                str.startswith('DIST_SUBDIR')]
            self.assertEqual(found, expected_rules)

    def test_final_target_pp_files(self):
        """Test that FINAL_TARGET_PP_FILES is written to backend.mk correctly."""
        env = self._consume('dist-files', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'DIST_FILES_0 += $(srcdir)/install.rdf',
            'DIST_FILES_0 += $(srcdir)/main.js',
            'DIST_FILES_0_PATH := $(DEPTH)/dist/bin/',
            'DIST_FILES_0_TARGET := misc',
            'PP_TARGETS += DIST_FILES_0',
        ]

        found = [str for str in lines if 'DIST_FILES' in str]
        self.assertEqual(found, expected)

    def test_localized_files(self):
        """Test that LOCALIZED_FILES is written to backend.mk correctly."""
        env = self._consume('localized-files', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'LOCALIZED_FILES_0_FILES += $(wildcard $(LOCALE_SRCDIR)/abc/*.abc)',
            'LOCALIZED_FILES_0_FILES += $(call MERGE_FILE,bar.ini)',
            'LOCALIZED_FILES_0_FILES += $(call MERGE_FILE,foo.js)',
            'LOCALIZED_FILES_0_DEST = $(FINAL_TARGET)/',
            'LOCALIZED_FILES_0_TARGET := libs',
            'INSTALL_TARGETS += LOCALIZED_FILES_0',
        ]

        found = [str for str in lines if 'LOCALIZED_FILES' in str]
        self.assertEqual(found, expected)

    def test_localized_pp_files(self):
        """Test that LOCALIZED_PP_FILES is written to backend.mk correctly."""
        env = self._consume('localized-pp-files', RecursiveMakeBackend)

        backend_path = mozpath.join(env.topobjdir, 'backend.mk')
        lines = [l.strip() for l in open(backend_path, 'rt').readlines()[2:]]

        expected = [
            'LOCALIZED_PP_FILES_0 += $(call MERGE_FILE,bar.ini)',
            'LOCALIZED_PP_FILES_0 += $(call MERGE_FILE,foo.js)',
            'LOCALIZED_PP_FILES_0_PATH = $(FINAL_TARGET)/',
            'LOCALIZED_PP_FILES_0_TARGET := libs',
            'LOCALIZED_PP_FILES_0_FLAGS := --silence-missing-directive-warnings',
            'PP_TARGETS += LOCALIZED_PP_FILES_0',
        ]

        found = [str for str in lines if 'LOCALIZED_PP_FILES' in str]
        self.assertEqual(found, expected)

    def test_config(self):
        """Test that CONFIGURE_SUBST_FILES are properly handled."""
        env = self._consume('test_config', RecursiveMakeBackend)

        self.assertEqual(
            open(os.path.join(env.topobjdir, 'file'), 'r').readlines(), [
                '#ifdef foo\n',
                'bar baz\n',
                '@bar@\n',
            ])

    def test_prog_lib_c_only(self):
        """Test that C-only binary artifacts are marked as such."""
        env = self._consume('prog-lib-c-only', RecursiveMakeBackend)

        # PROGRAM C-onlyness.
        with open(os.path.join(env.topobjdir, 'c-program', 'backend.mk'), 'rb') as fh:
            lines = fh.readlines()
            lines = [line.rstrip() for line in lines]

            self.assertIn('PROG_IS_C_ONLY_c_test_program := 1', lines)

        with open(os.path.join(env.topobjdir, 'cxx-program', 'backend.mk'), 'rb') as fh:
            lines = fh.readlines()
            lines = [line.rstrip() for line in lines]

            # Test for only the absence of the variable, not the precise
            # form of the variable assignment.
            for line in lines:
                self.assertNotIn('PROG_IS_C_ONLY_cxx_test_program', line)

        # SIMPLE_PROGRAMS C-onlyness.
        with open(os.path.join(env.topobjdir, 'c-simple-programs', 'backend.mk'), 'rb') as fh:
            lines = fh.readlines()
            lines = [line.rstrip() for line in lines]

            self.assertIn('PROG_IS_C_ONLY_c_simple_program := 1', lines)

        with open(os.path.join(env.topobjdir, 'cxx-simple-programs', 'backend.mk'), 'rb') as fh:
            lines = fh.readlines()
            lines = [line.rstrip() for line in lines]

            for line in lines:
                self.assertNotIn('PROG_IS_C_ONLY_cxx_simple_program', line)

        # Libraries C-onlyness.
        with open(os.path.join(env.topobjdir, 'c-library', 'backend.mk'), 'rb') as fh:
            lines = fh.readlines()
            lines = [line.rstrip() for line in lines]

            self.assertIn('LIB_IS_C_ONLY := 1', lines)

        with open(os.path.join(env.topobjdir, 'cxx-library', 'backend.mk'), 'rb') as fh:
            lines = fh.readlines()
            lines = [line.rstrip() for line in lines]

            for line in lines:
                self.assertNotIn('LIB_IS_C_ONLY', line)

    def test_linkage(self):
        env = self._consume('linkage', RecursiveMakeBackend)
        expected_linkage = {
            'prog': {
                'SHARED_LIBS': ['qux/qux.so',
                                '../shared/baz.so'],
                'STATIC_LIBS': ['../real/foo.a'],
                'OS_LIBS': ['-lfoo', '-lbaz', '-lbar'],
            },
            'shared': {
                'OS_LIBS': ['-lfoo'],
                'SHARED_LIBS': ['../prog/qux/qux.so'],
                'STATIC_LIBS': [],
            },
            'static': {
                'STATIC_LIBS': ['../real/foo.a'],
                'OS_LIBS': ['-lbar'],
                'SHARED_LIBS': ['../prog/qux/qux.so'],
            },
            'real': {
                'STATIC_LIBS': [],
                'SHARED_LIBS': ['../prog/qux/qux.so'],
                'OS_LIBS': ['-lbaz'],
            }
        }
        actual_linkage = {}
        for name in expected_linkage.keys():
            with open(os.path.join(env.topobjdir, name, 'backend.mk'), 'rb') as fh:
                actual_linkage[name] = [line.rstrip() for line in fh.readlines()]
        for name in expected_linkage:
            for var in expected_linkage[name]:
                for val in expected_linkage[name][var]:
                    val = os.path.normpath(val)
                    line = '%s += %s' % (var, val)
                    self.assertIn(line,
                                  actual_linkage[name])
                    actual_linkage[name].remove(line)
                for line in actual_linkage[name]:
                    self.assertNotIn('%s +=' % var, line)

    def test_list_files(self):
        env = self._consume('linkage', RecursiveMakeBackend)
        expected_list_files = {
            'prog/MyProgram_exe.list': [
                '../static/bar/bar1.o',
                '../static/bar/bar2.o',
                '../static/bar/bar_helper/bar_helper1.o',
            ],
            'shared/baz_so.list': [
                'baz/baz1.o',
            ],
        }
        actual_list_files = {}
        for name in expected_list_files.keys():
            with open(os.path.join(env.topobjdir, name), 'rb') as fh:
                actual_list_files[name] = [line.rstrip()
                                           for line in fh.readlines()]
        for name in expected_list_files:
            self.assertEqual(actual_list_files[name],
                             [os.path.normpath(f) for f in expected_list_files[name]])

        # We don't produce a list file for a shared library composed only of
        # object files in its directory, but instead list them in a variable.
        with open(os.path.join(env.topobjdir, 'prog', 'qux', 'backend.mk'), 'rb') as fh:
            lines = [line.rstrip() for line in fh.readlines()]

        self.assertIn('qux.so_OBJS := qux1.o', lines)

    def test_jar_manifests(self):
        env = self._consume('jar-manifests', RecursiveMakeBackend)

        with open(os.path.join(env.topobjdir, 'backend.mk'), 'rb') as fh:
            lines = fh.readlines()

        lines = [line.rstrip() for line in lines]

        self.assertIn('JAR_MANIFEST := %s/jar.mn' % env.topsrcdir, lines)

    def test_test_manifests_duplicate_support_files(self):
        """Ensure duplicate support-files in test manifests work."""
        env = self._consume('test-manifests-duplicate-support-files',
            RecursiveMakeBackend)

        p = os.path.join(env.topobjdir, '_build_manifests', 'install', '_test_files')
        m = InstallManifest(p)
        self.assertIn('testing/mochitest/tests/support-file.txt', m)

    def test_install_manifests_package_tests(self):
        """Ensure test suites honor package_tests=False."""
        env = self._consume('test-manifests-package-tests', RecursiveMakeBackend)

        man_dir = mozpath.join(env.topobjdir, '_build_manifests', 'install')
        self.assertTrue(os.path.isdir(man_dir))

        full = mozpath.join(man_dir, '_test_files')
        self.assertTrue(os.path.exists(full))

        m = InstallManifest(path=full)

        # Only mochitest.js should be in the install manifest.
        self.assertTrue('testing/mochitest/tests/mochitest.js' in m)

        # The path is odd here because we do not normalize at test manifest
        # processing time.  This is a fragile test because there's currently no
        # way to iterate the manifest.
        self.assertFalse('instrumentation/./not_packaged.java' in m)

    def test_program_paths(self):
        """PROGRAMs with various moz.build settings that change the destination should produce
        the expected paths in backend.mk."""
        env = self._consume('program-paths', RecursiveMakeBackend)

        expected = [
            ('dist-bin', '$(DEPTH)/dist/bin/dist-bin.prog'),
            ('dist-subdir', '$(DEPTH)/dist/bin/foo/dist-subdir.prog'),
            ('final-target', '$(DEPTH)/final/target/final-target.prog'),
            ('not-installed', 'not-installed.prog'),
        ]
        prefix = 'PROGRAM = '
        for (subdir, expected_program) in expected:
            with open(os.path.join(env.topobjdir, subdir, 'backend.mk'), 'rb') as fh:
                lines = fh.readlines()
                program = [line.rstrip().split(prefix, 1)[1] for line in lines
                           if line.startswith(prefix)][0]
                self.assertEqual(program, expected_program)


if __name__ == '__main__':
    main()
