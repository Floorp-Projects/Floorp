# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import itertools
import json
import logging
import os
import types

from collections import namedtuple

import mozwebidlcodegen

import mozbuild.makeutil as mozmakeutil
from mozpack.copier import FilePurger
from mozpack.manifests import (
    InstallManifest,
)
import mozpack.path as mozpath

from .common import CommonBackend
from ..frontend.data import (
    AndroidEclipseProjectData,
    ConfigFileSubstitution,
    Defines,
    DirectoryTraversal,
    Exports,
    GeneratedInclude,
    HostProgram,
    HostSimpleProgram,
    InstallationTarget,
    IPDLFile,
    JARManifest,
    JavaJarData,
    LibraryDefinition,
    LocalInclude,
    PerSourceFlag,
    Program,
    Resources,
    SandboxDerived,
    SandboxWrapped,
    SimpleProgram,
    TestManifest,
    VariablePassthru,
    XPIDLFile,
)
from ..util import (
    ensureParentDir,
    FileAvoidWrite,
)
from ..makeutil import Makefile

class BackendMakeFile(object):
    """Represents a generated backend.mk file.

    This is both a wrapper around a file handle as well as a container that
    holds accumulated state.

    It's worth taking a moment to explain the make dependencies. The
    generated backend.mk as well as the Makefile.in (if it exists) are in the
    GLOBAL_DEPS list. This means that if one of them changes, all targets
    in that Makefile are invalidated. backend.mk also depends on all of its
    input files.

    It's worth considering the effect of file mtimes on build behavior.

    Since we perform an "all or none" traversal of moz.build files (the whole
    tree is scanned as opposed to individual files), if we were to blindly
    write backend.mk files, the net effect of updating a single mozbuild file
    in the tree is all backend.mk files have new mtimes. This would in turn
    invalidate all make targets across the whole tree! This would effectively
    undermine incremental builds as any mozbuild change would cause the entire
    tree to rebuild!

    The solution is to not update the mtimes of backend.mk files unless they
    actually change. We use FileAvoidWrite to accomplish this.
    """

    def __init__(self, srcdir, objdir, environment):
        self.srcdir = srcdir
        self.objdir = objdir
        self.relobjdir = objdir[len(environment.topobjdir) + 1:]
        self.environment = environment
        self.name = mozpath.join(objdir, 'backend.mk')

        # XPIDLFiles attached to this file.
        self.idls = []
        self.xpt_name = None

        self.fh = FileAvoidWrite(self.name, capture_diff=True)
        self.fh.write('# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT EDIT.\n')
        self.fh.write('\n')

    def write(self, buf):
        self.fh.write(buf)

    # For compatibility with makeutil.Makefile
    def add_statement(self, stmt):
        self.write('%s\n' % stmt)

    def close(self):
        if self.xpt_name:
            self.fh.write('XPT_NAME := %s\n' % self.xpt_name)

            # We just recompile all xpidls because it's easier and less error
            # prone.
            self.fh.write('NONRECURSIVE_TARGETS += export\n')
            self.fh.write('NONRECURSIVE_TARGETS_export += xpidl\n')
            self.fh.write('NONRECURSIVE_TARGETS_export_xpidl_DIRECTORY = '
                '$(DEPTH)/xpcom/xpidl\n')
            self.fh.write('NONRECURSIVE_TARGETS_export_xpidl_TARGETS += '
                'export\n')

        return self.fh.close()

    @property
    def diff(self):
        return self.fh.diff


class RecursiveMakeTraversal(object):
    """
    Helper class to keep track of how the "traditional" recursive make backend
    recurses subdirectories. This is useful until all adhoc rules are removed
    from Makefiles.

    Each directory may have one or more types of subdirectories:
        - parallel
        - static
        - (normal) dirs
        - tests
        - tools

    The "traditional" recursive make backend recurses through those by first
    building the current directory, followed by parallel directories (in
    parallel), then static directories, dirs, tests and tools (all
    sequentially).
    """
    SubDirectoryCategories = ['parallel', 'static', 'dirs', 'tests', 'tools']
    SubDirectoriesTuple = namedtuple('SubDirectories', SubDirectoryCategories)
    class SubDirectories(SubDirectoriesTuple):
        def __new__(self):
            return RecursiveMakeTraversal.SubDirectoriesTuple.__new__(self, [], [], [], [], [])

    def __init__(self):
        self._traversal = {}

    def add(self, dir, **kargs):
        """
        Function signature is, in fact:
            def add(self, dir, parallel=[], static=[], dirs=[],
                               tests=[], tools=[])
        but it's done with **kargs to avoid repetitive code.

        Adds a directory to traversal, registering its subdirectories,
        sorted by categories. If the directory was already added to
        traversal, adds the new subdirectories to the already known lists.
        """
        subdirs = self._traversal.setdefault(dir, self.SubDirectories())
        for key, value in kargs.items():
            assert(key in self.SubDirectoryCategories)
            getattr(subdirs, key).extend(value)

    @staticmethod
    def default_filter(current, subdirs):
        """
        Default filter for use with compute_dependencies and traverse.
        """
        return current, subdirs.parallel, \
               subdirs.static + subdirs.dirs + subdirs.tests + subdirs.tools

    def call_filter(self, current, filter):
        """
        Helper function to call a filter from compute_dependencies and
        traverse.
        """
        return filter(current, self._traversal.get(current,
            self.SubDirectories()))

    def compute_dependencies(self, filter=None):
        """
        Compute make dependencies corresponding to the registered directory
        traversal.

        filter is a function with the following signature:
            def filter(current, subdirs)
        where current is the directory being traversed, and subdirs the
        SubDirectories instance corresponding to it.
        The filter function returns a tuple (filtered_current, filtered_parallel,
        filtered_dirs) where filtered_current is either current or None if
        the current directory is to be skipped, and filtered_parallel and
        filtered_dirs are lists of parallel directories and sequential
        directories, which can be rearranged from whatever is given in the
        SubDirectories members.

        The default filter corresponds to a default recursive traversal.
        """
        filter = filter or self.default_filter

        deps = {}

        def recurse(start_node, prev_nodes=None):
            current, parallel, sequential = self.call_filter(start_node, filter)
            if current is not None:
                if start_node != '':
                    deps[start_node] = prev_nodes
                prev_nodes = (start_node,)
            if not start_node in self._traversal:
                return prev_nodes
            parallel_nodes = []
            for node in parallel:
                nodes = recurse(node, prev_nodes)
                if nodes and nodes != ('',):
                    parallel_nodes.extend(nodes)
            if parallel_nodes:
                prev_nodes = tuple(parallel_nodes)
            for dir in sequential:
                prev_nodes = recurse(dir, prev_nodes)
            return prev_nodes

        return recurse(''), deps

    def traverse(self, start, filter=None):
        """
        Iterate over the filtered subdirectories, following the traditional
        make traversal order.
        """
        if filter is None:
            filter = self.default_filter

        current, parallel, sequential = self.call_filter(start, filter)
        if current is not None:
            yield start
        if not start in self._traversal:
            return
        for node in parallel:
            for n in self.traverse(node, filter):
                yield n
        for dir in sequential:
            for d in self.traverse(dir, filter):
                yield d

    def get_subdirs(self, dir):
        """
        Returns all direct subdirectories under the given directory.
        """
        return self._traversal.get(dir, self.SubDirectories())


class RecursiveMakeBackend(CommonBackend):
    """Backend that integrates with the existing recursive make build system.

    This backend facilitates the transition from Makefile.in to moz.build
    files.

    This backend performs Makefile.in -> Makefile conversion. It also writes
    out .mk files containing content derived from moz.build files. Both are
    consumed by the recursive make builder.

    This backend may eventually evolve to write out non-recursive make files.
    However, as long as there are Makefile.in files in the tree, we are tied to
    recursive make and thus will need this backend.
    """

    def _init(self):
        CommonBackend._init(self)

        self._backend_files = {}
        self._ipdl_sources = set()

        def detailed(summary):
            s = '{:d} total backend files; ' \
                '{:d} created; {:d} updated; {:d} unchanged; ' \
                '{:d} deleted; {:d} -> {:d} Makefile'.format(
                summary.created_count + summary.updated_count +
                summary.unchanged_count,
                summary.created_count,
                summary.updated_count,
                summary.unchanged_count,
                summary.deleted_count,
                summary.makefile_in_count,
                summary.makefile_out_count)

            return s

        # This is a little kludgy and could be improved with a better API.
        self.summary.backend_detailed_summary = types.MethodType(detailed,
            self.summary)
        self.summary.makefile_in_count = 0
        self.summary.makefile_out_count = 0

        self._test_manifests = {}

        self.backend_input_files.add(mozpath.join(self.environment.topobjdir,
            'config', 'autoconf.mk'))

        self._install_manifests = {
            k: InstallManifest() for k in [
                'dist_bin',
                'dist_idl',
                'dist_include',
                'dist_public',
                'dist_private',
                'dist_sdk',
                'tests',
                'xpidl',
            ]}

        self._traversal = RecursiveMakeTraversal()
        self._may_skip = {
            'export': set(),
            'compile': set(),
            'binaries': set(),
            'libs': set(),
            'tools': set(),
        }

        derecurse = self.environment.substs.get('MOZ_PSEUDO_DERECURSE', '').split(',')
        self._parallel_export = False
        self._no_skip = False
        if derecurse != ['']:
            self._parallel_export = 'no-parallel-export' not in derecurse
            self._no_skip = 'no-skip' in derecurse

    def consume_object(self, obj):
        """Write out build files necessary to build with recursive make."""

        if not isinstance(obj, SandboxDerived):
            return

        if obj.objdir not in self._backend_files:
            self._backend_files[obj.objdir] = \
                BackendMakeFile(obj.srcdir, obj.objdir, obj.config)
        backend_file = self._backend_files[obj.objdir]

        CommonBackend.consume_object(self, obj)

        # CommonBackend handles XPIDLFile and TestManifest, but we want to do
        # some extra things for them.
        if isinstance(obj, XPIDLFile):
            backend_file.idls.append(obj)
            backend_file.xpt_name = '%s.xpt' % obj.module

        elif isinstance(obj, TestManifest):
            self._process_test_manifest(obj, backend_file)

        # If CommonBackend acknowledged the object, we're done with it.
        if obj._ack:
            return

        if isinstance(obj, DirectoryTraversal):
            self._process_directory_traversal(obj, backend_file)
        elif isinstance(obj, ConfigFileSubstitution):
            # Other ConfigFileSubstitution should have been acked by
            # CommonBackend.
            assert os.path.basename(obj.output_path) == 'Makefile'
            self._create_makefile(obj)
        elif isinstance(obj, VariablePassthru):
            unified_suffixes = dict(
                UNIFIED_CSRCS='c',
                UNIFIED_CMMSRCS='mm',
                UNIFIED_CPPSRCS='cpp',
            )

            files_per_unification = 16
            if 'FILES_PER_UNIFIED_FILE' in obj.variables.keys():
                files_per_unification = obj.variables['FILES_PER_UNIFIED_FILE']

            do_unify = not self.environment.substs.get(
                'MOZ_DISABLE_UNIFIED_COMPILATION') and files_per_unification > 1

            # Sorted so output is consistent and we don't bump mtimes.
            for k, v in sorted(obj.variables.items()):
                if k in unified_suffixes:
                    if do_unify:
                        # On Windows, path names have a maximum length of 255 characters,
                        # so avoid creating extremely long path names.
                        unified_prefix = backend_file.relobjdir
                        if len(unified_prefix) > 30:
                            unified_prefix = unified_prefix[-30:].split('/', 1)[-1]
                        unified_prefix = unified_prefix.replace('/', '_')

                        self._add_unified_build_rules(backend_file, v,
                            backend_file.objdir,
                            unified_prefix='Unified_%s_%s' % (
                                unified_suffixes[k],
                                unified_prefix),
                            unified_suffix=unified_suffixes[k],
                            unified_files_makefile_variable=k,
                            include_curdir_build_rules=False,
                            files_per_unified_file=files_per_unification)
                        backend_file.write('%s += $(%s)\n' % (k[len('UNIFIED_'):], k))
                    else:
                        backend_file.write('%s += %s\n' % (
                            k[len('UNIFIED_'):], ' '.join(sorted(v))))
                elif isinstance(v, list):
                    for item in v:
                        backend_file.write('%s += %s\n' % (k, item))
                elif isinstance(v, bool):
                    if v:
                        backend_file.write('%s := 1\n' % k)
                else:
                    backend_file.write('%s := %s\n' % (k, v))

        elif isinstance(obj, Defines):
            defines = obj.get_defines()
            if defines:
                backend_file.write('DEFINES +=')
                for define in defines:
                    backend_file.write(' %s' % define)
                backend_file.write('\n')

        elif isinstance(obj, Exports):
            self._process_exports(obj, obj.exports, backend_file)

        elif isinstance(obj, Resources):
            self._process_resources(obj, obj.resources, backend_file)

        elif isinstance(obj, JARManifest):
            backend_file.write('JAR_MANIFEST := %s\n' % obj.path)

        elif isinstance(obj, IPDLFile):
            self._ipdl_sources.add(mozpath.join(obj.srcdir, obj.basename))

        elif isinstance(obj, Program):
            self._process_program(obj.program, backend_file)

        elif isinstance(obj, HostProgram):
            self._process_host_program(obj.program, backend_file)

        elif isinstance(obj, SimpleProgram):
            self._process_simple_program(obj.program, backend_file)

        elif isinstance(obj, HostSimpleProgram):
            self._process_host_simple_program(obj.program, backend_file)

        elif isinstance(obj, LocalInclude):
            self._process_local_include(obj.path, backend_file)

        elif isinstance(obj, GeneratedInclude):
            self._process_generated_include(obj.path, backend_file)

        elif isinstance(obj, PerSourceFlag):
            self._process_per_source_flag(obj, backend_file)

        elif isinstance(obj, InstallationTarget):
            self._process_installation_target(obj, backend_file)

        elif isinstance(obj, SandboxWrapped):
            # Process a rich build system object from the front-end
            # as-is.  Please follow precedent and handle CamelCaseData
            # in a function named _process_camel_case_data.  At some
            # point in the future, this unwrapping process may be
            # automated.
            if isinstance(obj.wrapped, JavaJarData):
                self._process_java_jar_data(obj.wrapped, backend_file)
            elif isinstance(obj.wrapped, AndroidEclipseProjectData):
                self._process_android_eclipse_project_data(obj.wrapped, backend_file)
            else:
                return

        elif isinstance(obj, LibraryDefinition):
            self._process_library_definition(obj, backend_file)

        else:
            return
        obj.ack()

    def _fill_root_mk(self):
        """
        Create two files, root.mk and root-deps.mk, the first containing
        convenience variables, and the other dependency definitions for a
        hopefully proper directory traversal.
        """
        if not self._no_skip:
            for tier, skip in self._may_skip.items():
                self.log(logging.DEBUG, 'fill_root_mk', {
                    'number': len(skip), 'tier': tier
                    }, 'Ignoring {number} directories during {tier}')

        # Traverse directories in parallel, and skip static dirs
        def parallel_filter(current, subdirs):
            all_subdirs = subdirs.parallel + subdirs.dirs + \
                          subdirs.tests + subdirs.tools
            if current in self._may_skip[tier]:
                current = None
            # subtiers/*_start and subtiers/*_finish, under subtiers/*, are
            # kept sequential. Others are all forced parallel.
            if current and current.startswith('subtiers/') and all_subdirs and \
                    all_subdirs[0].startswith('subtiers/'):
                return current, [], all_subdirs
            return current, all_subdirs, []

        # build everything in parallel, including static dirs
        def compile_filter(current, subdirs):
            current, parallel, sequential = parallel_filter(current, subdirs)
            return current, subdirs.static + parallel, sequential

        # Skip static dirs during export traversal, or build everything in
        # parallel when enabled.
        def export_filter(current, subdirs):
            if self._parallel_export:
                return parallel_filter(current, subdirs)
            return current, subdirs.parallel, \
                subdirs.dirs + subdirs.tests + subdirs.tools

        # Skip tools dirs during libs traversal. Because of bug 925236 and
        # possible other unknown race conditions, don't parallelize the libs
        # tier.
        def libs_filter(current, subdirs):
            if current in self._may_skip[tier]:
                current = None
            return current, [], subdirs.parallel + \
                subdirs.dirs + subdirs.tests

        # Because of bug 925236 and possible other unknown race conditions,
        # don't parallelize the tools tier.
        def tools_filter(current, subdirs):
            if current in self._may_skip[tier]:
                current = None
            return current, subdirs.parallel, \
                subdirs.dirs + subdirs.tests + subdirs.tools

        # compile, binaries and tools tiers use the same traversal as export
        filters = {
            'export': export_filter,
            'compile': compile_filter,
            'binaries': parallel_filter,
            'libs': libs_filter,
            'tools': tools_filter,
        }

        root_deps_mk = Makefile()

        # Fill the dependencies for traversal of each tier.
        for tier, filter in filters.items():
            main, all_deps = \
                self._traversal.compute_dependencies(filter)
            for dir, deps in all_deps.items():
                rule = root_deps_mk.create_rule(['%s/%s' % (dir, tier)])
                if deps is not None:
                    rule.add_dependencies('%s/%s' % (d, tier) for d in deps if d)
            rule = root_deps_mk.create_rule(['recurse_%s' % tier])
            if main:
                rule.add_dependencies('%s/%s' % (d, tier) for d in main)

        root_mk = Makefile()

        # Fill root.mk with the convenience variables.
        for tier, filter in filters.items() + [('all', self._traversal.default_filter)]:
            # Gather filtered subtiers for the given tier
            all_direct_subdirs = reduce(lambda x, y: x + y,
                                        self._traversal.get_subdirs(''), [])
            direct_subdirs = [d for d in all_direct_subdirs
                              if filter(d, self._traversal.get_subdirs(d))[0]]
            subtiers = [d.replace('subtiers/', '') for d in direct_subdirs
                        if d.startswith('subtiers/')]

            if tier != 'all':
                # Gather filtered directories for the given tier
                dirs = [d for d in direct_subdirs if not d.startswith('subtiers/')]
                if dirs:
                    # For build systems without tiers (js/src), output a list
                    # of directories for each tier.
                    all_dirs = self._traversal.traverse('', filter)
                    root_mk.add_statement('%s_dirs := %s' % (tier, ' '.join(all_dirs)))
                    continue
                if subtiers:
                    # Output the list of filtered subtiers for the given tier.
                    root_mk.add_statement('%s_subtiers := %s' % (tier, ' '.join(subtiers)))

            for subtier in subtiers:
                # subtier_dirs[0] is 'subtiers/%s_start' % subtier, skip it
                subtier_dirs = list(self._traversal.traverse('subtiers/%s_start' % subtier, filter))[1:]
                if tier == 'all':
                    for dir in subtier_dirs:
                        # Output convenience variables to be able to map directories
                        # to subtier names from Makefiles.
                        stamped = dir.replace('/', '_')
                        root_mk.add_statement('subtier_of_%s := %s' % (stamped, subtier))

                else:
                    # Output the list of filtered directories for each tier/subtier
                    # pair.
                    root_mk.add_statement('%s_subtier_%s := %s' % (tier, subtier, ' '.join(subtier_dirs)))

        root_mk.add_statement('$(call include_deps,root-deps.mk)')

        with self._write_file(
                mozpath.join(self.environment.topobjdir, 'root.mk')) as root:
            root_mk.dump(root, removal_guard=False)

        with self._write_file(
                mozpath.join(self.environment.topobjdir, 'root-deps.mk')) as root_deps:
            root_deps_mk.dump(root_deps, removal_guard=False)

    def _add_unified_build_rules(self, makefile, files, output_directory,
                                 unified_prefix='Unified',
                                 unified_suffix='cpp',
                                 extra_dependencies=[],
                                 unified_files_makefile_variable='unified_files',
                                 include_curdir_build_rules=True,
                                 poison_windows_h=False,
                                 files_per_unified_file=16):

        # In case it's a generator.
        files = sorted(files)

        explanation = "\n" \
            "# We build files in 'unified' mode by including several files\n" \
            "# together into a single source file.  This cuts down on\n" \
            "# compilation times and debug information size.  %d was chosen as\n" \
            "# a reasonable compromise between clobber rebuild time, incremental\n" \
            "# rebuild time, and compiler memory usage." % files_per_unified_file
        makefile.add_statement(explanation)

        def unified_files():
            "Return an iterator of (unified_filename, source_filenames) tuples."
            # Our last returned list of source filenames may be short, and we
            # don't want the fill value inserted by izip_longest to be an
            # issue.  So we do a little dance to filter it out ourselves.
            dummy_fill_value = ("dummy",)
            def filter_out_dummy(iterable):
                return itertools.ifilter(lambda x: x != dummy_fill_value,
                                         iterable)

            # From the itertools documentation, slightly modified:
            def grouper(n, iterable):
                "grouper(3, 'ABCDEFG', 'x') --> ABC DEF Gxx"
                args = [iter(iterable)] * n
                return itertools.izip_longest(fillvalue=dummy_fill_value, *args)

            for i, unified_group in enumerate(grouper(files_per_unified_file,
                                                      files)):
                just_the_filenames = list(filter_out_dummy(unified_group))
                yield '%s%d.%s' % (unified_prefix, i, unified_suffix), just_the_filenames

        all_sources = ' '.join(source for source, _ in unified_files())
        makefile.add_statement('%s := %s' % (unified_files_makefile_variable,
                                               all_sources))

        for unified_file, source_filenames in unified_files():
            if extra_dependencies:
                rule = makefile.create_rule([unified_file])
                rule.add_dependencies(extra_dependencies)

            # The rule we just defined is only for cases where the cpp files get
            # blown away and we need to regenerate them.  The rule doesn't correctly
            # handle source files being added/removed/renamed.  Therefore, we
            # generate them here also to make sure everything's up-to-date.
            with self._write_file(mozpath.join(output_directory, unified_file)) as f:
                f.write('#define MOZ_UNIFIED_BUILD\n')
                includeTemplate = '#include "%(cppfile)s"'
                if poison_windows_h:
                    includeTemplate += (
                        '\n'
                        '#ifdef _WINDOWS_\n'
                        '#error "%(cppfile)s included windows.h"\n'
                        "#endif")
                includeTemplate += (
                    '\n'
                    '#ifdef PL_ARENA_CONST_ALIGN_MASK\n'
                    '#error "%(cppfile)s uses PL_ARENA_CONST_ALIGN_MASK, '
                    'so it cannot be built in unified mode."\n'
                    '#undef PL_ARENA_CONST_ALIGN_MASK\n'
                    '#endif\n'
                    '#ifdef FORCE_PR_LOG\n'
                    '#error "%(cppfile)s forces NSPR logging, '
                    'so it cannot be built in unified mode."\n'
                    '#undef FORCE_PR_LOG\n'
                    '#endif\n'
                    '#ifdef INITGUID\n'
                    '#error "%(cppfile)s defines INITGUID, '
                    'so it cannot be built in unified mode."\n'
                    '#undef INITGUID\n'
                    '#endif')
                f.write('\n'.join(includeTemplate % { "cppfile": s } for
                                  s in source_filenames))

        if include_curdir_build_rules:
            makefile.add_statement('\n'
                '# Make sometimes gets confused between "foo" and "$(CURDIR)/foo".\n'
                '# Help it out by explicitly specifiying dependencies.')
            makefile.add_statement('all_absolute_unified_files := \\\n'
                                   '  $(addprefix $(CURDIR)/,$(%s))'
                                   % unified_files_makefile_variable)
            rule = makefile.create_rule(['$(all_absolute_unified_files)'])
            rule.add_dependencies(['$(CURDIR)/%: %'])

    def consume_finished(self):
        CommonBackend.consume_finished(self)

        for objdir, backend_file in sorted(self._backend_files.items()):
            srcdir = backend_file.srcdir
            with self._write_file(fh=backend_file) as bf:
                makefile_in = mozpath.join(srcdir, 'Makefile.in')
                makefile = mozpath.join(objdir, 'Makefile')

                # If Makefile.in exists, use it as a template. Otherwise,
                # create a stub.
                stub = not os.path.exists(makefile_in)
                if not stub:
                    self.log(logging.DEBUG, 'substitute_makefile',
                        {'path': makefile}, 'Substituting makefile: {path}')
                    self.summary.makefile_in_count += 1

                    for tier, skiplist in self._may_skip.items():
                        if tier in ('compile', 'binaries'):
                            continue
                        if bf.relobjdir in skiplist:
                            skiplist.remove(bf.relobjdir)
                else:
                    self.log(logging.DEBUG, 'stub_makefile',
                        {'path': makefile}, 'Creating stub Makefile: {path}')

                obj = self.Substitution()
                obj.output_path = makefile
                obj.input_path = makefile_in
                obj.topsrcdir = bf.environment.topsrcdir
                obj.topobjdir = bf.environment.topobjdir
                obj.config = bf.environment
                self._create_makefile(obj, stub=stub)

        # Write out a master list of all IPDL source files.
        ipdl_dir = mozpath.join(self.environment.topobjdir, 'ipc', 'ipdl')
        mk = mozmakeutil.Makefile()

        sorted_ipdl_sources = list(sorted(self._ipdl_sources))
        mk.add_statement('ALL_IPDLSRCS := %s' % ' '.join(sorted_ipdl_sources))

        def files_from(ipdl):
            base = mozpath.basename(ipdl)
            root, ext = mozpath.splitext(base)

            # Both .ipdl and .ipdlh become .cpp files
            files = ['%s.cpp' % root]
            if ext == '.ipdl':
                # .ipdl also becomes Child/Parent.cpp files
                files.extend(['%sChild.cpp' % root,
                              '%sParent.cpp' % root])
            return files

        ipdl_cppsrcs = list(itertools.chain(*[files_from(p) for p in sorted_ipdl_sources]))
        self._add_unified_build_rules(mk, ipdl_cppsrcs, ipdl_dir,
                                      unified_prefix='UnifiedProtocols',
                                      unified_files_makefile_variable='CPPSRCS')

        mk.add_statement('IPDLDIRS := %s' % ' '.join(sorted(set(mozpath.dirname(p)
            for p in self._ipdl_sources))))

        with self._write_file(mozpath.join(ipdl_dir, 'ipdlsrcs.mk')) as ipdls:
            mk.dump(ipdls, removal_guard=False)

        # These contain autogenerated sources that the build config doesn't
        # yet know about.
        # TODO Emit GENERATED_SOURCES so these special cases are dealt with
        # the proper way.
        self._may_skip['compile'] -= {'ipc/ipdl'}
        self._may_skip['compile'] -= {'dom/bindings', 'dom/bindings/test'}

        self._fill_root_mk()

        # Write out a dependency file used to determine whether a config.status
        # re-run is needed.
        inputs = sorted(p.replace(os.sep, '/') for p in self.backend_input_files)

        # We need to use $(DEPTH) so the target here matches what's in
        # rules.mk. If they are different, the dependencies don't get pulled in
        # properly.
        with self._write_file('%s.pp' % self._backend_output_list_file) as backend_deps:
            backend_deps.write('$(DEPTH)/backend.%s: %s\n' %
                (self.__class__.__name__, ' '.join(inputs)))
            for path in inputs:
                backend_deps.write('%s:\n' % path)

        with open(self._backend_output_list_file, 'a'):
            pass
        os.utime(self._backend_output_list_file, None)

        # Make the master test manifest files.
        for flavor, t in self._test_manifests.items():
            install_prefix, manifests = t
            manifest_stem = mozpath.join(install_prefix, '%s.ini' % flavor)
            self._write_master_test_manifest(mozpath.join(
                self.environment.topobjdir, '_tests', manifest_stem),
                manifests)

            # Catch duplicate inserts.
            try:
                self._install_manifests['tests'].add_optional_exists(manifest_stem)
            except ValueError:
                pass

        self._write_manifests('install', self._install_manifests)

        ensureParentDir(mozpath.join(self.environment.topobjdir, 'dist', 'foo'))

    def _process_directory_traversal(self, obj, backend_file):
        """Process a data.DirectoryTraversal instance."""
        fh = backend_file.fh

        def relativize(dirs):
            return [mozpath.normpath(mozpath.join(backend_file.relobjdir, d))
                for d in dirs]

        for tier, dirs in obj.tier_dirs.iteritems():
            fh.write('TIERS += %s\n' % tier)
            # For pseudo derecursification, subtiers are treated as pseudo
            # directories, with a special hierarchy:
            # - subtier1 - subtier1_start - dirA - dirAA
            # |          |                |      + dirAB
            # |          |                ...
            # |          |                + dirB
            # |          + subtier1_finish
            # + subtier2 - subtier2_start ...
            # ...        + subtier2_finish
            self._traversal.add('subtiers/%s' % tier,
                                dirs=['subtiers/%s_start' % tier,
                                      'subtiers/%s_finish' % tier])

            if dirs:
                fh.write('tier_%s_dirs += %s\n' % (tier, ' '.join(dirs)))
                fh.write('DIRS += $(tier_%s_dirs)\n' % tier)
                self._traversal.add('subtiers/%s_start' % tier,
                                    dirs=relativize(dirs))

            # tier_static_dirs should have the same keys as tier_dirs.
            if obj.tier_static_dirs[tier]:
                fh.write('tier_%s_staticdirs += %s\n' % (
                    tier, ' '.join(obj.tier_static_dirs[tier])))
                self._traversal.add('subtiers/%s_start' % tier,
                                    static=relativize(obj.tier_static_dirs[tier]))

            self._traversal.add('subtiers/%s_start' % tier)
            self._traversal.add('subtiers/%s_finish' % tier)
            self._traversal.add('', dirs=['subtiers/%s' % tier])

        if obj.dirs:
            fh.write('DIRS := %s\n' % ' '.join(obj.dirs))
            self._traversal.add(backend_file.relobjdir, dirs=relativize(obj.dirs))

        if obj.parallel_dirs:
            fh.write('PARALLEL_DIRS := %s\n' % ' '.join(obj.parallel_dirs))
            self._traversal.add(backend_file.relobjdir,
                                parallel=relativize(obj.parallel_dirs))

        if obj.tool_dirs:
            fh.write('TOOL_DIRS := %s\n' % ' '.join(obj.tool_dirs))
            self._traversal.add(backend_file.relobjdir,
                                tools=relativize(obj.tool_dirs))

        if obj.test_dirs:
            fh.write('TEST_DIRS := %s\n' % ' '.join(obj.test_dirs))
            if self.environment.substs.get('ENABLE_TESTS', False):
                self._traversal.add(backend_file.relobjdir,
                                    tests=relativize(obj.test_dirs))

        if obj.test_tool_dirs and \
            self.environment.substs.get('ENABLE_TESTS', False):

            fh.write('TOOL_DIRS += %s\n' % ' '.join(obj.test_tool_dirs))
            self._traversal.add(backend_file.relobjdir,
                                tools=relativize(obj.test_tool_dirs))

        # The directory needs to be registered whether subdirectories have been
        # registered or not.
        self._traversal.add(backend_file.relobjdir)

        if obj.is_tool_dir:
            fh.write('IS_TOOL_DIR := 1\n')

        if self._no_skip:
            return

        affected_tiers = set(obj.affected_tiers)
        # Until all SOURCES are really in moz.build, consider all directories
        # building binaries to require a pass at compile, too.
        if 'binaries' in affected_tiers:
            affected_tiers.add('compile')
        if 'compile' in affected_tiers or 'binaries' in affected_tiers:
            affected_tiers.add('libs')
        if obj.is_tool_dir and 'libs' in affected_tiers:
            affected_tiers.remove('libs')
            affected_tiers.add('tools')

        for tier in set(self._may_skip.keys()) - affected_tiers:
            self._may_skip[tier].add(backend_file.relobjdir)

    def _process_hierarchy(self, obj, element, namespace, action):
        """Walks the ``HierarchicalStringList`` ``element`` and performs
        ``action`` on each string in the heirarcy.

        ``action`` is a callback to be invoked with the following arguments:
        - ``source`` - The path to the source file named by the current string
        - ``dest``   - The relative path, including the namespace, of the
                       destination file.
        """
        strings = element.get_strings()
        if namespace:
            namespace += '/'

        for s in strings:
            source = mozpath.normpath(mozpath.join(obj.srcdir, s))
            dest = '%s%s' % (namespace, mozpath.basename(s))
            flags = None
            if '__getitem__' in dir(element):
                flags = element[s]
            action(source, dest, flags)

        children = element.get_children()
        for subdir in sorted(children):
            self._process_hierarchy(obj, children[subdir],
                namespace=namespace + subdir,
                action=action)

    def _process_exports(self, obj, exports, backend_file):
        # This may not be needed, but is present for backwards compatibility
        # with the old make rules, just in case.
        if not obj.dist_install:
            return

        def handle_export(source, dest, flags):
            self._install_manifests['dist_include'].add_symlink(source, dest)

            if not os.path.exists(source):
                raise Exception('File listed in EXPORTS does not exist: %s' % source)

        self._process_hierarchy(obj, exports,
                                namespace="",
                                action=handle_export)

    def _process_resources(self, obj, resources, backend_file):
        dep_path = mozpath.join(self.environment.topobjdir, '_build_manifests', '.deps', 'install')
        def handle_resource(source, dest, flags):
            if flags.preprocess:
                if dest.endswith('.in'):
                    dest = dest[:-3]
                dep_file = mozpath.join(dep_path, mozpath.basename(source) + '.pp')
                self._install_manifests['dist_bin'].add_preprocess(source, dest, dep_file, marker='%', defines=obj.defines)
            else:
                self._install_manifests['dist_bin'].add_symlink(source, dest)

            if not os.path.exists(source):
                raise Exception('File listed in RESOURCE_FILES does not exist: %s' % source)

        # Resources need to go in the 'res' subdirectory of $(DIST)/bin, so we
        # specify a root namespace of 'res'.
        self._process_hierarchy(obj, resources,
                                namespace='res',
                                action=handle_resource)

    def _process_installation_target(self, obj, backend_file):
        # A few makefiles need to be able to override the following rules via
        # make XPI_NAME=blah commands, so we default to the lazy evaluation as
        # much as possible here to avoid breaking things.
        if obj.xpiname:
            backend_file.write('XPI_NAME = %s\n' % (obj.xpiname))
        if obj.subdir:
            backend_file.write('DIST_SUBDIR = %s\n' % (obj.subdir))
        if obj.target and not obj.is_custom():
            backend_file.write('FINAL_TARGET = $(DEPTH)/%s\n' % (obj.target))
        else:
            backend_file.write('FINAL_TARGET = $(if $(XPI_NAME),$(DIST)/xpi-stage/$(XPI_NAME),$(DIST)/bin)$(DIST_SUBDIR:%=/%)\n')

        if not obj.enabled:
            backend_file.write('NO_DIST_INSTALL := 1\n')

    def _handle_idl_manager(self, manager):
        build_files = self._install_manifests['xpidl']

        for p in ('Makefile', 'backend.mk', '.deps/.mkdir.done',
            'xpt/.mkdir.done'):
            build_files.add_optional_exists(p)

        for idl in manager.idls.values():
            self._install_manifests['dist_idl'].add_symlink(idl['source'],
                idl['basename'])
            self._install_manifests['dist_include'].add_optional_exists('%s.h'
                % idl['root'])

        for module in manager.modules:
            build_files.add_optional_exists(mozpath.join('xpt',
                '%s.xpt' % module))
            build_files.add_optional_exists(mozpath.join('.deps',
                '%s.pp' % module))

        modules = manager.modules
        xpt_modules = sorted(modules.keys())
        rules = []

        for module in xpt_modules:
            deps = sorted(modules[module])
            idl_deps = ['$(dist_idl_dir)/%s.idl' % dep for dep in deps]
            rules.extend([
                # It may seem strange to have the .idl files listed as
                # prerequisites both here and in the auto-generated .pp files.
                # It is necessary to list them here to handle the case where a
                # new .idl is added to an xpt. If we add a new .idl and nothing
                # else has changed, the new .idl won't be referenced anywhere
                # except in the command invocation. Therefore, the .xpt won't
                # be rebuilt because the dependencies say it is up to date. By
                # listing the .idls here, we ensure the make file has a
                # reference to the new .idl. Since the new .idl presumably has
                # an mtime newer than the .xpt, it will trigger xpt generation.
                '$(idl_xpt_dir)/%s.xpt: %s' % (module, ' '.join(idl_deps)),
                '\t@echo "$(notdir $@)"',
                '\t$(idlprocess) $(basename $(notdir $@)) %s' % ' '.join(deps),
                '',
            ])

        # Create dependency for output header so we force regeneration if the
        # header was deleted. This ideally should not be necessary. However,
        # some processes (such as PGO at the time this was implemented) wipe
        # out dist/include without regard to our install manifests.

        obj = self.Substitution()
        obj.output_path = mozpath.join(self.environment.topobjdir, 'config',
            'makefiles', 'xpidl', 'Makefile')
        obj.input_path = mozpath.join(self.environment.topsrcdir, 'config',
            'makefiles', 'xpidl', 'Makefile.in')
        obj.topsrcdir = self.environment.topsrcdir
        obj.topobjdir = self.environment.topobjdir
        obj.config = self.environment
        self._create_makefile(obj, extra=dict(
            xpidl_rules='\n'.join(rules),
            xpidl_modules=' '.join(xpt_modules),
        ))

    def _process_program(self, program, backend_file):
        backend_file.write('PROGRAM = %s\n' % program)

    def _process_host_program(self, program, backend_file):
        backend_file.write('HOST_PROGRAM = %s\n' % program)

    def _process_simple_program(self, program, backend_file):
        backend_file.write('SIMPLE_PROGRAMS += %s\n' % program)

    def _process_host_simple_program(self, program, backend_file):
        backend_file.write('HOST_SIMPLE_PROGRAMS += %s\n' % program)

    def _process_test_manifest(self, obj, backend_file):
        # Much of the logic in this function could be moved to CommonBackend.
        self.backend_input_files.add(mozpath.join(obj.topsrcdir,
            obj.manifest_relpath))

        # Don't allow files to be defined multiple times unless it is allowed.
        # We currently allow duplicates for non-test files or test files if
        # the manifest is listed as a duplicate.
        for source, (dest, is_test) in obj.installs.items():
            try:
                self._install_manifests['tests'].add_symlink(source, dest)
            except ValueError:
                if not obj.dupe_manifest and is_test:
                    raise

        for base, pattern, dest in obj.pattern_installs:
            try:
                self._install_manifests['tests'].add_pattern_symlink(base,
                    pattern, dest)
            except ValueError:
                if not obj.dupe_manifest:
                    raise

        for dest in obj.external_installs:
            try:
                self._install_manifests['tests'].add_optional_exists(dest)
            except ValueError:
                if not obj.dupe_manifest:
                    raise

        m = self._test_manifests.setdefault(obj.flavor,
            (obj.install_prefix, set()))
        m[1].add(obj.manifest_obj_relpath)

    def _process_local_include(self, local_include, backend_file):
        if local_include.startswith('/'):
            path = '$(topsrcdir)'
        else:
            path = '$(srcdir)/'
        backend_file.write('LOCAL_INCLUDES += -I%s%s\n' % (path, local_include))

    def _process_generated_include(self, generated_include, backend_file):
        if generated_include.startswith('/'):
            path = self.environment.topobjdir.replace('\\', '/')
        else:
            path = ''
        backend_file.write('LOCAL_INCLUDES += -I%s%s\n' % (path, generated_include))

    def _process_per_source_flag(self, per_source_flag, backend_file):
        for flag in per_source_flag.flags:
            backend_file.write('%s_FLAGS += %s\n' % (mozpath.basename(per_source_flag.file_name), flag))

    def _process_java_jar_data(self, jar, backend_file):
        target = jar.name
        backend_file.write('JAVA_JAR_TARGETS += %s\n' % target)
        backend_file.write('%s_DEST := %s.jar\n' % (target, jar.name))
        if jar.sources:
            backend_file.write('%s_JAVAFILES := %s\n' %
                (target, ' '.join(jar.sources)))
        if jar.generated_sources:
            backend_file.write('%s_PP_JAVAFILES := %s\n' %
                (target, ' '.join(mozpath.join('generated', f) for f in jar.generated_sources)))
        if jar.extra_jars:
            backend_file.write('%s_EXTRA_JARS := %s\n' %
                (target, ' '.join(jar.extra_jars)))
        if jar.javac_flags:
            backend_file.write('%s_JAVAC_FLAGS := %s\n' %
                (target, ' '.join(jar.javac_flags)))

    def _process_android_eclipse_project_data(self, project, backend_file):
        # We add a single target to the backend.mk corresponding to
        # the moz.build defining the Android Eclipse project. This
        # target depends on some targets to be fresh, and installs a
        # manifest generated by the Android Eclipse build backend. The
        # manifests for all projects live in $TOPOBJDIR/android_eclipse
        # and are installed into subdirectories thereof.

        project_directory = mozpath.join(self.environment.topobjdir, 'android_eclipse', project.name)
        manifest_path = mozpath.join(self.environment.topobjdir, 'android_eclipse', '%s.manifest' % project.name)

        fragment = Makefile()
        rule = fragment.create_rule(targets=['ANDROID_ECLIPSE_PROJECT_%s' % project.name])
        rule.add_dependencies(project.recursive_make_targets)
        args = ['--no-remove',
            '--no-remove-all-directory-symlinks',
            '--no-remove-empty-directories',
            project_directory,
            manifest_path]
        rule.add_commands(['$(call py_action,process_install_manifest,%s)' % ' '.join(args)])
        fragment.dump(backend_file.fh, removal_guard=False)

    def _process_library_definition(self, libdef, backend_file):
        backend_file.write('LIBRARY_NAME = %s\n' % libdef.basename)
        thisobjdir = libdef.objdir
        topobjdir = libdef.topobjdir.replace(os.sep, '/')
        for objdir, basename in libdef.static_libraries:
            # If this is an external objdir (i.e., comm-central), use the other
            # directory instead of $(DEPTH).
            if objdir.startswith(topobjdir + '/'):
                relpath = '$(DEPTH)/%s' % mozpath.relpath(objdir, topobjdir)
            else:
                relpath = mozpath.relpath(objdir, thisobjdir)
            backend_file.write('SHARED_LIBRARY_LIBS += %s/$(LIB_PREFIX)%s.$(LIB_SUFFIX)\n'
                               % (relpath, basename))

    def _write_manifests(self, dest, manifests):
        man_dir = mozpath.join(self.environment.topobjdir, '_build_manifests',
            dest)

        # We have a purger for the manifests themselves to ensure legacy
        # manifests are deleted.
        purger = FilePurger()

        for k, manifest in manifests.items():
            purger.add(k)

            with self._write_file(mozpath.join(man_dir, k)) as fh:
                manifest.write(fileobj=fh)

        purger.purge(man_dir)

    def _write_master_test_manifest(self, path, manifests):
        with self._write_file(path) as master:
            master.write(
                '; THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT MODIFY BY HAND.\n\n')

            for manifest in sorted(manifests):
                master.write('[include:%s]\n' % manifest)

    class Substitution(object):
        """BaseConfigSubstitution-like class for use with _create_makefile."""
        __slots__ = (
            'input_path',
            'output_path',
            'topsrcdir',
            'topobjdir',
            'config',
        )

    def _create_makefile(self, obj, stub=False, extra=None):
        '''Creates the given makefile. Makefiles are treated the same as
        config files, but some additional header and footer is added to the
        output.

        When the stub argument is True, no source file is used, and a stub
        makefile with the default header and footer only is created.
        '''
        with self._get_preprocessor(obj) as pp:
            if extra:
                pp.context.update(extra)
            if not pp.context.get('autoconfmk', ''):
                pp.context['autoconfmk'] = 'autoconf.mk'
            pp.handleLine(b'# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT MODIFY BY HAND.\n');
            pp.handleLine(b'DEPTH := @DEPTH@\n')
            pp.handleLine(b'topsrcdir := @top_srcdir@\n')
            pp.handleLine(b'srcdir := @srcdir@\n')
            pp.handleLine(b'VPATH := @srcdir@\n')
            pp.handleLine(b'relativesrcdir := @relativesrcdir@\n')
            pp.handleLine(b'include $(DEPTH)/config/@autoconfmk@\n')
            if not stub:
                pp.do_include(obj.input_path)
            # Empty line to avoid failures when last line in Makefile.in ends
            # with a backslash.
            pp.handleLine(b'\n')
            pp.handleLine(b'include $(topsrcdir)/config/recurse.mk\n')
        if not stub:
            # Adding the Makefile.in here has the desired side-effect
            # that if the Makefile.in disappears, this will force
            # moz.build traversal. This means that when we remove empty
            # Makefile.in files, the old file will get replaced with
            # the autogenerated one automatically.
            self.backend_input_files.add(obj.input_path)

        self.summary.makefile_out_count += 1

    def _handle_webidl_collection(self, webidls):
        if not webidls.all_stems():
            return

        bindings_dir = mozpath.join(self.environment.topobjdir, 'dom',
            'bindings')

        all_inputs = set(webidls.all_static_sources())
        for s in webidls.all_non_static_basenames():
            all_inputs.add(mozpath.join(bindings_dir, s))

        generated_events_stems = webidls.generated_events_stems()
        exported_stems = webidls.all_regular_stems()

        # The WebIDL manager reads configuration from a JSON file. So, we
        # need to write this file early.
        o = dict(
            webidls=sorted(all_inputs),
            generated_events_stems=sorted(generated_events_stems),
            exported_stems=sorted(exported_stems),
            example_interfaces=sorted(webidls.example_interfaces),
        )

        file_lists = mozpath.join(bindings_dir, 'file-lists.json')
        with self._write_file(file_lists) as fh:
            json.dump(o, fh, sort_keys=True, indent=2)

        manager = mozwebidlcodegen.create_build_system_manager(
            self.environment.topsrcdir,
            self.environment.topobjdir,
            mozpath.join(self.environment.topobjdir, 'dist')
        )

        # The manager is the source of truth on what files are generated.
        # Consult it for install manifests.
        include_dir = mozpath.join(self.environment.topobjdir, 'dist',
            'include')
        for f in manager.expected_build_output_files():
            if f.startswith(include_dir):
                self._install_manifests['dist_include'].add_optional_exists(
                    mozpath.relpath(f, include_dir))

        # We pass WebIDL info to make via a completely generated make file.
        mk = Makefile()
        mk.add_statement('nonstatic_webidl_files := %s' % ' '.join(
            sorted(webidls.all_non_static_basenames())))
        mk.add_statement('globalgen_sources := %s' % ' '.join(
            sorted(manager.GLOBAL_DEFINE_FILES)))
        mk.add_statement('test_sources := %s' % ' '.join(
            sorted('%sBinding.cpp' % s for s in webidls.all_test_stems())))

        # Add rules to preprocess bindings.
        # This should ideally be using PP_TARGETS. However, since the input
        # filenames match the output filenames, the existing PP_TARGETS rules
        # result in circular dependencies and other make weirdness. One
        # solution is to rename the input or output files repsectively. See
        # bug 928195 comment 129.
        for source in sorted(webidls.all_preprocessed_sources()):
            basename = os.path.basename(source)
            rule = mk.create_rule([basename])
            rule.add_dependencies([source, '$(GLOBAL_DEPS)'])
            rule.add_commands([
                # Remove the file before writing so bindings that go from
                # static to preprocessed don't end up writing to a symlink,
                # which would modify content in the source directory.
                '$(RM) $@',
                '$(call py_action,preprocessor,$(DEFINES) $(ACDEFINES) '
                    '$(XULPPFLAGS) $< -o $@)'
            ])

        # Bindings are compiled in unified mode to speed up compilation and
        # to reduce linker memory size. Note that test bindings are separated
        # from regular ones so tests bindings aren't shipped.
        self._add_unified_build_rules(mk,
            webidls.all_regular_cpp_basenames(),
            bindings_dir,
            unified_prefix='UnifiedBindings',
            unified_files_makefile_variable='unified_binding_cpp_files',
            poison_windows_h=True)

        webidls_mk = mozpath.join(bindings_dir, 'webidlsrcs.mk')
        with self._write_file(webidls_mk) as fh:
            mk.dump(fh, removal_guard=False)
