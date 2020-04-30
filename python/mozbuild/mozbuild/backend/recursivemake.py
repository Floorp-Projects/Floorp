# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import io
import logging
import os
import re
import six

from collections import (
    defaultdict,
    namedtuple,
)
from itertools import chain
from operator import itemgetter
from six import StringIO

from mozpack.manifests import (
    InstallManifest,
)
import mozpack.path as mozpath

from mozbuild import frontend
from mozbuild.frontend.context import (
    AbsolutePath,
    Path,
    RenamedSourcePath,
    SourcePath,
    ObjDirPath,
)
from .common import CommonBackend
from .make import MakeBackend
from ..frontend.data import (
    BaseLibrary,
    BaseProgram,
    BaseRustLibrary,
    ChromeManifestEntry,
    ComputedFlags,
    ConfigFileSubstitution,
    ContextDerived,
    Defines,
    DirectoryTraversal,
    ExternalLibrary,
    FinalTargetFiles,
    FinalTargetPreprocessedFiles,
    GeneratedFile,
    GeneratedSources,
    HostDefines,
    HostGeneratedSources,
    HostLibrary,
    HostProgram,
    HostRustProgram,
    HostSimpleProgram,
    HostSources,
    InstallationTarget,
    JARManifest,
    Linkable,
    LocalInclude,
    LocalizedFiles,
    LocalizedPreprocessedFiles,
    ObjdirFiles,
    ObjdirPreprocessedFiles,
    PerSourceFlag,
    Program,
    HostSharedLibrary,
    RustProgram,
    RustTests,
    SandboxedWasmLibrary,
    SharedLibrary,
    SimpleProgram,
    Sources,
    StaticLibrary,
    TestManifest,
    VariablePassthru,
    WasmGeneratedSources,
    WasmSources,
    XPIDLModule,
)
from ..util import (
    ensureParentDir,
    FileAvoidWrite,
    OrderedDefaultDict,
    pairwise,
)
from ..makeutil import Makefile
from mozbuild.shellutil import quote as shell_quote

# To protect against accidentally adding logic to Makefiles that belong in moz.build,
# we check if moz.build-like variables are defined in Makefiles. If they are, we throw
# an error to encourage the usage of moz.build instead.
_MOZBUILD_ONLY_VARIABLES = set(frontend.context.VARIABLES.keys()) - {
    # The migration to moz.build from Makefiles still isn't complete, and there's still
    # some straggling Makefile logic that uses variables that only moz.build should
    # use.
    # These remaining variables are excluded from our blacklist. As the variables here
    # are migrated from Makefiles in the future, they should be removed from this
    # "override" list.
    'XPI_NAME',
    'USE_EXTENSION_MANIFEST',
    'CFLAGS',
    'CXXFLAGS',
}

DEPRECATED_VARIABLES = [
    'ALLOW_COMPILER_WARNINGS',
    'EXPORT_LIBRARY',
    'EXTRA_LIBS',
    'FAIL_ON_WARNINGS',
    'HOST_LIBS',
    'LIBXUL_LIBRARY',
    'MOCHITEST_A11Y_FILES',
    'MOCHITEST_BROWSER_FILES',
    'MOCHITEST_BROWSER_FILES_PARTS',
    'MOCHITEST_CHROME_FILES',
    'MOCHITEST_FILES',
    'MOCHITEST_FILES_PARTS',
    'MOCHITEST_METRO_FILES',
    'MOCHITEST_ROBOCOP_FILES',
    'MODULE_OPTIMIZE_FLAGS',
    'MOZ_CHROME_FILE_FORMAT',
    'SHORT_LIBNAME',
    'TESTING_JS_MODULES',
    'TESTING_JS_MODULE_DIR',
]

MOZBUILD_VARIABLES_MESSAGE = 'It should only be defined in moz.build files.'

DEPRECATED_VARIABLES_MESSAGE = (
    'This variable has been deprecated. It does nothing. It must be removed '
    'in order to build.'
)


def make_quote(s):
    return s.replace('#', '\#').replace('$', '$$')


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

    def __init__(self, srcdir, objdir, environment, topsrcdir, topobjdir, dry_run):
        self.topsrcdir = topsrcdir
        self.srcdir = srcdir
        self.objdir = objdir
        self.relobjdir = mozpath.relpath(objdir, topobjdir)
        self.environment = environment
        self.name = mozpath.join(objdir, 'backend.mk')

        self.xpt_name = None

        self.fh = FileAvoidWrite(self.name, capture_diff=True, dry_run=dry_run)
        self.fh.write('# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT EDIT.\n')
        self.fh.write('\n')

    def write(self, buf):
        self.fh.write(buf)

    def write_once(self, buf):
        buf = six.ensure_text(buf)
        if '\n' + buf not in six.ensure_text(self.fh.getvalue()):
            self.write(buf)

    # For compatibility with makeutil.Makefile
    def add_statement(self, stmt):
        self.write('%s\n' % stmt)

    def close(self):
        if self.xpt_name:
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
        - (normal) dirs
        - tests
    """
    SubDirectoryCategories = ['dirs', 'tests']
    SubDirectoriesTuple = namedtuple('SubDirectories', SubDirectoryCategories)

    class SubDirectories(SubDirectoriesTuple):
        def __new__(self):
            return RecursiveMakeTraversal.SubDirectoriesTuple.__new__(self, [], [])

    def __init__(self):
        self._traversal = {}
        self._attached = set()

    def add(self, dir, dirs=[], tests=[]):
        """
        Adds a directory to traversal, registering its subdirectories,
        sorted by categories. If the directory was already added to
        traversal, adds the new subdirectories to the already known lists.
        """
        subdirs = self._traversal.setdefault(dir, self.SubDirectories())
        for key, value in (('dirs', dirs), ('tests', tests)):
            assert(key in self.SubDirectoryCategories)
            # Callers give us generators
            value = list(value)
            getattr(subdirs, key).extend(value)
            self._attached |= set(value)

    @staticmethod
    def default_filter(current, subdirs):
        """
        Default filter for use with compute_dependencies and traverse.
        """
        return current, [], subdirs.dirs + subdirs.tests

    def call_filter(self, current, filter):
        """
        Helper function to call a filter from compute_dependencies and
        traverse.
        """
        return filter(current, self.get_subdirs(current))

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
            if start_node not in self._traversal:
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
        if start not in self._traversal:
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
        result = self._traversal.get(dir, self.SubDirectories())
        if dir == '':
            unattached = set(self._traversal) - self._attached - set([''])
            if unattached:
                new_result = self.SubDirectories()
                new_result.dirs.extend(result.dirs)
                new_result.dirs.extend(sorted(unattached))
                new_result.tests.extend(result.tests)
                result = new_result
        return result


class RecursiveMakeBackend(MakeBackend):
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
        MakeBackend._init(self)

        self._backend_files = {}
        self._idl_dirs = set()

        self._makefile_in_count = 0
        self._makefile_out_count = 0

        self._test_manifests = {}

        self.backend_input_files.add(mozpath.join(self.environment.topobjdir,
                                                  'config', 'autoconf.mk'))

        self._install_manifests = defaultdict(InstallManifest)
        # The build system relies on some install manifests always existing
        # even if they are empty, because the directories are still filled
        # by the build system itself, and the install manifests are only
        # used for a "magic" rm -rf.
        self._install_manifests['dist_public']
        self._install_manifests['dist_private']

        self._traversal = RecursiveMakeTraversal()
        self._compile_graph = OrderedDefaultDict(set)
        self._rust_targets = set()
        self._rust_lib_targets = set()
        self._gkrust_target = None

        self._no_skip = {
            'export': set(),
            'libs': set(),
            'misc': set(),
            'tools': set(),
            'check': set(),
            'syms': set(),
        }

    def summary(self):
        summary = super(RecursiveMakeBackend, self).summary()
        summary.extend('; {makefile_in:d} -> {makefile_out:d} Makefile',
                       makefile_in=self._makefile_in_count,
                       makefile_out=self._makefile_out_count)
        return summary

    def _get_backend_file_for(self, obj):
        if obj.objdir not in self._backend_files:
            self._backend_files[obj.objdir] = \
                BackendMakeFile(obj.srcdir, obj.objdir, obj.config,
                                obj.topsrcdir, self.environment.topobjdir, self.dry_run)
        return self._backend_files[obj.objdir]

    def consume_object(self, obj):
        """Write out build files necessary to build with recursive make."""

        if not isinstance(obj, ContextDerived):
            return False

        backend_file = self._get_backend_file_for(obj)

        consumed = CommonBackend.consume_object(self, obj)

        # CommonBackend handles XPIDLModule, but we want to do
        # some extra things for them.
        if isinstance(obj, XPIDLModule):
            backend_file.xpt_name = '%s.xpt' % obj.name
            self._idl_dirs.add(obj.relobjdir)

        # If CommonBackend acknowledged the object, we're done with it.
        if consumed:
            return True

        if not isinstance(obj, Defines):
            self.consume_object(obj.defines)

        if isinstance(obj, Linkable):
            self._process_test_support_file(obj)

        if isinstance(obj, DirectoryTraversal):
            self._process_directory_traversal(obj, backend_file)
        elif isinstance(obj, ConfigFileSubstitution):
            # Other ConfigFileSubstitution should have been acked by
            # CommonBackend.
            assert os.path.basename(obj.output_path) == 'Makefile'
            self._create_makefile(obj)
        elif isinstance(obj, (Sources, GeneratedSources)):
            suffix_map = {
                '.s': 'ASFILES',
                '.c': 'CSRCS',
                '.m': 'CMSRCS',
                '.mm': 'CMMSRCS',
                '.cpp': 'CPPSRCS',
                '.S': 'SSRCS',
            }
            variables = [suffix_map[obj.canonical_suffix]]
            if isinstance(obj, GeneratedSources):
                variables.append('GARBAGE')
                base = backend_file.objdir
                cls = ObjDirPath
                prefix = '!'
            else:
                base = backend_file.srcdir
                cls = SourcePath
                prefix = ''
            for f in sorted(obj.files):
                p = self._pretty_path(
                    cls(obj._context, prefix + mozpath.relpath(f, base)),
                    backend_file,
                )
                for var in variables:
                    backend_file.write('%s += %s\n' % (var, p))
            self._compile_graph[mozpath.join(
                backend_file.relobjdir, 'target-objects')]
        elif isinstance(obj, (HostSources, HostGeneratedSources)):
            suffix_map = {
                '.c': 'HOST_CSRCS',
                '.mm': 'HOST_CMMSRCS',
                '.cpp': 'HOST_CPPSRCS',
            }
            variables = [suffix_map[obj.canonical_suffix]]
            if isinstance(obj, HostGeneratedSources):
                variables.append('GARBAGE')
                base = backend_file.objdir
                cls = ObjDirPath
                prefix = '!'
            else:
                base = backend_file.srcdir
                cls = SourcePath
                prefix = ''
            for f in sorted(obj.files):
                p = self._pretty_path(
                    cls(obj._context, prefix + mozpath.relpath(f, base)),
                    backend_file,
                )
                for var in variables:
                    backend_file.write('%s += %s\n' % (var, p))
            self._compile_graph[mozpath.join(
                backend_file.relobjdir, 'host-objects')]
        elif isinstance(obj, (WasmSources, WasmGeneratedSources)):
            suffix_map = {
                '.c': 'WASM_CSRCS',
                '.cpp': 'WASM_CPPSRCS',
            }
            variables = [suffix_map[obj.canonical_suffix]]
            if isinstance(obj, WasmGeneratedSources):
                variables.append('GARBAGE')
                base = backend_file.objdir
                cls = ObjDirPath
                prefix = '!'
            else:
                base = backend_file.srcdir
                cls = SourcePath
                prefix = ''
            for f in sorted(obj.files):
                p = self._pretty_path(
                    cls(obj._context, prefix + mozpath.relpath(f, base)),
                    backend_file,
                )
                for var in variables:
                    backend_file.write('%s += %s\n' % (var, p))
            self._compile_graph[mozpath.join(
                backend_file.relobjdir, 'target-objects')]
        elif isinstance(obj, VariablePassthru):
            # Sorted so output is consistent and we don't bump mtimes.
            for k, v in sorted(obj.variables.items()):
                if isinstance(v, list):
                    for item in v:
                        backend_file.write(
                            '%s += %s\n' % (k, make_quote(shell_quote(item))))
                elif isinstance(v, bool):
                    if v:
                        backend_file.write('%s := 1\n' % k)
                elif isinstance(v, Path):
                    path = self._pretty_path(Path(obj._context, v), backend_file)
                    backend_file.write('%s := %s\n' % (k, path))
                else:
                    backend_file.write('%s := %s\n' % (k, v))
        elif isinstance(obj, HostDefines):
            self._process_defines(obj, backend_file, which='HOST_DEFINES')
        elif isinstance(obj, Defines):
            self._process_defines(obj, backend_file)

        elif isinstance(obj, GeneratedFile):
            if obj.required_before_compile:
                tier = 'export'
            elif obj.required_during_compile:
                tier = None
            elif obj.localized:
                tier = 'libs'
            else:
                tier = 'misc'
            if tier:
                self._no_skip[tier].add(backend_file.relobjdir)
            backend_file.write_once('include $(topsrcdir)/config/AB_rCD.mk\n')
            for stmt in self._format_statements_for_generated_file(
                    obj, tier,
                    extra_dependencies='backend.mk' if obj.flags else ''):
                backend_file.write(stmt + '\n')

        elif isinstance(obj, JARManifest):
            self._no_skip['libs'].add(backend_file.relobjdir)
            backend_file.write('JAR_MANIFEST := %s\n' % obj.path.full_path)

        elif isinstance(obj, RustProgram):
            self._process_rust_program(obj, backend_file)
            # Hook the program into the compile graph.
            build_target = self._build_target_for_obj(obj)
            self._compile_graph[build_target]
            self._rust_targets.add(build_target)

        elif isinstance(obj, HostRustProgram):
            self._process_host_rust_program(obj, backend_file)
            # Hook the program into the compile graph.
            build_target = self._build_target_for_obj(obj)
            self._compile_graph[build_target]
            self._rust_targets.add(build_target)

        elif isinstance(obj, RustTests):
            self._process_rust_tests(obj, backend_file)

        elif isinstance(obj, Program):
            self._process_program(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)
            self._no_skip['syms'].add(backend_file.relobjdir)

        elif isinstance(obj, HostProgram):
            self._process_host_program(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, SimpleProgram):
            self._process_simple_program(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)
            self._no_skip['syms'].add(backend_file.relobjdir)

        elif isinstance(obj, HostSimpleProgram):
            self._process_host_simple_program(obj.program, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, LocalInclude):
            self._process_local_include(obj.path, backend_file)

        elif isinstance(obj, PerSourceFlag):
            self._process_per_source_flag(obj, backend_file)

        elif isinstance(obj, ComputedFlags):
            self._process_computed_flags(obj, backend_file)

        elif isinstance(obj, InstallationTarget):
            self._process_installation_target(obj, backend_file)

        elif isinstance(obj, BaseRustLibrary):
            self.backend_input_files.add(obj.cargo_file)
            self._process_rust_library(obj, backend_file)
            # No need to call _process_linked_libraries, because Rust
            # libraries are self-contained objects at this point.

            # Hook the library into the compile graph.
            build_target = self._build_target_for_obj(obj)
            self._compile_graph[build_target]
            self._rust_targets.add(build_target)
            self._rust_lib_targets.add(build_target)
            if obj.is_gkrust:
                self._gkrust_target = build_target

        elif isinstance(obj, SharedLibrary):
            self._process_shared_library(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)
            self._no_skip['syms'].add(backend_file.relobjdir)

        elif isinstance(obj, StaticLibrary):
            self._process_static_library(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, SandboxedWasmLibrary):
            self._process_sandboxed_wasm_library(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, HostLibrary):
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, HostSharedLibrary):
            self._process_host_shared_library(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, ObjdirFiles):
            self._process_objdir_files(obj, obj.files, backend_file)

        elif isinstance(obj, ObjdirPreprocessedFiles):
            self._process_final_target_pp_files(obj, obj.files, backend_file, 'OBJDIR_PP_FILES')

        elif isinstance(obj, LocalizedFiles):
            self._process_localized_files(obj, obj.files, backend_file)

        elif isinstance(obj, LocalizedPreprocessedFiles):
            self._process_localized_pp_files(obj, obj.files, backend_file)

        elif isinstance(obj, FinalTargetFiles):
            self._process_final_target_files(obj, obj.files, backend_file)

        elif isinstance(obj, FinalTargetPreprocessedFiles):
            self._process_final_target_pp_files(obj, obj.files, backend_file, 'DIST_FILES')

        elif isinstance(obj, ChromeManifestEntry):
            self._process_chrome_manifest_entry(obj, backend_file)

        elif isinstance(obj, TestManifest):
            self._process_test_manifest(obj, backend_file)

        else:
            return False

        return True

    def _fill_root_mk(self):
        """
        Create two files, root.mk and root-deps.mk, the first containing
        convenience variables, and the other dependency definitions for a
        hopefully proper directory traversal.
        """
        for tier, no_skip in self._no_skip.items():
            self.log(logging.DEBUG, 'fill_root_mk', {
                'number': len(no_skip), 'tier': tier
                }, 'Using {number} directories during {tier}')

        def should_skip(tier, dir):
            if tier in self._no_skip:
                return dir not in self._no_skip[tier]
            return False

        # Traverse directories in parallel, and skip static dirs
        def parallel_filter(current, subdirs):
            all_subdirs = subdirs.dirs + subdirs.tests
            if should_skip(tier, current) or current.startswith('subtiers/'):
                current = None
            return current, all_subdirs, []

        # build everything in parallel, including static dirs
        # Because of bug 925236 and possible other unknown race conditions,
        # don't parallelize the libs tier.
        def libs_filter(current, subdirs):
            if should_skip('libs', current) or current.startswith('subtiers/'):
                current = None
            return current, [], subdirs.dirs + subdirs.tests

        # Because of bug 925236 and possible other unknown race conditions,
        # don't parallelize the tools tier. There aren't many directories for
        # this tier anyways.
        def tools_filter(current, subdirs):
            if should_skip('tools', current) or current.startswith('subtiers/'):
                current = None
            return current, [], subdirs.dirs + subdirs.tests

        filters = [
            ('export', parallel_filter),
            ('libs', libs_filter),
            ('misc', parallel_filter),
            ('tools', tools_filter),
            ('check', parallel_filter),
        ]

        root_deps_mk = Makefile()

        # Fill the dependencies for traversal of each tier.
        for tier, filter in sorted(filters, key=itemgetter(0)):
            main, all_deps = \
                self._traversal.compute_dependencies(filter)
            for dir, deps in sorted(all_deps.items()):
                if deps is not None or (dir in self._idl_dirs
                                        and tier == 'export'):
                    rule = root_deps_mk.create_rule(['%s/%s' % (dir, tier)])
                    if deps:
                        rule.add_dependencies(
                            '%s/%s' % (d, tier) for d in sorted(deps) if d)
                    if dir in self._idl_dirs and tier == 'export':
                        rule.add_dependencies(['xpcom/xpidl/%s' % tier])
            rule = root_deps_mk.create_rule(['recurse_%s' % tier])
            if main:
                rule.add_dependencies('%s/%s' % (d, tier) for d in sorted(main))

        all_compile_deps = six.moves.reduce(
            lambda x, y: x | y,
            self._compile_graph.values()) if self._compile_graph else set()
        # Include the following as dependencies of the top recursion target for
        # compilation:
        # - nodes that are not dependended upon by anything. Typically, this
        #   would include programs, that need to be recursed, but that nothing
        #   depends on.
        # - nodes that have no dependencies of their own. Technically, this is
        #   not necessary, because other things have dependencies on them, and
        #   they all end up rooting to nodes from the above category. But the
        #   way make works[1] is such that there can be benefits listing them
        #   as direct dependencies of the top recursion target, to somehow
        #   prioritize them.
        #   1. See bug 1262241 comment 5.
        compile_roots = [t for t, deps in six.iteritems(self._compile_graph)
                         if not deps or t not in all_compile_deps]

        def add_category_rules(category, roots, graph):
            rule = root_deps_mk.create_rule(['recurse_%s' % category])
            # Directories containing rust compilations don't generally depend
            # on other directories in the tree, so putting them first here will
            # start them earlier in the build.
            rust_roots = sorted(r for r in roots if r in self._rust_targets)
            rust_libs = sorted(r for r in roots if r in self._rust_lib_targets)
            if category == 'compile' and rust_roots:
                rust_rule = root_deps_mk.create_rule(['recurse_rust'])
                rust_rule.add_dependencies(rust_roots)
                # Ensure our cargo invocations are serialized, and gecko comes
                # first. Cargo will lock on the build output directory anyway,
                # so trying to run things in parallel is not useful. Dependencies
                # for gecko are especially expensive to build and parallelize
                # poorly, so prioritizing these will save some idle time in full
                # builds.
                for prior_target, target in pairwise(
                        sorted([t for t in rust_libs],
                               key=lambda t: t != self._gkrust_target)):
                    r = root_deps_mk.create_rule([target])
                    r.add_dependencies([prior_target])

            rule.add_dependencies(chain(rust_roots, roots))
            for target, deps in sorted(graph.items()):
                if deps:
                    rule = root_deps_mk.create_rule([target])
                    rule.add_dependencies(sorted(deps))

        non_default_roots = defaultdict(list)
        non_default_graphs = defaultdict(lambda: OrderedDefaultDict(set))

        for root in compile_roots:
            # If this is a non-default target, separate the root from the
            # rest of the compile graph.
            target_name = mozpath.basename(root)

            if target_name not in ('target', 'target-objects',
                                   'host', 'host-objects'):
                non_default_roots[target_name].append(root)
                non_default_graphs[target_name][root] = self._compile_graph[root]
                del self._compile_graph[root]

        for root in chain(*non_default_roots.values()):
            compile_roots.remove(root)
            dirname = mozpath.dirname(root)
            # If a directory only contains non-default compile targets, we don't
            # attempt to dump symbols there.
            if (dirname in self._no_skip['syms'] and
                '%s/target' % dirname not in self._compile_graph):
                self._no_skip['syms'].remove(dirname)

        add_category_rules('compile', compile_roots, self._compile_graph)
        for category, graph in sorted(six.iteritems(non_default_graphs)):
            add_category_rules(category, non_default_roots[category], graph)

        root_mk = Makefile()

        # Fill root.mk with the convenience variables.
        for tier, filter in filters:
            all_dirs = self._traversal.traverse('', filter)
            root_mk.add_statement('%s_dirs := %s' % (tier, ' '.join(all_dirs)))

        # Need a list of compile targets because we can't use pattern rules:
        # https://savannah.gnu.org/bugs/index.php?42833
        root_mk.add_statement('compile_targets := %s' % ' '.join(sorted(
            set(self._compile_graph.keys()) | all_compile_deps)))
        root_mk.add_statement('syms_targets := %s' % ' '.join(sorted(
            set('%s/syms' % d for d in self._no_skip['syms']))))
        root_mk.add_statement('rust_targets := %s' % ' '.join(sorted(
            self._rust_targets)))

        root_mk.add_statement('non_default_tiers := %s' % ' '.join(sorted(
            non_default_roots.keys())))

        for category, graphs in sorted(six.iteritems(non_default_graphs)):
            category_dirs = [mozpath.dirname(target)
                             for target in graphs.keys()]
            root_mk.add_statement('%s_dirs := %s' % (category,
                                                     ' '.join(category_dirs)))

        root_mk.add_statement('include root-deps.mk')

        with self._write_file(
                mozpath.join(self.environment.topobjdir, 'root.mk')) as root:
            root_mk.dump(root, removal_guard=False)

        with self._write_file(
                mozpath.join(self.environment.topobjdir, 'root-deps.mk')) as root_deps:
            root_deps_mk.dump(root_deps, removal_guard=False)

    def _add_unified_build_rules(self, makefile, unified_source_mapping,
                                 unified_files_makefile_variable='unified_files',
                                 include_curdir_build_rules=True):

        # In case it's a generator.
        unified_source_mapping = sorted(unified_source_mapping)

        explanation = "\n" \
            "# We build files in 'unified' mode by including several files\n" \
            "# together into a single source file.  This cuts down on\n" \
            "# compilation times and debug information size."
        makefile.add_statement(explanation)

        all_sources = ' '.join(source for source, _ in unified_source_mapping)
        makefile.add_statement('%s := %s' % (unified_files_makefile_variable,
                                             all_sources))

        if include_curdir_build_rules:
            makefile.add_statement(
                '\n'
                '# Make sometimes gets confused between "foo" and "$(CURDIR)/foo".\n'
                '# Help it out by explicitly specifiying dependencies.')
            makefile.add_statement('all_absolute_unified_files := \\\n'
                                   '  $(addprefix $(CURDIR)/,$(%s))'
                                   % unified_files_makefile_variable)
            rule = makefile.create_rule(['$(all_absolute_unified_files)'])
            rule.add_dependencies(['$(CURDIR)/%: %'])

    def _check_blacklisted_variables(self, makefile_in, makefile_content):
        if 'EXTERNALLY_MANAGED_MAKE_FILE' in makefile_content:
            # Bypass the variable restrictions for externally managed makefiles.
            return

        for l in makefile_content.splitlines():
            l = l.strip()
            # Don't check comments
            if l.startswith('#'):
                continue
            for x in chain(_MOZBUILD_ONLY_VARIABLES, DEPRECATED_VARIABLES):
                if x not in l:
                    continue

                # Finding the variable name in the Makefile is not enough: it
                # may just appear as part of something else, like DIRS appears
                # in GENERATED_DIRS.
                if re.search(r'\b%s\s*[:?+]?=' % x, l):
                    if x in _MOZBUILD_ONLY_VARIABLES:
                        message = MOZBUILD_VARIABLES_MESSAGE
                    else:
                        message = DEPRECATED_VARIABLES_MESSAGE
                    raise Exception('Variable %s is defined in %s. %s'
                                    % (x, makefile_in, message))

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
                    self._makefile_in_count += 1

                    # In the export and libs tiers, we don't skip directories
                    # containing a Makefile.in.
                    # topobjdir is handled separatedly, don't do anything for
                    # it.
                    if bf.relobjdir:
                        for tier in ('export', 'libs',):
                            self._no_skip[tier].add(bf.relobjdir)
                else:
                    self.log(logging.DEBUG, 'stub_makefile',
                             {'path': makefile}, 'Creating stub Makefile: {path}')

                obj = self.Substitution()
                obj.output_path = makefile
                obj.input_path = makefile_in
                obj.topsrcdir = backend_file.topsrcdir
                obj.topobjdir = bf.environment.topobjdir
                obj.config = bf.environment
                self._create_makefile(obj, stub=stub)
                with io.open(obj.output_path, encoding='utf-8') as fh:
                    content = fh.read()
                    # Directories with a Makefile containing a tools target, or
                    # XPI_PKGNAME or INSTALL_EXTENSION_ID can't be skipped and
                    # must run during the 'tools' tier.
                    for t in ('XPI_PKGNAME', 'INSTALL_EXTENSION_ID', 'tools'):
                        if t not in content:
                            continue
                        if t == 'tools' and not re.search('(?:^|\s)tools.*::', content, re.M):
                            continue
                        if objdir == self.environment.topobjdir:
                            continue
                        self._no_skip['tools'].add(mozpath.relpath(objdir,
                                                                   self.environment.topobjdir))

                    # Directories with a Makefile containing a check target
                    # can't be skipped and must run during the 'check' tier.
                    if re.search('(?:^|\s)check.*::', content, re.M):
                        self._no_skip['check'].add(mozpath.relpath(objdir,
                                                                   self.environment.topobjdir))

                    # Detect any Makefile.ins that contain variables on the
                    # moz.build-only list
                    self._check_blacklisted_variables(makefile_in, content)

        self._fill_root_mk()

        # Make the master test manifest files.
        for flavor, t in self._test_manifests.items():
            install_prefix, manifests = t
            manifest_stem = mozpath.join(install_prefix, '%s.ini' % flavor)
            self._write_master_test_manifest(mozpath.join(
                self.environment.topobjdir, '_tests', manifest_stem),
                manifests)

            # Catch duplicate inserts.
            try:
                self._install_manifests['_tests'].add_optional_exists(manifest_stem)
            except ValueError:
                pass

        self._write_manifests('install', self._install_manifests)

        ensureParentDir(mozpath.join(self.environment.topobjdir, 'dist', 'foo'))

    def _pretty_path_parts(self, path, backend_file):
        assert isinstance(path, Path)
        if isinstance(path, SourcePath):
            if path.full_path.startswith(backend_file.srcdir):
                return '$(srcdir)', path.full_path[len(backend_file.srcdir):]
            if path.full_path.startswith(backend_file.topsrcdir):
                return '$(topsrcdir)', path.full_path[len(backend_file.topsrcdir):]
        elif isinstance(path, ObjDirPath):
            if path.full_path.startswith(backend_file.objdir):
                return '', path.full_path[len(backend_file.objdir) + 1:]
            if path.full_path.startswith(self.environment.topobjdir):
                return '$(DEPTH)', path.full_path[len(self.environment.topobjdir):]

        return '', path.full_path

    def _pretty_path(self, path, backend_file):
        return ''.join(self._pretty_path_parts(path, backend_file))

    def _process_unified_sources(self, obj):
        backend_file = self._get_backend_file_for(obj)

        suffix_map = {
            '.c': 'UNIFIED_CSRCS',
            '.mm': 'UNIFIED_CMMSRCS',
            '.cpp': 'UNIFIED_CPPSRCS',
        }

        var = suffix_map[obj.canonical_suffix]
        non_unified_var = var[len('UNIFIED_'):]

        if obj.have_unified_mapping:
            self._add_unified_build_rules(backend_file,
                                          obj.unified_source_mapping,
                                          unified_files_makefile_variable=var,
                                          include_curdir_build_rules=False)
            backend_file.write('%s += $(%s)\n' % (non_unified_var, var))
        else:
            # Sorted so output is consistent and we don't bump mtimes.
            source_files = list(sorted(obj.files))

            backend_file.write('%s += %s\n' % (
                    non_unified_var, ' '.join(source_files)))

        self._compile_graph[mozpath.join(
            backend_file.relobjdir, 'target-objects')]

    def _process_directory_traversal(self, obj, backend_file):
        """Process a data.DirectoryTraversal instance."""
        fh = backend_file.fh

        def relativize(base, dirs):
            return (mozpath.relpath(d.translated, base) for d in dirs)

        if obj.dirs:
            fh.write('DIRS := %s\n' % ' '.join(relativize(backend_file.objdir, obj.dirs)))
            self._traversal.add(
                backend_file.relobjdir, dirs=relativize(self.environment.topobjdir, obj.dirs)
            )

        # The directory needs to be registered whether subdirectories have been
        # registered or not.
        self._traversal.add(backend_file.relobjdir)

    def _process_defines(self, obj, backend_file, which='DEFINES'):
        """Output the DEFINES rules to the given backend file."""
        defines = list(obj.get_defines())
        if defines:
            defines = ' '.join(shell_quote(d) for d in defines)
            backend_file.write_once('%s += %s\n' % (which, defines))

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
            backend_file.write(
                'FINAL_TARGET = $(if $(XPI_NAME),$(DIST)/xpi-stage/$(XPI_NAME),'
                '$(DIST)/bin)$(DIST_SUBDIR:%=/%)\n'
                )

        if not obj.enabled:
            backend_file.write('NO_DIST_INSTALL := 1\n')

    def _handle_idl_manager(self, manager):
        build_files = self._install_manifests['xpidl']

        for p in ('Makefile', 'backend.mk', '.deps/.mkdir.done'):
            build_files.add_optional_exists(p)

        for stem in manager.idl_stems():
            self._install_manifests['dist_include'].add_optional_exists('%s.h' % stem)

        for module in manager.modules:
            build_files.add_optional_exists(mozpath.join('.deps', '%s.pp' % module))

        modules = manager.modules
        xpt_modules = sorted(modules.keys())

        mk = Makefile()
        all_directories = set()

        for module_name in xpt_modules:
            module = manager.modules[module_name]
            all_directories |= module.directories
            deps = sorted(module.idl_files)

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

            mk.add_statement('%s_deps := %s' % (module_name, ' '.join(deps)))

            build_files.add_optional_exists('%s.xpt' % module_name)

        mk.add_statement('all_idl_dirs := %s' % ' '.join(sorted(all_directories)))

        rules = StringIO()
        mk.dump(rules, removal_guard=False)

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
            xpidl_rules=rules.getvalue(),
            xpidl_modules=' '.join(xpt_modules),
        ))

    def _process_program(self, obj, backend_file):
        backend_file.write('PROGRAM = %s\n' % self._pretty_path(obj.output_path, backend_file))
        if not obj.cxx_link and not self.environment.bin_suffix:
            backend_file.write('PROG_IS_C_ONLY_%s := 1\n' % obj.program)

    def _process_host_program(self, program, backend_file):
        backend_file.write('HOST_PROGRAM = %s\n' %
                           self._pretty_path(program.output_path, backend_file))

    def _process_rust_program_base(self, obj, backend_file,
                                   target_variable,
                                   target_cargo_variable):
        backend_file.write_once('CARGO_FILE := %s\n' % obj.cargo_file)
        backend_file.write_once('CARGO_TARGET_DIR := .\n')
        backend_file.write('%s += %s\n' % (target_variable, obj.location))
        backend_file.write('%s += %s\n' % (target_cargo_variable, obj.name))

    def _process_rust_program(self, obj, backend_file):
        self._process_rust_program_base(obj, backend_file,
                                        'RUST_PROGRAMS',
                                        'RUST_CARGO_PROGRAMS')

    def _process_host_rust_program(self, obj, backend_file):
        self._process_rust_program_base(obj, backend_file,
                                        'HOST_RUST_PROGRAMS',
                                        'HOST_RUST_CARGO_PROGRAMS')

    def _process_rust_tests(self, obj, backend_file):
        if obj.config.substs.get('MOZ_RUST_TESTS'):
            # If --enable-rust-tests has been set, run these as a part of
            # make check.
            self._no_skip['check'].add(backend_file.relobjdir)
            backend_file.write('check:: force-cargo-test-run\n')
        build_target = self._build_target_for_obj(obj)
        self._compile_graph[build_target]
        self._process_non_default_target(obj, 'force-cargo-test-run',
                                         backend_file)
        backend_file.write_once('CARGO_FILE := $(srcdir)/Cargo.toml\n')
        backend_file.write_once('RUST_TESTS := %s\n' % ' '.join(obj.names))
        backend_file.write_once('RUST_TEST_FEATURES := %s\n' % ' '.join(obj.features))

    def _process_simple_program(self, obj, backend_file):
        if obj.is_unit_test:
            backend_file.write('CPP_UNIT_TESTS += %s\n' % obj.program)
            assert obj.cxx_link
        else:
            backend_file.write('SIMPLE_PROGRAMS += %s\n' % obj.program)
            if not obj.cxx_link and not self.environment.bin_suffix:
                backend_file.write('PROG_IS_C_ONLY_%s := 1\n' % obj.program)

    def _process_host_simple_program(self, program, backend_file):
        backend_file.write('HOST_SIMPLE_PROGRAMS += %s\n' % program)

    def _process_test_support_file(self, obj):
        # Ensure test support programs and libraries are tracked by an
        # install manifest for the benefit of the test packager.
        if not obj.install_target.startswith('_tests'):
            return

        dest_basename = None
        if isinstance(obj, BaseLibrary):
            dest_basename = obj.lib_name
        elif isinstance(obj, BaseProgram):
            dest_basename = obj.program
        if dest_basename is None:
            return

        self._install_manifests['_tests'].add_optional_exists(
            mozpath.join(obj.install_target[len('_tests') + 1:],
                         dest_basename))

    def _process_test_manifest(self, obj, backend_file):
        # Much of the logic in this function could be moved to CommonBackend.
        for source in obj.source_relpaths:
            self.backend_input_files.add(mozpath.join(obj.topsrcdir,
                                                      source))

        # Don't allow files to be defined multiple times unless it is allowed.
        # We currently allow duplicates for non-test files or test files if
        # the manifest is listed as a duplicate.
        for source, (dest, is_test) in obj.installs.items():
            try:
                self._install_manifests['_test_files'].add_link(source, dest)
            except ValueError:
                if not obj.dupe_manifest and is_test:
                    raise

        for base, pattern, dest in obj.pattern_installs:
            try:
                self._install_manifests['_test_files'].add_pattern_link(base,
                                                                        pattern, dest)
            except ValueError:
                if not obj.dupe_manifest:
                    raise

        for dest in obj.external_installs:
            try:
                self._install_manifests['_test_files'].add_optional_exists(dest)
            except ValueError:
                if not obj.dupe_manifest:
                    raise

        m = self._test_manifests.setdefault(obj.flavor,
                                            (obj.install_prefix, set()))
        m[1].add(obj.manifest_obj_relpath)

        try:
            from reftest import ReftestManifest

            if isinstance(obj.manifest, ReftestManifest):
                # Mark included files as part of the build backend so changes
                # result in re-config.
                self.backend_input_files |= obj.manifest.manifests
        except ImportError:
            # Ignore errors caused by the reftest module not being present.
            # This can happen when building SpiderMonkey standalone, for example.
            pass

    def _process_local_include(self, local_include, backend_file):
        d, path = self._pretty_path_parts(local_include, backend_file)
        if isinstance(local_include, ObjDirPath) and not d:
            # path doesn't start with a slash in this case
            d = '$(CURDIR)/'
        elif d == '$(DEPTH)':
            d = '$(topobjdir)'
        quoted_path = shell_quote(path) if path else path
        if quoted_path != path:
            path = quoted_path[0] + d + quoted_path[1:]
        else:
            path = d + path
        backend_file.write('LOCAL_INCLUDES += -I%s\n' % path)

    def _process_per_source_flag(self, per_source_flag, backend_file):
        for flag in per_source_flag.flags:
            backend_file.write('%s_FLAGS += %s\n' %
                               (mozpath.basename(per_source_flag.file_name), flag))

    def _process_computed_flags(self, computed_flags, backend_file):
        for var, flags in computed_flags.get_flags():
            backend_file.write(
                'COMPUTED_%s += %s\n' % (var,
                                         ' '.join(make_quote(shell_quote(f)) for f in flags)))

    def _process_non_default_target(self, libdef, target_name, backend_file):
        backend_file.write("%s:: %s\n" % (libdef.output_category, target_name))
        backend_file.write('MOZBUILD_NON_DEFAULT_TARGETS += %s\n' % target_name)

    def _process_shared_library(self, libdef, backend_file):
        backend_file.write_once('LIBRARY_NAME := %s\n' % libdef.basename)
        backend_file.write('FORCE_SHARED_LIB := 1\n')
        backend_file.write('IMPORT_LIBRARY := %s\n' % libdef.import_name)
        backend_file.write('SHARED_LIBRARY := %s\n' % libdef.lib_name)
        if libdef.soname:
            backend_file.write('DSO_SONAME := %s\n' % libdef.soname)
        if libdef.symbols_file:
            if libdef.symbols_link_arg:
                backend_file.write('EXTRA_DSO_LDOPTS += %s\n' % libdef.symbols_link_arg)
                backend_file.write('EXTRA_DEPS += %s\n' % libdef.symbols_file)
        if not libdef.cxx_link:
            backend_file.write('LIB_IS_C_ONLY := 1\n')
        if libdef.output_category:
            self._process_non_default_target(libdef, libdef.lib_name,
                                             backend_file)
            # Override the install rule target for this library. This is hacky,
            # but can go away as soon as we start building libraries in their
            # final location (bug 1459764).
            backend_file.write('SHARED_LIBRARY_TARGET := %s\n' %
                               libdef.output_category)

    def _process_static_library(self, libdef, backend_file):
        backend_file.write_once('LIBRARY_NAME := %s\n' % libdef.basename)
        backend_file.write('FORCE_STATIC_LIB := 1\n')
        backend_file.write('REAL_LIBRARY := %s\n' % libdef.lib_name)
        if libdef.no_expand_lib:
            backend_file.write('NO_EXPAND_LIBS := 1\n')

    def _process_sandboxed_wasm_library(self, libdef, backend_file):
        backend_file.write('WASM_LIBRARY := %s\n' % libdef.lib_name)

    def _process_rust_library(self, libdef, backend_file):
        backend_file.write_once('%s := %s\n' % (libdef.LIB_FILE_VAR, libdef.import_name))
        backend_file.write_once('CARGO_FILE := $(srcdir)/Cargo.toml\n')
        # Need to normalize the path so Cargo sees the same paths from all
        # possible invocations of Cargo with this CARGO_TARGET_DIR.  Otherwise,
        # Cargo's dependency calculations don't work as we expect and we wind
        # up recompiling lots of things.
        target_dir = mozpath.join(backend_file.objdir, libdef.target_dir)
        target_dir = mozpath.normpath(target_dir)
        backend_file.write('CARGO_TARGET_DIR := %s\n' % target_dir)
        if libdef.features:
            backend_file.write('%s := %s\n' % (libdef.FEATURES_VAR, ' '.join(libdef.features)))
        if libdef.output_category:
            self._process_non_default_target(libdef, libdef.import_name, backend_file)

    def _process_host_shared_library(self, libdef, backend_file):
        backend_file.write('HOST_SHARED_LIBRARY = %s\n' % libdef.lib_name)

    def _build_target_for_obj(self, obj):
        if hasattr(obj, 'output_category') and obj.output_category:
            target_name = obj.output_category
        else:
            target_name = obj.KIND
        if target_name == 'wasm':
            target_name = 'target'
        return '%s/%s' % (mozpath.relpath(obj.objdir,
                                          self.environment.topobjdir), target_name)

    def _process_linked_libraries(self, obj, backend_file):
        def pretty_relpath(lib, name):
            return os.path.normpath(mozpath.join(mozpath.relpath(lib.objdir, obj.objdir),
                                                 name))

        objs, shared_libs, os_libs, static_libs = self._expand_libs(obj)

        obj_target = obj.name
        if isinstance(obj, Program):
            obj_target = self._pretty_path(obj.output_path, backend_file)

        objs_ref = ' \\\n    '.join(os.path.relpath(o, obj.objdir)
                                    for o in objs)
        # Don't bother with a list file if we're only linking objects built
        # in this directory or building a real static library. This
        # accommodates clang-plugin, where we would otherwise pass an
        # incorrect list file format to the host compiler as well as when
        # creating an archive with AR, which doesn't understand list files.
        if (objs == obj.objs and not isinstance(obj, (HostLibrary, StaticLibrary,
                                                      SandboxedWasmLibrary)) or
            isinstance(obj, (StaticLibrary, SandboxedWasmLibrary)) and
            obj.no_expand_lib):
            backend_file.write_once('%s_OBJS := %s\n' % (obj.name,
                                                         objs_ref))
            backend_file.write('%s: %s\n' % (obj_target, objs_ref))
        elif not isinstance(obj, (HostLibrary, StaticLibrary,
                                  SandboxedWasmLibrary)):
            list_file_path = '%s.list' % obj.name.replace('.', '_')
            list_file_ref = self._make_list_file(obj.KIND, obj.objdir, objs,
                                                 list_file_path)
            backend_file.write_once('%s_OBJS := %s\n' %
                                    (obj.name, list_file_ref))
            backend_file.write_once('%s: %s\n' % (obj_target, list_file_path))
            backend_file.write('%s: %s\n' % (obj_target, objs_ref))

        for lib in shared_libs:
            assert obj.KIND != 'host' and obj.KIND != 'wasm'
            backend_file.write_once('SHARED_LIBS += %s\n' %
                                    pretty_relpath(lib, lib.import_name))

        # We have to link any Rust libraries after all intermediate static
        # libraries have been listed to ensure that the Rust libraries are
        # searched after the C/C++ objects that might reference Rust symbols.
        var = 'HOST_LIBS' if obj.KIND == 'host' else 'STATIC_LIBS'
        for lib in chain(
                (l for l in static_libs if not isinstance(l, BaseRustLibrary)),
                (l for l in static_libs if isinstance(l, BaseRustLibrary)),
        ):
            backend_file.write_once('%s += %s\n' %
                                    (var, pretty_relpath(lib, lib.import_name)))

        for lib in os_libs:
            if obj.KIND == 'target':
                backend_file.write_once('OS_LIBS += %s\n' % lib)
            elif obj.KIND == 'host':
                backend_file.write_once('HOST_EXTRA_LIBS += %s\n' % lib)

        if not isinstance(obj, (StaticLibrary, HostLibrary)) or obj.no_expand_lib:
            # This will create the node even if there aren't any linked libraries.
            build_target = self._build_target_for_obj(obj)
            self._compile_graph[build_target]

            # Make the build target depend on all the target/host-objects that
            # recursively are linked into it.
            def recurse_libraries(obj):
                for lib in obj.linked_libraries:
                    if isinstance(lib, (StaticLibrary, HostLibrary)) and not lib.no_expand_lib:
                        recurse_libraries(lib)
                    elif not isinstance(lib, ExternalLibrary):
                        self._compile_graph[build_target].add(
                            self._build_target_for_obj(lib))
                relobjdir = mozpath.relpath(obj.objdir, self.environment.topobjdir)
                objects_target = mozpath.join(relobjdir, '%s-objects' % obj.KIND)
                if objects_target in self._compile_graph:
                    self._compile_graph[build_target].add(objects_target)

            recurse_libraries(obj)

        # Process library-based defines
        self._process_defines(obj.lib_defines, backend_file)

    def _process_final_target_files(self, obj, files, backend_file):
        target = obj.install_target
        path = mozpath.basedir(target, (
            'dist/bin',
            'dist/xpi-stage',
            '_tests',
            'dist/include',
        ))
        if not path:
            raise Exception("Cannot install to " + target)

        # Exports are not interesting to artifact builds.
        if path == 'dist/include' and self.environment.is_artifact_build:
            return

        manifest = path.replace('/', '_')
        install_manifest = self._install_manifests[manifest]
        reltarget = mozpath.relpath(target, path)

        for path, files in files.walk():
            target_var = (mozpath.join(target, path)
                          if path else target).replace('/', '_')
            have_objdir_files = False
            for f in files:
                assert not isinstance(f, RenamedSourcePath)
                dest = mozpath.join(reltarget, path, f.target_basename)
                if not isinstance(f, ObjDirPath):
                    if '*' in f:
                        if f.startswith('/') or isinstance(f, AbsolutePath):
                            basepath, wild = os.path.split(f.full_path)
                            if '*' in basepath:
                                raise Exception("Wildcards are only supported in the filename part"
                                                " of srcdir-relative or absolute paths.")

                            install_manifest.add_pattern_link(basepath, wild, path)
                        else:
                            install_manifest.add_pattern_link(f.srcdir, f, path)
                    else:
                        install_manifest.add_link(f.full_path, dest)
                else:
                    install_manifest.add_optional_exists(dest)
                    backend_file.write('%s_FILES += %s\n' % (
                        target_var, self._pretty_path(f, backend_file)))
                    have_objdir_files = True
            if have_objdir_files:
                tier = 'export' if obj.install_target == 'dist/include' else 'misc'
                self._no_skip[tier].add(backend_file.relobjdir)
                backend_file.write('%s_DEST := $(DEPTH)/%s\n'
                                   % (target_var,
                                      mozpath.join(target, path)))
                backend_file.write('%s_TARGET := %s\n' % (target_var, tier))
                backend_file.write('INSTALL_TARGETS += %s\n' % target_var)

    def _process_final_target_pp_files(self, obj, files, backend_file, name):
        # Bug 1177710 - We'd like to install these via manifests as
        # preprocessed files. But they currently depend on non-standard flags
        # being added via some Makefiles, so for now we just pass them through
        # to the underlying Makefile.in.
        #
        # Note that if this becomes a manifest, OBJDIR_PP_FILES will likely
        # still need to use PP_TARGETS internally because we can't have an
        # install manifest for the root of the objdir.
        for i, (path, files) in enumerate(files.walk()):
            self._no_skip['misc'].add(backend_file.relobjdir)
            var = '%s_%d' % (name, i)
            for f in files:
                backend_file.write('%s += %s\n' % (
                    var, self._pretty_path(f, backend_file)))
            backend_file.write('%s_PATH := $(DEPTH)/%s\n'
                               % (var, mozpath.join(obj.install_target, path)))
            backend_file.write('%s_TARGET := misc\n' % var)
            backend_file.write('PP_TARGETS += %s\n' % var)

    def _write_localized_files_files(self, files, name, backend_file):
        for f in files:
            if not isinstance(f, ObjDirPath):
                # The emitter asserts that all srcdir files start with `en-US/`
                e, f = f.split('en-US/')
                assert(not e)
                if '*' in f:
                    # We can't use MERGE_FILE for wildcards because it takes
                    # only the first match internally. This is only used
                    # in one place in the tree currently so we'll hardcode
                    # that specific behavior for now.
                    backend_file.write('%s += $(wildcard $(LOCALE_SRCDIR)/%s)\n' % (name, f))
                else:
                    backend_file.write('%s += $(call MERGE_FILE,%s)\n' % (name, f))
            else:
                # Objdir files are allowed from LOCALIZED_GENERATED_FILES
                backend_file.write('%s += %s\n' % (name, self._pretty_path(f, backend_file)))

    def _process_localized_files(self, obj, files, backend_file):
        target = obj.install_target
        path = mozpath.basedir(target, ('dist/bin', ))
        if not path:
            raise Exception('Cannot install localized files to ' + target)
        for i, (path, files) in enumerate(files.walk()):
            name = 'LOCALIZED_FILES_%d' % i
            self._no_skip['libs'].add(backend_file.relobjdir)
            self._write_localized_files_files(files, name + '_FILES', backend_file)
            # Use FINAL_TARGET here because some l10n repack rules set
            # XPI_NAME to generate langpacks.
            backend_file.write('%s_DEST = $(FINAL_TARGET)/%s\n' % (name, path))
            backend_file.write('%s_TARGET := libs\n' % name)
            backend_file.write('INSTALL_TARGETS += %s\n' % name)

    def _process_localized_pp_files(self, obj, files, backend_file):
        target = obj.install_target
        path = mozpath.basedir(target, ('dist/bin', ))
        if not path:
            raise Exception('Cannot install localized files to ' + target)
        for i, (path, files) in enumerate(files.walk()):
            name = 'LOCALIZED_PP_FILES_%d' % i
            self._no_skip['libs'].add(backend_file.relobjdir)
            self._write_localized_files_files(files, name, backend_file)
            # Use FINAL_TARGET here because some l10n repack rules set
            # XPI_NAME to generate langpacks.
            backend_file.write('%s_PATH = $(FINAL_TARGET)/%s\n' % (name, path))
            backend_file.write('%s_TARGET := libs\n' % name)
            # Localized files will have different content in different
            # localizations, and some preprocessed files may not have
            # any preprocessor directives.
            backend_file.write('%s_FLAGS := --silence-missing-directive-warnings\n' % name)
            backend_file.write('PP_TARGETS += %s\n' % name)

    def _process_objdir_files(self, obj, files, backend_file):
        # We can't use an install manifest for the root of the objdir, since it
        # would delete all the other files that get put there by the build
        # system.
        for i, (path, files) in enumerate(files.walk()):
            self._no_skip['misc'].add(backend_file.relobjdir)
            for f in files:
                backend_file.write('OBJDIR_%d_FILES += %s\n' % (
                    i, self._pretty_path(f, backend_file)))
            backend_file.write('OBJDIR_%d_DEST := $(topobjdir)/%s\n' % (i, path))
            backend_file.write('OBJDIR_%d_TARGET := misc\n' % i)
            backend_file.write('INSTALL_TARGETS += OBJDIR_%d\n' % i)

    def _process_chrome_manifest_entry(self, obj, backend_file):
        fragment = Makefile()
        rule = fragment.create_rule(targets=['misc:'])

        top_level = mozpath.join(obj.install_target, 'chrome.manifest')
        if obj.path != top_level:
            args = [
                mozpath.join('$(DEPTH)', top_level),
                make_quote(shell_quote('manifest %s' %
                                       mozpath.relpath(obj.path,
                                                       obj.install_target))),
            ]
            rule.add_commands(['$(call py_action,buildlist,%s)' %
                               ' '.join(args)])
        args = [
            mozpath.join('$(DEPTH)', obj.path),
            make_quote(shell_quote(str(obj.entry))),
        ]
        rule.add_commands(['$(call py_action,buildlist,%s)' % ' '.join(args)])
        fragment.dump(backend_file.fh, removal_guard=False)

        self._no_skip['misc'].add(obj.relsrcdir)

    def _write_manifests(self, dest, manifests):
        man_dir = mozpath.join(self.environment.topobjdir, '_build_manifests',
                               dest)

        for k, manifest in manifests.items():
            with self._write_file(mozpath.join(man_dir, k)) as fh:
                manifest.write(fileobj=fh)

    def _write_master_test_manifest(self, path, manifests):
        with self._write_file(path) as master:
            master.write(
                '# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT MODIFY BY HAND.\n\n')

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
            pp.handleLine('# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT MODIFY BY HAND.\n')
            pp.handleLine('DEPTH := @DEPTH@\n')
            pp.handleLine('topobjdir := @topobjdir@\n')
            pp.handleLine('topsrcdir := @top_srcdir@\n')
            pp.handleLine('srcdir := @srcdir@\n')
            pp.handleLine('srcdir_rel := @srcdir_rel@\n')
            pp.handleLine('relativesrcdir := @relativesrcdir@\n')
            pp.handleLine('include $(DEPTH)/config/@autoconfmk@\n')
            if not stub:
                pp.do_include(obj.input_path)
            # Empty line to avoid failures when last line in Makefile.in ends
            # with a backslash.
            pp.handleLine('\n')
            pp.handleLine('include $(topsrcdir)/config/recurse.mk\n')
        if not stub:
            # Adding the Makefile.in here has the desired side-effect
            # that if the Makefile.in disappears, this will force
            # moz.build traversal. This means that when we remove empty
            # Makefile.in files, the old file will get replaced with
            # the autogenerated one automatically.
            self.backend_input_files.add(obj.input_path)

        self._makefile_out_count += 1

    def _handle_linked_rust_crates(self, obj, extern_crate_file):
        backend_file = self._get_backend_file_for(obj)

        backend_file.write('RS_STATICLIB_CRATE_SRC := %s\n' % extern_crate_file)

    def _handle_ipdl_sources(self, ipdl_dir, sorted_ipdl_sources, sorted_nonstatic_ipdl_sources,
                             sorted_static_ipdl_sources, unified_ipdl_cppsrcs_mapping):
        # Write out a master list of all IPDL source files.
        mk = Makefile()

        sorted_nonstatic_ipdl_basenames = list()
        for source in sorted_nonstatic_ipdl_sources:
            basename = os.path.basename(source)
            sorted_nonstatic_ipdl_basenames.append(basename)
            rule = mk.create_rule([basename])
            rule.add_dependencies([source])
            rule.add_commands([
                '$(RM) $@',
                '$(call py_action,preprocessor,$(DEFINES) $(ACDEFINES) '
                '$< -o $@)'
            ])

        mk.add_statement('ALL_IPDLSRCS := %s %s' % (' '.join(sorted_nonstatic_ipdl_basenames),
                                                    ' '.join(sorted_static_ipdl_sources)))

        self._add_unified_build_rules(mk, unified_ipdl_cppsrcs_mapping,
                                      unified_files_makefile_variable='CPPSRCS')

        # Preprocessed ipdl files are generated in ipdl_dir.
        mk.add_statement(
            'IPDLDIRS := %s %s' % (ipdl_dir, ' '.join(sorted(set(mozpath.dirname(p)
                                                      for p in sorted_static_ipdl_sources)))))

        with self._write_file(mozpath.join(ipdl_dir, 'ipdlsrcs.mk')) as ipdls:
            mk.dump(ipdls, removal_guard=False)

    def _handle_webidl_build(self, bindings_dir, unified_source_mapping,
                             webidls, expected_build_output_files,
                             global_define_files):
        include_dir = mozpath.join(self.environment.topobjdir, 'dist',
                                   'include')
        for f in expected_build_output_files:
            if f.startswith(include_dir):
                self._install_manifests['dist_include'].add_optional_exists(
                    mozpath.relpath(f, include_dir))

        # We pass WebIDL info to make via a completely generated make file.
        mk = Makefile()
        mk.add_statement('nonstatic_webidl_files := %s' % ' '.join(
            sorted(webidls.all_non_static_basenames())))
        mk.add_statement('globalgen_sources := %s' % ' '.join(
            sorted(global_define_files)))
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
            # GLOBAL_DEPS would be used here, but due to the include order of
            # our makefiles it's not set early enough to be useful, so we use
            # WEBIDL_PP_DEPS, which has analagous content.
            rule.add_dependencies([source, '$(WEBIDL_PP_DEPS)'])
            rule.add_commands([
                # Remove the file before writing so bindings that go from
                # static to preprocessed don't end up writing to a symlink,
                # which would modify content in the source directory.
                '$(RM) $@',
                '$(call py_action,preprocessor,$(DEFINES) $(ACDEFINES) '
                '$< -o $@)'
            ])

        self._add_unified_build_rules(mk,
                                      unified_source_mapping,
                                      unified_files_makefile_variable='unified_binding_cpp_files')

        webidls_mk = mozpath.join(bindings_dir, 'webidlsrcs.mk')
        with self._write_file(webidls_mk) as fh:
            mk.dump(fh, removal_guard=False)

        # Add the test directory to the compile graph.
        if self.environment.substs.get('ENABLE_TESTS'):
            self._compile_graph[mozpath.join(
                mozpath.relpath(bindings_dir, self.environment.topobjdir),
                'test', 'target-objects')]

    def _format_generated_file_input_name(self, path, obj):
        if obj.localized:
            # Localized generated files can have locale-specific inputs, which
            # are indicated by paths starting with `en-US/` or containing
            # `locales/en-US/`.
            if 'locales/en-US' in path:
                # We need an "absolute source path" relative to
                # topsrcdir, like "/source/path".
                if not path.startswith('/'):
                    path = '/' + mozpath.relpath(path.full_path, obj.topsrcdir)
                e, f = path.split('locales/en-US/', 1)
                assert(f)
                return '$(call MERGE_RELATIVE_FILE,{},{}locales)'.format(
                    f, e if not e.startswith('/') else e[len('/'):])
            elif path.startswith('en-US/'):
                e, f = path.split('en-US/', 1)
                assert(not e)
                return '$(call MERGE_FILE,%s)' % f
            return self._pretty_path(path, self._get_backend_file_for(obj))
        else:
            return self._pretty_path(path, self._get_backend_file_for(obj))

    def _format_generated_file_output_name(self, path, obj):
        return path
