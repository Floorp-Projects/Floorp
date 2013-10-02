# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit
import os
import unittest
from collections import OrderedDict
from mozbuild.action.link_deps import (
    DependencyLinker,
    Grouping,
)
from mozbuild.makeutil import Makefile
from StringIO import StringIO


class TestDependencyLinker(unittest.TestCase):
    def setUp(self):
        self._cwd = os.getcwd()
        os.chdir(os.path.dirname(__file__))

    def tearDown(self):
        os.chdir(self._cwd)

    def gen_depfile(self, name, rules):
        mk = Makefile()
        for target, deps in rules.items():
            mk.create_rule([target]) \
              .add_dependencies(deps)
        depfile = StringIO()
        mk.dump(depfile, removal_guard=True)
        depfile.seek(0)
        depfile.name = name
        return depfile

    def test_dependency_linker(self):
        linker = DependencyLinker(
            topsrcdir=os.path.abspath(os.path.join(os.pardir, os.pardir)),
            topobjdir=os.path.abspath(os.pardir),
            dist=os.path.abspath(os.path.join(os.pardir, 'dist'))
        )
        linker.add_dependencies(self.gen_depfile('.deps/foo.o.pp', {'foo.o': [
            '../../foo/foo.cpp',
            '/usr/include/stdint.h',
            '../dist/include/bar.h',
            '../qux_gen/qux.h',
            '../../hoge/hoge.h'
        ]}))

        linker.add_dependencies(self.gen_depfile('.deps/bar.o.pp', {'bar.o': [
            '../../foo/bar.cpp',
            '/usr/include/stdint.h',
            '../dist/include/bar.h',
            '../../foo/foo.h',
            '../../hoge/hoge.h'
        ]}))

        result = StringIO()
        linker.dump(result)

        self.assertEqual(result.getvalue().splitlines(), [
            '$(DEPTH)/test/foo.o: $(topsrcdir)/foo/foo.cpp '
                '$(DIST)/include/bar.h $(DEPTH)/qux_gen/qux.h '
                '$(topsrcdir)/hoge/hoge.h',
            '$(DEPTH)/test/bar.o: $(topsrcdir)/foo/bar.cpp '
                '$(DIST)/include/bar.h $(topsrcdir)/foo/foo.h '
                '$(topsrcdir)/hoge/hoge.h',
            '$(DEPTH)/qux_gen/qux.h $(DIST)/include/bar.h '
                '$(topsrcdir)/foo/bar.cpp $(topsrcdir)/foo/foo.cpp '
                '$(topsrcdir)/foo/foo.h $(topsrcdir)/hoge/hoge.h:'
        ])

    def test_dependency_grouping(self):
        linker = DependencyLinker(
            topsrcdir=os.path.abspath(os.path.join(os.pardir, os.pardir)),
            topobjdir=os.path.abspath(os.pardir),
            dist=os.path.abspath(os.path.join(os.pardir, 'dist')),
            group=Grouping.ALL_TARGETS
        )
        linker.add_dependencies(self.gen_depfile('.deps/foo.o.pp', {'foo.o': [
            '../../foo/foo.cpp',
            '/usr/include/stdint.h',
            '../dist/include/bar.h',
            '../qux_gen/qux.h',
            '../../hoge/hoge.h'
        ]}))

        linker.add_dependencies(self.gen_depfile('.deps/bar.o.pp', {'bar.o': [
            '../../foo/bar.cpp',
            '/usr/include/stdint.h',
            '../dist/include/bar.h',
            '../../foo/foo.h',
            '../../hoge/hoge.h'
        ]}))

        result = StringIO()
        linker.dump(result)

        self.assertEqual(result.getvalue().splitlines(), [
            '$(DEPTH)/test/foo.o $(DEPTH)/test/bar.o: '
                '$(topsrcdir)/foo/foo.cpp $(DIST)/include/bar.h '
                '$(DEPTH)/qux_gen/qux.h $(topsrcdir)/hoge/hoge.h '
                '$(topsrcdir)/foo/bar.cpp $(topsrcdir)/foo/foo.h',
            '$(DEPTH)/qux_gen/qux.h $(DIST)/include/bar.h '
                '$(topsrcdir)/foo/bar.cpp $(topsrcdir)/foo/foo.cpp '
                '$(topsrcdir)/foo/foo.h $(topsrcdir)/hoge/hoge.h:'
        ])

    def test_dependency_by_depfile(self):
        linker = DependencyLinker(
            topsrcdir=os.path.abspath(os.pardir),
            topobjdir=os.path.abspath(os.curdir),
            dist=os.path.abspath('dist'),
            group=Grouping.BY_DEPFILE
        )
        linker.add_dependencies(self.gen_depfile('foo/binaries', OrderedDict([
            ('$(DEPTH)/foo/foo.o', [
                '$(topsrcdir)/foo/foo.cpp',
                '$(DIST)/include/bar.h',
                '$(DEPTH)/qux_gen/qux.h',
                '$(topsrcdir)/hoge/hoge.h',
            ]),
            ('$(DEPTH)/foo/bar.o', [
                '$(topsrcdir)/foo/bar.cpp',
                '$(DIST)/include/bar.h',
                '$(topsrcdir)/foo/foo.h',
                '$(topsrcdir)/hoge/hoge.h',
            ]),
            ('$(DEPTH)/foo/foobar.lib', [
                '$(DEPTH)/foo/foo.o',
                '$(DEPTH)/foo/bar.o',
            ]),
            ('$(DIST)/lib/foobar.lib', [ '$(DEPTH)/foo/foobar.lib' ]),
        ])))
        linker.add_dependencies(self.gen_depfile('hoge/binaries', OrderedDict([
            ('$(DEPTH)/hoge/hoge.exe', [
                '$(DEPTH)/hoge/hoge.o',
                '$(DIST)/lib/foobar.lib',
            ]),
            ('$(DEPTH)/hoge/hoge.o', [
                '$(topsrcdir)/hoge/hoge.cpp',
                '$(topsrcdir)/hoge/hoge.h',
            ]),
        ])))

        result = StringIO()
        linker.dump(result, removal_guard=False)

        self.assertEqual(result.getvalue().splitlines(), [
            '$(DEPTH)/foo/binaries: $(topsrcdir)/foo/foo.cpp '
                '$(DIST)/include/bar.h $(DEPTH)/qux_gen/qux.h '
                '$(topsrcdir)/hoge/hoge.h $(topsrcdir)/foo/bar.cpp '
                '$(topsrcdir)/foo/foo.h',
            '$(DEPTH)/hoge/binaries: $(DEPTH)/foo/binaries '
                '$(topsrcdir)/hoge/hoge.cpp $(topsrcdir)/hoge/hoge.h',
        ])

if __name__ == '__main__':
    mozunit.main()
