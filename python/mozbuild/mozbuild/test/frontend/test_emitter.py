# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import unittest

from mozunit import main

from mozbuild.frontend.data import (
    ConfigFileSubstitution,
    DirectoryTraversal,
    ReaderSummary,
    VariablePassthru,
    Exports,
    Program,
    XpcshellManifests,
    IPDLFile,
)
from mozbuild.frontend.emitter import TreeMetadataEmitter
from mozbuild.frontend.reader import BuildReader

from mozbuild.test.common import MockConfig


data_path = os.path.abspath(os.path.dirname(__file__))
data_path = os.path.join(data_path, 'data')


class TestEmitterBasic(unittest.TestCase):
    def reader(self, name):
        config = MockConfig(os.path.join(data_path, name), extra_substs=dict(
            ENABLE_TESTS='1',
            BIN_SUFFIX='.prog',
        ))

        return BuildReader(config)

    def read_topsrcdir(self, reader):
        emitter = TreeMetadataEmitter(reader.config)

        objs = list(emitter.emit(reader.read_topsrcdir()))
        self.assertGreater(len(objs), 0)
        self.assertIsInstance(objs[-1], ReaderSummary)

        return objs[:-1]

    def test_dirs_traversal_simple(self):
        reader = self.reader('traversal-simple')
        objs = self.read_topsrcdir(reader)
        self.assertEqual(len(objs), 4)

        for o in objs:
            self.assertIsInstance(o, DirectoryTraversal)
            self.assertEqual(o.parallel_dirs, [])
            self.assertEqual(o.tool_dirs, [])
            self.assertEqual(o.test_dirs, [])
            self.assertEqual(o.test_tool_dirs, [])
            self.assertEqual(len(o.tier_dirs), 0)
            self.assertEqual(len(o.tier_static_dirs), 0)
            self.assertTrue(os.path.isabs(o.sandbox_main_path))
            self.assertEqual(len(o.sandbox_all_paths), 1)

        reldirs = [o.relativedir for o in objs]
        self.assertEqual(reldirs, ['', 'foo', 'foo/biz', 'bar'])

        dirs = [o.dirs for o in objs]
        self.assertEqual(dirs, [['foo', 'bar'], ['biz'], [], []])

    def test_traversal_all_vars(self):
        reader = self.reader('traversal-all-vars')
        objs = self.read_topsrcdir(reader)
        self.assertEqual(len(objs), 6)

        for o in objs:
            self.assertIsInstance(o, DirectoryTraversal)

        reldirs = set([o.relativedir for o in objs])
        self.assertEqual(reldirs, set(['', 'parallel', 'regular', 'test',
            'test_tool', 'tool']))

        for o in objs:
            reldir = o.relativedir

            if reldir == '':
                self.assertEqual(o.dirs, ['regular'])
                self.assertEqual(o.parallel_dirs, ['parallel'])
                self.assertEqual(o.test_dirs, ['test'])
                self.assertEqual(o.test_tool_dirs, ['test_tool'])
                self.assertEqual(o.tool_dirs, ['tool'])
                self.assertEqual(o.external_make_dirs, ['external_make'])
                self.assertEqual(o.parallel_external_make_dirs,
                    ['parallel_external_make'])

    def test_tier_simple(self):
        reader = self.reader('traversal-tier-simple')
        objs = self.read_topsrcdir(reader)
        self.assertEqual(len(objs), 4)

        reldirs = [o.relativedir for o in objs]
        self.assertEqual(reldirs, ['', 'foo', 'foo/biz', 'bar'])

    def test_config_file_substitution(self):
        reader = self.reader('config-file-substitution')
        objs = self.read_topsrcdir(reader)
        self.assertEqual(len(objs), 3)

        self.assertIsInstance(objs[0], DirectoryTraversal)
        self.assertIsInstance(objs[1], ConfigFileSubstitution)
        self.assertIsInstance(objs[2], ConfigFileSubstitution)

        topobjdir = os.path.abspath(reader.config.topobjdir)
        self.assertEqual(objs[1].relpath, 'foo')
        self.assertEqual(os.path.normpath(objs[1].output_path),
            os.path.normpath(os.path.join(topobjdir, 'foo')))
        self.assertEqual(os.path.normpath(objs[2].output_path),
            os.path.normpath(os.path.join(topobjdir, 'bar')))

    def test_variable_passthru(self):
        reader = self.reader('variable-passthru')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 2)
        self.assertIsInstance(objs[0], DirectoryTraversal)
        self.assertIsInstance(objs[1], VariablePassthru)

        wanted = dict(
            ASFILES=['fans.asm', 'tans.s'],
            CMMSRCS=['fans.mm', 'tans.mm'],
            CSRCS=['fans.c', 'tans.c'],
            CPP_UNIT_TESTS=['foo.cpp'],
            DEFINES=['-Dfans', '-Dtans'],
            EXTRA_COMPONENTS=['fans.js', 'tans.js'],
            EXTRA_PP_COMPONENTS=['fans.pp.js', 'tans.pp.js'],
            EXTRA_JS_MODULES=['bar.jsm', 'foo.jsm'],
            EXTRA_PP_JS_MODULES=['bar.pp.jsm', 'foo.pp.jsm'],
            FAIL_ON_WARNINGS=True,
            GTEST_CSRCS=['test1.c', 'test2.c'],
            GTEST_CMMSRCS=['test1.mm', 'test2.mm'],
            GTEST_CPPSRCS=['test1.cpp', 'test2.cpp'],
            HOST_CPPSRCS=['fans.cpp', 'tans.cpp'],
            HOST_CSRCS=['fans.c', 'tans.c'],
            HOST_LIBRARY_NAME='host_fans',
            LIBRARY_NAME='lib_name',
            LIBS=['fans.lib', 'tans.lib'],
            NO_DIST_INSTALL='1',
            MODULE='module_name',
            SDK_LIBRARY=['fans.sdk', 'tans.sdk'],
            SHARED_LIBRARY_LIBS=['fans.sll', 'tans.sll'],
            SIMPLE_PROGRAMS=['fans.x', 'tans.x'],
            SSRCS=['fans.S', 'tans.S'],
        )

        variables = objs[1].variables
        self.assertEqual(len(variables), len(wanted))

        for var, val in wanted.items():
            # print("test_variable_passthru[%s]" % var)
            self.assertIn(var, variables)
            self.assertEqual(variables[var], val)

    def test_exports(self):
        reader = self.reader('exports')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 2)
        self.assertIsInstance(objs[0], DirectoryTraversal)
        self.assertIsInstance(objs[1], Exports)

        exports = objs[1].exports
        self.assertEqual(exports.get_strings(), ['foo.h', 'bar.h', 'baz.h'])

        self.assertIn('mozilla', exports._children)
        mozilla = exports._children['mozilla']
        self.assertEqual(mozilla.get_strings(), ['mozilla1.h', 'mozilla2.h'])

        self.assertIn('dom', mozilla._children)
        dom = mozilla._children['dom']
        self.assertEqual(dom.get_strings(), ['dom1.h', 'dom2.h', 'dom3.h'])

        self.assertIn('gfx', mozilla._children)
        gfx = mozilla._children['gfx']
        self.assertEqual(gfx.get_strings(), ['gfx.h'])

        self.assertIn('vpx', exports._children)
        vpx = exports._children['vpx']
        self.assertEqual(vpx.get_strings(), ['mem.h', 'mem2.h'])

        self.assertIn('nspr', exports._children)
        nspr = exports._children['nspr']
        self.assertIn('private', nspr._children)
        private = nspr._children['private']
        self.assertEqual(private.get_strings(), ['pprio.h', 'pprthred.h'])

        self.assertIn('overwrite', exports._children)
        overwrite = exports._children['overwrite']
        self.assertEqual(overwrite.get_strings(), ['new.h'])

    def test_program(self):
        reader = self.reader('program')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 2)
        self.assertIsInstance(objs[0], DirectoryTraversal)
        self.assertIsInstance(objs[1], Program)

        program = objs[1].program
        self.assertEqual(program, 'test_program.prog')

    def test_xpcshell_manifests(self):
        reader = self.reader('xpcshell_manifests')
        objs = self.read_topsrcdir(reader)

        inis = []
        for o in objs:
            if isinstance(o, XpcshellManifests):
                inis.append(o.xpcshell_manifests)

        iniByDir = [
            'bar/xpcshell.ini',
            'enabled_var/xpcshell.ini',
            'foo/xpcshell.ini',
            'tans/xpcshell.ini',
            ]

        self.assertEqual(sorted(inis), iniByDir)

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

if __name__ == '__main__':
    main()
