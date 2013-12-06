# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Read dependency Makefiles and merge them together. Also normalizes paths
relative to a given topobjdir and topsrcdir, and drops files outside topobjdir
and topsrcdir.

The build creates many .pp files under .deps directories. Those contain
specific dependencies for individual targets.

As an example, let's say we have a tree like the following:
  foo/ contains foo1.cpp and foo2.cpp, built, respectively, as foo1.o and
  foo2.o, and depending on foo.h, in the same directory, and bar.h in a bar/
  directory.
  bar/ contains bar.cpp, built as bar.cpp, and depending on bar.h.
  build/ contains no source, but builds a libfoobar.a library from foo1.o,
  foo2.o and bar.o.

In that example, we have .pp files like the following (ignoring removal
guards):
  foo/.deps/foo1.o.pp:
    foo1.o: /path/to/foo/foo1.cpp /path/to/foo/foo.h \
            /path/to/bar/bar.h /usr/include/some/system/headers
  foo/.deps/foo2.o.pp:
    foo2.o: /path/to/foo/foo2.cpp /path/to/foo/foo.h \
            /path/to/bar/bar.h /usr/include/some/system/headers
  bar/.deps/bar.o.pp:
    bar.o: /path/to/bar/bar.cpp /path/to/bar/bar.h \
           /usr/include/some/system/headers
  build/.deps/libfoobar.a.pp:
    libfoobar.a: ../foo/foo1.o ../foo/foo2.o ../bar/bar.o

The first stage is to group and normalize all dependencies per directory.
This is the Grouping.ALL_TARGETS case.

The intermediate result would be:
  foo/binaries:
    $(DEPTH)/foo/foo1.o $(DEPTH)/foo/foo2.o: $(topsrcdir)/foo/foo1.cpp \
      $(topsrcdir)/foo/foo2.cpp $(topsrcdir)/foo/foo.h $(topsrcdir)/bar/bar.h
  bar/binaries:
    $(DEPTH)/bar/bar.o: $(topsrcdir)/bar/bar.cpp $(topsrcdir)/bar/bar.h
  build/binaries:
    $(DEPTH)/build/libfoobar.a: $(DEPTH)/foo/foo1.o $(DEPTH)/foo/foo2.o \
      $(DEPTH)/bar/bar.o

Those files are not meant to be directly included by a Makefile, so the broad
dependencies they represent is not a problem. But it helps making the next step
faster, by reading less and smaller files.

The second stage is grouping those rules by file they are defined in. This
means replacing occurrences of, e.g. $(DEPTH)/foo/foo1.o with the file that
defines $(DEPTH)/foo/foo1.o as a target, which is $(DEPTH)/foo/binaries.

The end result would thus be:
  binaries-deps.mk:
    $(DEPTH)/foo/binaries: $(topsrcdir)/foo/foo1.cpp \
      $(topsrcdir)/foo/foo2.cpp $(topsrcdir)/foo/foo.h \
      $(topsrcdir)/bar/bar.h
    $(DEPTH)/bar/binaries: $(topsrcdir)/bar/bar.cpp $(topsrcdir)/bar/bar.h
    $(DEPTH)/build/binaries: $(DEPTH)/foo/binaries $(DEPTH)/bar/binaries

A change in foo/foo1.cpp would then trigger a rebuild of foo/binaries and
build/binaries.
'''

import argparse
import os
import sys
from collections import OrderedDict
from mozbuild.makeutil import (
    Makefile,
    read_dep_makefile,
)
import mozpack.path as mozpath


def enum(**enums):
    return type('Enum', (), enums)


Grouping = enum(NO=0, BY_DEPFILE=1, ALL_TARGETS=2)


class DependencyLinker(Makefile):
    def __init__(self, topsrcdir, topobjdir, dist, group=Grouping.NO,
                 abspaths=False):
        topsrcdir = mozpath.normsep(os.path.normcase(os.path.abspath(topsrcdir)))
        topobjdir = mozpath.normsep(os.path.normcase(os.path.abspath(topobjdir)))
        dist = mozpath.normsep(os.path.normcase(os.path.abspath(dist)))
        if abspaths:
            topsrcdir_value = topsrcdir
            topobjdir_value = topobjdir
            dist_value = dist
        else:
            topsrcdir_value = '$(topsrcdir)'
            topobjdir_value = '$(DEPTH)'
            dist_value = '$(DIST)'

        self._normpaths = {
            topsrcdir: topsrcdir_value,
            topobjdir: topobjdir_value,
            dist: dist_value,
            '$(topsrcdir)': topsrcdir_value,
            '$(DEPTH)': topobjdir_value,
            '$(DIST)': dist_value,
            '$(depth)': topobjdir_value, # normcase may lowercase variable refs when
            '$(dist)': dist_value,       # they are in the original dependency file
            mozpath.relpath(topobjdir, os.curdir): topobjdir_value,
            mozpath.relpath(dist, os.curdir): dist_value,
        }
        try:
            # mozpath.relpath(topsrcdir, os.curdir) fails when source directory
            # and object directory are not on the same drive on Windows. In
            # this case, the value is not useful in self._normpaths anyways.
            self._normpaths[mozpath.relpath(topsrcdir, os.curdir)] = topsrcdir_value
        except ValueError:
            pass

        Makefile.__init__(self)
        self._group = group
        self._targets = OrderedDict()

    def add_dependencies(self, fh):
        depfile = self.normpath(os.path.abspath(fh.name))
        for rule in read_dep_makefile(fh):
            deps = list(rule.dependencies())
            if deps:
                deps = list(self.normpaths(deps))
                for t in self.normpaths(rule.targets()):
                    if t in self._targets:
                        raise Exception('Found target %s in %s and %s'
                                        % (t, self._targets[t][0], depfile))
                    self._targets[t] = (depfile, deps)

    def dump(self, fh, removal_guard=True):
        rules = {}
        for t, (depfile, deps) in self._targets.items():
            if self._group == Grouping.BY_DEPFILE:
                if depfile not in rules:
                    rules[depfile] = self.create_rule([depfile])
                rules[depfile].add_dependencies(d if d not in self._targets else self._targets[d][0] for d in deps)
            elif self._group == Grouping.ALL_TARGETS:
                if 'all' not in rules:
                    rules['all'] = self.create_rule()
                rules['all'].add_targets([t]) \
                            .add_dependencies(deps)
            elif self._group == Grouping.NO:
                self.create_rule([t]) \
                    .add_dependencies(deps)
        Makefile.dump(self, fh, removal_guard)

    # os.path.abspath is slow, especially when run hundred of thousands of
    # times with a lot of times the same path or partial path, so cleverly
    # cache results and partial results.
    def normpath(self, path):
        key = mozpath.normsep(os.path.normcase(path))
        result = self._normpaths.get(key, False)
        if result != False:
            return result
        dir, file = os.path.split(path)
        if file in ['.', '..']:
            result = self.normpath(os.path.abspath(path))
            self._normpaths[key] = result
            return result
        if dir != path:
            if dir == '':
               dir = '.'
            d = self.normpath(dir)
            if d:
                result = '%s/%s' % (d, os.path.basename(path))
                self._normpaths[key] = result
                return result
        self._normpaths[key] = None
        return None

    def normpaths(self, paths):
        for p in paths:
            r = self.normpath(p)
            if r:
                yield r


def main(args):
    parser = argparse.ArgumentParser(description='Link dependency files.')
    parser.add_argument('--topsrcdir', required=True,
        help='Path to the top-level source directory.')
    parser.add_argument('--topobjdir', required=True,
        help='Path to the top-level object directory.')
    parser.add_argument('--dist', required=True,
        help='Path to the dist directory.')
    parser.add_argument('-o', '--output',
        help='Send output to the given file.')
    parser.add_argument('dependency_files', nargs='*')
    parser.add_argument('--guard', action="store_true",
        help='Add removal guards in the linked result.')
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--group-by-depfile', action='store_true',
        help='Group dependencies by depfile.')
    group.add_argument('--group-all', action='store_true',
        help='Group all dependencies under one target.')
    parser.add_argument('--abspaths', action='store_true',
        help='Use absolute paths instead of using make variable references.')
    opts = parser.parse_args(args)

    if opts.group_by_depfile:
        group = Grouping.BY_DEPFILE
    elif opts.group_all:
        group = Grouping.ALL_TARGETS
    else:
        group = Grouping.NO
    linker = DependencyLinker(topsrcdir=opts.topsrcdir,
                              topobjdir=opts.topobjdir,
                              dist=opts.dist,
                              group=group,
                              abspaths=opts.abspaths)
    for f in opts.dependency_files:
        linker.add_dependencies(open(f))

    if opts.output:
        out = open(opts.output, 'w')
    else:
        out = sys.stdout
    linker.dump(out, removal_guard=opts.guard)


if __name__ == '__main__':
    main(sys.argv[1:])
