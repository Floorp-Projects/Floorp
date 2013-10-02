# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import errno
import itertools
import logging
import os
import re
import types

from collections import namedtuple

import mozbuild.makeutil as mozmakeutil
from mozpack.copier import FilePurger
from mozpack.manifests import (
    InstallManifest,
)
import mozpack.path as mozpath

from .common import CommonBackend
from ..frontend.data import (
    ConfigFileSubstitution,
    Defines,
    DirectoryTraversal,
    Exports,
    GeneratedEventWebIDLFile,
    GeneratedWebIDLFile,
    IPDLFile,
    LocalInclude,
    PreprocessedTestWebIDLFile,
    PreprocessedWebIDLFile,
    Program,
    SandboxDerived,
    TestWebIDLFile,
    VariablePassthru,
    XPIDLFile,
    TestManifest,
    WebIDLFile,
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
        self.path = os.path.join(objdir, 'backend.mk')

        # XPIDLFiles attached to this file.
        self.idls = []
        self.xpt_name = None

        self.fh = FileAvoidWrite(self.path)
        self.fh.write('# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT EDIT.\n')
        self.fh.write('\n')
        self.fh.write('MOZBUILD_DERIVED := 1\n')

        # The global rule to incur backend generation generates Makefiles.
        self.fh.write('NO_MAKEFILE_RULE := 1\n')

        # We can't blindly have a SUBMAKEFILES rule because some of the
        # Makefile may not have a corresponding Makefile.in. For the case
        # where a new directory is added, the mozbuild file referencing that
        # new directory will need updated. This will cause a full backend
        # scan and build, installing the new Makefile.
        self.fh.write('NO_SUBMAKEFILES_RULE := 1\n')

    def write(self, buf):
        self.fh.write(buf)

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
        self._webidl_sources = set()
        self._generated_events_webidl_sources = set()
        self._test_webidl_sources = set()
        self._preprocessed_test_webidl_sources = set()
        self._preprocessed_webidl_sources = set()
        self._generated_webidl_sources = set()

        def detailed(summary):
            return '{:d} total backend files. {:d} created; {:d} updated; {:d} unchanged'.format(
                summary.managed_count, summary.created_count,
                summary.updated_count, summary.unchanged_count)

        # This is a little kludgy and could be improved with a better API.
        self.summary.backend_detailed_summary = types.MethodType(detailed,
            self.summary)

        self._test_manifests = {}

        self.backend_input_files.add(os.path.join(self.environment.topobjdir,
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

    def _update_from_avoid_write(self, result):
        existed, updated = result

        if not existed:
            self.summary.created_count += 1
        elif updated:
            self.summary.updated_count += 1
        else:
            self.summary.unchanged_count += 1

    def consume_object(self, obj):
        """Write out build files necessary to build with recursive make."""

        CommonBackend.consume_object(self, obj)

        if not isinstance(obj, SandboxDerived):
            return

        backend_file = self._backend_files.get(obj.srcdir,
            BackendMakeFile(obj.srcdir, obj.objdir, self.get_environment(obj)))

        if isinstance(obj, DirectoryTraversal):
            self._process_directory_traversal(obj, backend_file)
        elif isinstance(obj, ConfigFileSubstitution):
            self._update_from_avoid_write(
                backend_file.environment.create_config_file(obj.output_path))
            self.backend_input_files.add(obj.input_path)
            self.summary.managed_count += 1
        elif isinstance(obj, XPIDLFile):
            backend_file.idls.append(obj)
            backend_file.xpt_name = '%s.xpt' % obj.module
        elif isinstance(obj, VariablePassthru):
            # Sorted so output is consistent and we don't bump mtimes.
            for k, v in sorted(obj.variables.items()):
                if isinstance(v, list):
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

        elif isinstance(obj, IPDLFile):
            self._ipdl_sources.add(mozpath.join(obj.srcdir, obj.basename))

        elif isinstance(obj, WebIDLFile):
            self._webidl_sources.add(mozpath.join(obj.srcdir, obj.basename))
            self._process_webidl_basename(obj.basename)

        elif isinstance(obj, GeneratedEventWebIDLFile):
            self._generated_events_webidl_sources.add(mozpath.join(obj.srcdir, obj.basename))

        elif isinstance(obj, TestWebIDLFile):
            self._test_webidl_sources.add(mozpath.join(obj.srcdir,
                                                       obj.basename))
            # Test WebIDL files are not exported.

        elif isinstance(obj, PreprocessedTestWebIDLFile):
            self._preprocessed_test_webidl_sources.add(mozpath.join(obj.srcdir,
                                                                    obj.basename))
            # Test WebIDL files are not exported.

        elif isinstance(obj, GeneratedWebIDLFile):
            self._generated_webidl_sources.add(mozpath.join(obj.srcdir,
                                                            obj.basename))
            self._process_webidl_basename(obj.basename)

        elif isinstance(obj, PreprocessedWebIDLFile):
            self._preprocessed_webidl_sources.add(mozpath.join(obj.srcdir,
                                                               obj.basename))
            self._process_webidl_basename(obj.basename)

        elif isinstance(obj, Program):
            self._process_program(obj.program, backend_file)

        elif isinstance(obj, TestManifest):
            self._process_test_manifest(obj, backend_file)

        elif isinstance(obj, LocalInclude):
            self._process_local_include(obj.path, backend_file)

        self._backend_files[obj.srcdir] = backend_file

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

        # Skip static dirs during export traversal, or build everything in
        # parallel when enabled.
        def export_filter(current, subdirs):
            if self._parallel_export:
                return parallel_filter(current, subdirs)
            return current, subdirs.parallel, \
                subdirs.dirs + subdirs.tests + subdirs.tools

        # Skip tools dirs during libs traversal
        def libs_filter(current, subdirs):
            if current in self._may_skip[tier]:
                current = None
            return current, subdirs.parallel, \
                subdirs.static + subdirs.dirs + subdirs.tests

        # compile, binaries and tools tiers use the same traversal as export
        filters = {
            'export': export_filter,
            'compile': parallel_filter,
            'binaries': parallel_filter,
            'libs': libs_filter,
            'tools': parallel_filter,
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
                    root_mk.add_statement('%s_dirs := %s' % (tier, ' '.join(dirs)))
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

        root = FileAvoidWrite(
            os.path.join(self.environment.topobjdir, 'root.mk'))
        root_deps = FileAvoidWrite(
            os.path.join(self.environment.topobjdir, 'root-deps.mk'))
        root_mk.dump(root, removal_guard=False)
        root_deps_mk.dump(root_deps, removal_guard=False)
        self._update_from_avoid_write(root.close())
        self._update_from_avoid_write(root_deps.close())

    def _add_unified_build_rules(self, makefile, files, output_directory,
                                 unified_prefix='Unified',
                                 extra_dependencies=[],
                                 unified_files_makefile_variable='unified_files',
                                 include_curdir_build_rules=True):
        files_per_unified_file = 16

        explanation = "\n" \
            "# We build files in 'unified' mode by including several files\n" \
            "# together into a single source file.  This cuts down on\n" \
            "# compilation times and debug information size.  %d was chosen as\n" \
            "# a reasonable compromise between clobber rebuild time, incremental\n" \
            "# rebuild time, and compiler memory usage.\n" % files_per_unified_file
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
                                                      sorted(files))):
                just_the_filenames = list(filter_out_dummy(unified_group))
                yield '%s%d.cpp' % (unified_prefix, i), just_the_filenames

        all_sources = ' '.join(source for source, _ in unified_files())
        makefile.add_statement('%s := %s\n' % (unified_files_makefile_variable,
                                               all_sources))

        regen_cmds = ['for f in $(filter %.cpp,$^); do echo "#include \\"$$f\\""; done > $@']
        for unified_file, source_filenames in unified_files():
            rule = makefile.create_rule([unified_file])
            rule.add_dependencies(extra_dependencies + source_filenames)
            rule.add_commands(regen_cmds)

            # The rule we just defined is only for cases where the cpp files get
            # blown away and we need to regenerate them.  The rule doesn't correctly
            # handle source files being added/removed/renamed.  Therefore, we
            # generate them here also to make sure everything's up-to-date.
            with FileAvoidWrite(os.path.join(output_directory, unified_file)) as f:
                f.write('\n'.join(['#include "%s"' % s for s in source_filenames]))

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

        for srcdir in sorted(self._backend_files.keys()):
            bf = self._backend_files[srcdir]

            if not os.path.exists(bf.objdir):
                try:
                    os.makedirs(bf.objdir)
                except OSError as error:
                    if error.errno != errno.EEXIST:
                        raise

            makefile_in = os.path.join(srcdir, 'Makefile.in')
            makefile = os.path.join(bf.objdir, 'Makefile')

            # If Makefile.in exists, use it as a template. Otherwise, create a
            # stub.
            stub = not os.path.exists(makefile_in)
            if not stub:
                self.log(logging.DEBUG, 'substitute_makefile',
                    {'path': makefile}, 'Substituting makefile: {path}')

                # Adding the Makefile.in here has the desired side-effect that
                # if the Makefile.in disappears, this will force moz.build
                # traversal. This means that when we remove empty Makefile.in
                # files, the old file will get replaced with the autogenerated
                # one automatically.
                self.backend_input_files.add(makefile_in)

                for skiplist in self._may_skip.values():
                    if bf.relobjdir in skiplist:
                        skiplist.remove(bf.relobjdir)
            else:
                self.log(logging.DEBUG, 'stub_makefile',
                    {'path': makefile}, 'Creating stub Makefile: {path}')

            # Can't skip directories with a jar.mn for the 'libs' tier.
            if bf.relobjdir in self._may_skip['libs'] and \
                    os.path.exists(os.path.join(srcdir, 'jar.mn')):
                self._may_skip['libs'].remove(bf.relobjdir)

            self._update_from_avoid_write(
                bf.environment.create_makefile(makefile, stub=stub))
            self.summary.managed_count += 1

            self._update_from_avoid_write(bf.close())
            self.summary.managed_count += 1

        self._fill_root_mk()

        # Write out a master list of all IPDL source files.
        ipdl_dir = os.path.join(self.environment.topobjdir, 'ipc', 'ipdl')
        ipdls = FileAvoidWrite(os.path.join(ipdl_dir, 'ipdlsrcs.mk'))
        mk = mozmakeutil.Makefile()

        sorted_ipdl_sources = list(sorted(self._ipdl_sources))
        mk.add_statement('ALL_IPDLSRCS := %s\n' % ' '.join(sorted_ipdl_sources))

        def files_from(ipdl):
            base = os.path.basename(ipdl)
            root, ext = os.path.splitext(base)

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

        mk.add_statement('IPDLDIRS := %s\n' % ' '.join(sorted(set(os.path.dirname(p)
            for p in self._ipdl_sources))))

        mk.dump(ipdls, removal_guard=False)

        self._update_from_avoid_write(ipdls.close())
        self.summary.managed_count += 1

        # Write out master lists of WebIDL source files.
        bindings_dir = os.path.join(self.environment.topobjdir, 'dom', 'bindings')
        webidls = FileAvoidWrite(os.path.join(bindings_dir, 'webidlsrcs.mk'))

        mk = mozmakeutil.Makefile()

        def write_var(variable, sources):
            files = [os.path.basename(f) for f in sorted(sources)]
            mk.add_statement('%s += %s\n' % (variable, ' '.join(files)))
        write_var('webidl_files', self._webidl_sources)
        write_var('generated_events_webidl_files', self._generated_events_webidl_sources)
        write_var('test_webidl_files', self._test_webidl_sources)
        write_var('preprocessed_test_webidl_files', self._preprocessed_test_webidl_sources)
        write_var('generated_webidl_files', self._generated_webidl_sources)
        write_var('preprocessed_webidl_files', self._preprocessed_webidl_sources)

        all_webidl_files = itertools.chain(iter(self._webidl_sources),
                                           iter(self._generated_events_webidl_sources),
                                           iter(self._generated_webidl_sources),
                                           iter(self._preprocessed_webidl_sources))
        all_webidl_files = [os.path.basename(x) for x in all_webidl_files]
        all_webidl_sources = [re.sub(r'\.webidl$', 'Binding.cpp', x) for x in all_webidl_files]

        self._add_unified_build_rules(mk, all_webidl_sources,
                                      bindings_dir,
                                      unified_prefix='UnifiedBindings',
                                      unified_files_makefile_variable='unified_binding_cpp_files')

        # Assume that Somebody Else has responsibility for correctly
        # specifying removal dependencies for |all_webidl_sources|.
        mk.dump(webidls, removal_guard=False)

        self._update_from_avoid_write(webidls.close())
        self.summary.managed_count += 1

        # Write out a dependency file used to determine whether a config.status
        # re-run is needed.
        backend_built_path = os.path.join(self.environment.topobjdir,
            'backend.%s.built' % self.__class__.__name__).replace(os.sep, '/')
        backend_deps = FileAvoidWrite('%s.pp' % backend_built_path)
        inputs = sorted(p.replace(os.sep, '/') for p in self.backend_input_files)

        # We need to use $(DEPTH) so the target here matches what's in
        # rules.mk. If they are different, the dependencies don't get pulled in
        # properly.
        backend_deps.write('$(DEPTH)/backend.RecursiveMakeBackend.built: %s\n' %
            ' '.join(inputs))
        for path in inputs:
            backend_deps.write('%s:\n' % path)

        self._update_from_avoid_write(backend_deps.close())
        self.summary.managed_count += 1

        # Make the master test manifest files.
        for flavor, t in self._test_manifests.items():
            install_prefix, manifests = t
            manifest_stem = os.path.join(install_prefix, '%s.ini' % flavor)
            self._write_master_test_manifest(os.path.join(
                self.environment.topobjdir, '_tests', manifest_stem),
                manifests)

            # Catch duplicate inserts.
            try:
                self._install_manifests['tests'].add_optional_exists(manifest_stem)
            except ValueError:
                pass

        self._write_manifests('install', self._install_manifests)

        ensureParentDir(os.path.join(self.environment.topobjdir, 'dist', 'foo'))

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

        if len(obj.external_make_dirs):
            fh.write('DIRS += %s\n' % ' '.join(obj.external_make_dirs))
            self._traversal.add(backend_file.relobjdir,
                                dirs=relativize(obj.external_make_dirs))

        if len(obj.parallel_external_make_dirs):
            fh.write('PARALLEL_DIRS += %s\n' %
                ' '.join(obj.parallel_external_make_dirs))
            self._traversal.add(backend_file.relobjdir,
                                parallel=relativize(obj.parallel_external_make_dirs))

        # The directory needs to be registered whether subdirectories have been
        # registered or not.
        self._traversal.add(backend_file.relobjdir)

        if obj.is_tool_dir:
            fh.write('IS_TOOL_DIR := 1\n')

        if self._no_skip:
            return

        affected_tiers = set(obj.affected_tiers)
        if 'compile' in affected_tiers or 'binaries' in affected_tiers:
            affected_tiers.add('libs')
        if obj.is_tool_dir and 'libs' in affected_tiers:
            affected_tiers.remove('libs')
            affected_tiers.add('tools')

        for tier in set(self._may_skip.keys()) - affected_tiers:
            self._may_skip[tier].add(backend_file.relobjdir)

    def _process_exports(self, obj, exports, backend_file, namespace=""):
        # This may not be needed, but is present for backwards compatibility
        # with the old make rules, just in case.
        if not obj.dist_install:
            return

        strings = exports.get_strings()
        if namespace:
            namespace += '/'

        for s in strings:
            source = os.path.normpath(os.path.join(obj.srcdir, s))
            dest = '%s%s' % (namespace, os.path.basename(s))
            self._install_manifests['dist_include'].add_symlink(source, dest)

            if not os.path.exists(source):
                raise Exception('File listed in EXPORTS does not exist: %s' % source)

        children = exports.get_children()
        for subdir in sorted(children):
            self._process_exports(obj, children[subdir], backend_file,
                namespace=namespace + subdir)

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

        out_path = os.path.join(self.environment.topobjdir, 'config',
            'makefiles', 'xpidl', 'Makefile')
        result = self.environment.create_config_file(out_path, extra=dict(
            xpidl_rules='\n'.join(rules),
            xpidl_modules=' '.join(xpt_modules),
        ))
        self._update_from_avoid_write(result)
        self.summary.managed_count += 1

        # The Makefile can't regenerate itself because of custom substitution.
        # We need to list it here to ensure changes cause regeneration.
        self.backend_input_files.add(os.path.join(self.environment.topsrcdir,
            'config', 'makefiles', 'xpidl', 'Makefile.in'))

    def _process_program(self, program, backend_file):
        backend_file.write('PROGRAM = %s\n' % program)

    def _process_webidl_basename(self, basename):
        header = 'mozilla/dom/%sBinding.h' % os.path.splitext(basename)[0]
        self._install_manifests['dist_include'].add_optional_exists(header)

    def _process_test_manifest(self, obj, backend_file):
        self.backend_input_files.add(os.path.join(obj.topsrcdir,
            obj.manifest_relpath))

        # Duplicate manifests may define the same file. That's OK.
        for source, dest in obj.installs.items():
            try:
                self._install_manifests['tests'].add_symlink(source, dest)
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
        m[1].add(obj.manifest_relpath)

    def _process_local_include(self, local_include, backend_file):
        if local_include.startswith('/'):
            path = '$(topsrcdir)'
        else:
            path = '$(srcdir)/'
        backend_file.write('LOCAL_INCLUDES += -I%s%s\n' % (path, local_include))

    def _write_manifests(self, dest, manifests):
        man_dir = os.path.join(self.environment.topobjdir, '_build_manifests',
            dest)

        # We have a purger for the manifests themselves to ensure legacy
        # manifests are deleted.
        purger = FilePurger()

        for k, manifest in manifests.items():
            purger.add(k)

            fh = FileAvoidWrite(os.path.join(man_dir, k))
            manifest.write(fileobj=fh)
            self._update_from_avoid_write(fh.close())

        purger.purge(man_dir)

    def _write_master_test_manifest(self, path, manifests):
        master = FileAvoidWrite(path)
        master.write(
            '; THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT MODIFY BY HAND.\n\n')

        for manifest in sorted(manifests):
            master.write('[include:%s]\n' % manifest)

        self._update_from_avoid_write(master.close())
        self.summary.managed_count += 1
