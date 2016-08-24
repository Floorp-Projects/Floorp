# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import logging
import os
import re

from collections import (
    defaultdict,
    namedtuple,
)
from StringIO import StringIO
from itertools import chain

from mozpack.manifests import (
    InstallManifest,
)
import mozpack.path as mozpath

from mozbuild.frontend.context import (
    AbsolutePath,
    Path,
    RenamedSourcePath,
    SourcePath,
    ObjDirPath,
)
from .common import CommonBackend
from ..frontend.data import (
    AndroidAssetsDirs,
    AndroidResDirs,
    AndroidExtraResDirs,
    AndroidExtraPackages,
    AndroidEclipseProjectData,
    ChromeManifestEntry,
    ConfigFileSubstitution,
    ContextDerived,
    ContextWrapped,
    Defines,
    DirectoryTraversal,
    ExternalLibrary,
    FinalTargetFiles,
    FinalTargetPreprocessedFiles,
    GeneratedFile,
    GeneratedSources,
    HostDefines,
    HostLibrary,
    HostProgram,
    HostSimpleProgram,
    HostSources,
    InstallationTarget,
    JARManifest,
    JavaJarData,
    Library,
    LocalInclude,
    ObjdirFiles,
    ObjdirPreprocessedFiles,
    PerSourceFlag,
    Program,
    RustLibrary,
    SharedLibrary,
    SimpleProgram,
    Sources,
    StaticLibrary,
    TestManifest,
    VariablePassthru,
    XPIDLFile,
)
from ..util import (
    ensureParentDir,
    FileAvoidWrite,
)
from ..makeutil import Makefile
from mozbuild.shellutil import quote as shell_quote

MOZBUILD_VARIABLES = [
    b'ANDROID_APK_NAME',
    b'ANDROID_APK_PACKAGE',
    b'ANDROID_ASSETS_DIRS',
    b'ANDROID_EXTRA_PACKAGES',
    b'ANDROID_EXTRA_RES_DIRS',
    b'ANDROID_GENERATED_RESFILES',
    b'ANDROID_RES_DIRS',
    b'ASFLAGS',
    b'CMSRCS',
    b'CMMSRCS',
    b'CPP_UNIT_TESTS',
    b'DIRS',
    b'DIST_INSTALL',
    b'EXTRA_DSO_LDOPTS',
    b'EXTRA_JS_MODULES',
    b'EXTRA_PP_COMPONENTS',
    b'EXTRA_PP_JS_MODULES',
    b'FORCE_SHARED_LIB',
    b'FORCE_STATIC_LIB',
    b'FINAL_LIBRARY',
    b'HOST_CFLAGS',
    b'HOST_CSRCS',
    b'HOST_CMMSRCS',
    b'HOST_CXXFLAGS',
    b'HOST_EXTRA_LIBS',
    b'HOST_LIBRARY_NAME',
    b'HOST_PROGRAM',
    b'HOST_SIMPLE_PROGRAMS',
    b'IS_COMPONENT',
    b'JAR_MANIFEST',
    b'JAVA_JAR_TARGETS',
    b'LD_VERSION_SCRIPT',
    b'LIBRARY_NAME',
    b'LIBS',
    b'MAKE_FRAMEWORK',
    b'MODULE',
    b'NO_DIST_INSTALL',
    b'NO_EXPAND_LIBS',
    b'NO_INTERFACES_MANIFEST',
    b'NO_JS_MANIFEST',
    b'OS_LIBS',
    b'PARALLEL_DIRS',
    b'PREF_JS_EXPORTS',
    b'PROGRAM',
    b'PYTHON_UNIT_TESTS',
    b'RESOURCE_FILES',
    b'SDK_HEADERS',
    b'SDK_LIBRARY',
    b'SHARED_LIBRARY_LIBS',
    b'SHARED_LIBRARY_NAME',
    b'SIMPLE_PROGRAMS',
    b'SONAME',
    b'STATIC_LIBRARY_NAME',
    b'TEST_DIRS',
    b'TOOL_DIRS',
    # XXX config/Makefile.in specifies this in a make invocation
    #'USE_EXTENSION_MANIFEST',
    b'XPCSHELL_TESTS',
    b'XPIDL_MODULE',
]

DEPRECATED_VARIABLES = [
    b'ANDROID_RESFILES',
    b'EXPORT_LIBRARY',
    b'EXTRA_LIBS',
    b'HOST_LIBS',
    b'LIBXUL_LIBRARY',
    b'MOCHITEST_A11Y_FILES',
    b'MOCHITEST_BROWSER_FILES',
    b'MOCHITEST_BROWSER_FILES_PARTS',
    b'MOCHITEST_CHROME_FILES',
    b'MOCHITEST_FILES',
    b'MOCHITEST_FILES_PARTS',
    b'MOCHITEST_METRO_FILES',
    b'MOCHITEST_ROBOCOP_FILES',
    b'MODULE_OPTIMIZE_FLAGS',
    b'MOZ_CHROME_FILE_FORMAT',
    b'SHORT_LIBNAME',
    b'TESTING_JS_MODULES',
    b'TESTING_JS_MODULE_DIR',
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

    def __init__(self, srcdir, objdir, environment, topsrcdir, topobjdir):
        self.topsrcdir = topsrcdir
        self.srcdir = srcdir
        self.objdir = objdir
        self.relobjdir = mozpath.relpath(objdir, topobjdir)
        self.environment = environment
        self.name = mozpath.join(objdir, 'backend.mk')

        self.xpt_name = None

        self.fh = FileAvoidWrite(self.name, capture_diff=True)
        self.fh.write('# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT EDIT.\n')
        self.fh.write('\n')

    def write(self, buf):
        self.fh.write(buf)

    def write_once(self, buf):
        if isinstance(buf, unicode):
            buf = buf.encode('utf-8')
        if b'\n' + buf not in self.fh.getvalue():
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

    def add(self, dir, dirs=[], tests=[]):
        """
        Adds a directory to traversal, registering its subdirectories,
        sorted by categories. If the directory was already added to
        traversal, adds the new subdirectories to the already known lists.
        """
        subdirs = self._traversal.setdefault(dir, self.SubDirectories())
        for key, value in (('dirs', dirs), ('tests', tests)):
            assert(key in self.SubDirectoryCategories)
            getattr(subdirs, key).extend(value)

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
        self._install_manifests['dist_sdk']

        self._traversal = RecursiveMakeTraversal()
        self._compile_graph = defaultdict(set)

        self._no_skip = {
            'export': set(),
            'libs': set(),
            'misc': set(),
            'tools': set(),
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
                    obj.topsrcdir, self.environment.topobjdir)
        return self._backend_files[obj.objdir]

    def consume_object(self, obj):
        """Write out build files necessary to build with recursive make."""

        if not isinstance(obj, ContextDerived):
            return False

        backend_file = self._get_backend_file_for(obj)

        consumed = CommonBackend.consume_object(self, obj)

        # CommonBackend handles XPIDLFile and TestManifest, but we want to do
        # some extra things for them.
        if isinstance(obj, XPIDLFile):
            backend_file.xpt_name = '%s.xpt' % obj.module
            self._idl_dirs.add(obj.relobjdir)

        elif isinstance(obj, TestManifest):
            self._process_test_manifest(obj, backend_file)

        # If CommonBackend acknowledged the object, we're done with it.
        if consumed:
            return True

        if not isinstance(obj, Defines):
            self.consume_object(obj.defines)

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
                '.rs': 'RSSRCS',
                '.S': 'SSRCS',
            }
            variables = [suffix_map[obj.canonical_suffix]]
            if isinstance(obj, GeneratedSources):
                variables.append('GARBAGE')
                base = backend_file.objdir
            else:
                base = backend_file.srcdir
            for f in sorted(obj.files):
                f = mozpath.relpath(f, base)
                for var in variables:
                    backend_file.write('%s += %s\n' % (var, f))
        elif isinstance(obj, HostSources):
            suffix_map = {
                '.c': 'HOST_CSRCS',
                '.mm': 'HOST_CMMSRCS',
                '.cpp': 'HOST_CPPSRCS',
            }
            var = suffix_map[obj.canonical_suffix]
            for f in sorted(obj.files):
                backend_file.write('%s += %s\n' % (
                    var, mozpath.relpath(f, backend_file.srcdir)))
        elif isinstance(obj, VariablePassthru):
            # Sorted so output is consistent and we don't bump mtimes.
            for k, v in sorted(obj.variables.items()):
                if k == 'HAS_MISC_RULE':
                    self._no_skip['misc'].add(backend_file.relobjdir)
                    continue
                if isinstance(v, list):
                    for item in v:
                        backend_file.write(
                            '%s += %s\n' % (k, make_quote(shell_quote(item))))
                elif isinstance(v, bool):
                    if v:
                        backend_file.write('%s := 1\n' % k)
                else:
                    backend_file.write('%s := %s\n' % (k, v))
        elif isinstance(obj, HostDefines):
            self._process_defines(obj, backend_file, which='HOST_DEFINES')
        elif isinstance(obj, Defines):
            self._process_defines(obj, backend_file)

        elif isinstance(obj, GeneratedFile):
            export_suffixes = (
                '.c',
                '.cpp',
                '.h',
                '.inc',
                '.py',
            )
            tier = 'export' if any(f.endswith(export_suffixes) for f in obj.outputs) else 'misc'
            self._no_skip[tier].add(backend_file.relobjdir)
            first_output = obj.outputs[0]
            dep_file = "%s.pp" % first_output
            backend_file.write('%s:: %s\n' % (tier, first_output))
            for output in obj.outputs:
                if output != first_output:
                    backend_file.write('%s: %s ;\n' % (output, first_output))
                backend_file.write('GARBAGE += %s\n' % output)
            backend_file.write('EXTRA_MDDEPEND_FILES += %s\n' % dep_file)
            if obj.script:
                backend_file.write("""{output}: {script}{inputs}{backend}
\t$(REPORT_BUILD)
\t$(call py_action,file_generate,{script} {method} {output} $(MDDEPDIR)/{dep_file}{inputs}{flags})

""".format(output=first_output,
           dep_file=dep_file,
           inputs=' ' + ' '.join([self._pretty_path(f, backend_file) for f in obj.inputs]) if obj.inputs else '',
           flags=' ' + ' '.join(obj.flags) if obj.flags else '',
           backend=' backend.mk' if obj.flags else '',
           script=obj.script,
           method=obj.method))

        elif isinstance(obj, JARManifest):
            self._no_skip['libs'].add(backend_file.relobjdir)
            backend_file.write('JAR_MANIFEST := %s\n' % obj.path.full_path)

        elif isinstance(obj, Program):
            self._process_program(obj.program, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, HostProgram):
            self._process_host_program(obj.program, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, SimpleProgram):
            self._process_simple_program(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, HostSimpleProgram):
            self._process_host_simple_program(obj.program, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, LocalInclude):
            self._process_local_include(obj.path, backend_file)

        elif isinstance(obj, PerSourceFlag):
            self._process_per_source_flag(obj, backend_file)

        elif isinstance(obj, InstallationTarget):
            self._process_installation_target(obj, backend_file)

        elif isinstance(obj, ContextWrapped):
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
                return False

        elif isinstance(obj, RustLibrary):
            self.backend_input_files.add(obj.cargo_file)
            self._process_rust_library(obj, backend_file)
            # No need to call _process_linked_libraries, because Rust
            # libraries are self-contained objects at this point.

        elif isinstance(obj, SharedLibrary):
            self._process_shared_library(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, StaticLibrary):
            self._process_static_library(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, HostLibrary):
            self._process_host_library(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, FinalTargetFiles):
            self._process_final_target_files(obj, obj.files, backend_file)

        elif isinstance(obj, FinalTargetPreprocessedFiles):
            self._process_final_target_pp_files(obj, obj.files, backend_file, 'DIST_FILES')

        elif isinstance(obj, ObjdirFiles):
            self._process_objdir_files(obj, obj.files, backend_file)

        elif isinstance(obj, ObjdirPreprocessedFiles):
            self._process_final_target_pp_files(obj, obj.files, backend_file, 'OBJDIR_PP_FILES')

        elif isinstance(obj, AndroidResDirs):
            # Order matters.
            for p in obj.paths:
                backend_file.write('ANDROID_RES_DIRS += %s\n' % p.full_path)

        elif isinstance(obj, AndroidAssetsDirs):
            # Order matters.
            for p in obj.paths:
                backend_file.write('ANDROID_ASSETS_DIRS += %s\n' % p.full_path)

        elif isinstance(obj, AndroidExtraResDirs):
            # Order does not matter.
            for p in sorted(set(p.full_path for p in obj.paths)):
                backend_file.write('ANDROID_EXTRA_RES_DIRS += %s\n' % p)

        elif isinstance(obj, AndroidExtraPackages):
            # Order does not matter.
            for p in sorted(set(obj.packages)):
                backend_file.write('ANDROID_EXTRA_PACKAGES += %s\n' % p)

        elif isinstance(obj, ChromeManifestEntry):
            self._process_chrome_manifest_entry(obj, backend_file)

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
        ]

        root_deps_mk = Makefile()

        # Fill the dependencies for traversal of each tier.
        for tier, filter in filters:
            main, all_deps = \
                self._traversal.compute_dependencies(filter)
            for dir, deps in all_deps.items():
                if deps is not None or (dir in self._idl_dirs \
                                        and tier == 'export'):
                    rule = root_deps_mk.create_rule(['%s/%s' % (dir, tier)])
                if deps:
                    rule.add_dependencies('%s/%s' % (d, tier) for d in deps if d)
                if dir in self._idl_dirs and tier == 'export':
                    rule.add_dependencies(['xpcom/xpidl/%s' % tier])
            rule = root_deps_mk.create_rule(['recurse_%s' % tier])
            if main:
                rule.add_dependencies('%s/%s' % (d, tier) for d in main)

        all_compile_deps = reduce(lambda x,y: x|y,
            self._compile_graph.values()) if self._compile_graph else set()
        compile_roots = set(self._compile_graph.keys()) - all_compile_deps

        rule = root_deps_mk.create_rule(['recurse_compile'])
        rule.add_dependencies(compile_roots)
        for target, deps in sorted(self._compile_graph.items()):
            if deps:
                rule = root_deps_mk.create_rule([target])
                rule.add_dependencies(deps)

        root_mk = Makefile()

        # Fill root.mk with the convenience variables.
        for tier, filter in filters:
            all_dirs = self._traversal.traverse('', filter)
            root_mk.add_statement('%s_dirs := %s' % (tier, ' '.join(all_dirs)))

        # Need a list of compile targets because we can't use pattern rules:
        # https://savannah.gnu.org/bugs/index.php?42833
        root_mk.add_statement('compile_targets := %s' % ' '.join(sorted(
            set(self._compile_graph.keys()) | all_compile_deps)))

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
            makefile.add_statement('\n'
                '# Make sometimes gets confused between "foo" and "$(CURDIR)/foo".\n'
                '# Help it out by explicitly specifiying dependencies.')
            makefile.add_statement('all_absolute_unified_files := \\\n'
                                   '  $(addprefix $(CURDIR)/,$(%s))'
                                   % unified_files_makefile_variable)
            rule = makefile.create_rule(['$(all_absolute_unified_files)'])
            rule.add_dependencies(['$(CURDIR)/%: %'])

    def _check_blacklisted_variables(self, makefile_in, makefile_content):
        if b'EXTERNALLY_MANAGED_MAKE_FILE' in makefile_content:
            # Bypass the variable restrictions for externally managed makefiles.
            return

        for l in makefile_content.splitlines():
            l = l.strip()
            # Don't check comments
            if l.startswith(b'#'):
                continue
            for x in chain(MOZBUILD_VARIABLES, DEPRECATED_VARIABLES):
                if x not in l:
                    continue

                # Finding the variable name in the Makefile is not enough: it
                # may just appear as part of something else, like DIRS appears
                # in GENERATED_DIRS.
                if re.search(r'\b%s\s*[:?+]?=' % x, l):
                    if x in MOZBUILD_VARIABLES:
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
                with open(obj.output_path) as fh:
                    content = fh.read()
                    # Skip every directory but those with a Makefile
                    # containing a tools target, or XPI_PKGNAME or
                    # INSTALL_EXTENSION_ID.
                    for t in (b'XPI_PKGNAME', b'INSTALL_EXTENSION_ID',
                            b'tools'):
                        if t not in content:
                            continue
                        if t == b'tools' and not re.search('(?:^|\s)tools.*::', content, re.M):
                            continue
                        if objdir == self.environment.topobjdir:
                            continue
                        self._no_skip['tools'].add(mozpath.relpath(objdir,
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

    def _process_directory_traversal(self, obj, backend_file):
        """Process a data.DirectoryTraversal instance."""
        fh = backend_file.fh

        def relativize(base, dirs):
            return (mozpath.relpath(d.translated, base) for d in dirs)

        if obj.dirs:
            fh.write('DIRS := %s\n' % ' '.join(
                relativize(backend_file.objdir, obj.dirs)))
            self._traversal.add(backend_file.relobjdir,
                dirs=relativize(self.environment.topobjdir, obj.dirs))

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
            backend_file.write('FINAL_TARGET = $(if $(XPI_NAME),$(DIST)/xpi-stage/$(XPI_NAME),$(DIST)/bin)$(DIST_SUBDIR:%=/%)\n')

        if not obj.enabled:
            backend_file.write('NO_DIST_INSTALL := 1\n')

    def _handle_idl_manager(self, manager):
        build_files = self._install_manifests['xpidl']

        for p in ('Makefile', 'backend.mk', '.deps/.mkdir.done'):
            build_files.add_optional_exists(p)

        for idl in manager.idls.values():
            self._install_manifests['dist_idl'].add_symlink(idl['source'],
                idl['basename'])
            self._install_manifests['dist_include'].add_optional_exists('%s.h'
                % idl['root'])

        for module in manager.modules:
            build_files.add_optional_exists(mozpath.join('.deps',
                '%s.pp' % module))

        modules = manager.modules
        xpt_modules = sorted(modules.keys())
        xpt_files = set()
        registered_xpt_files = set()

        mk = Makefile()

        for module in xpt_modules:
            install_target, sources = modules[module]
            deps = sorted(sources)

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
            xpt_path = '$(DEPTH)/%s/components/%s.xpt' % (install_target, module)
            xpt_files.add(xpt_path)
            mk.add_statement('%s_deps = %s' % (module, ' '.join(deps)))

            if install_target.startswith('dist/'):
                path = mozpath.relpath(xpt_path, '$(DEPTH)/dist')
                prefix, subpath = path.split('/', 1)
                key = 'dist_%s' % prefix

                self._install_manifests[key].add_optional_exists(subpath)

        rules = StringIO()
        mk.dump(rules, removal_guard=False)

        interfaces_manifests = []
        dist_dir = mozpath.join(self.environment.topobjdir, 'dist')
        for manifest, entries in manager.interface_manifests.items():
            interfaces_manifests.append(mozpath.join('$(DEPTH)', manifest))
            for xpt in sorted(entries):
                registered_xpt_files.add(mozpath.join(
                    '$(DEPTH)', mozpath.dirname(manifest), xpt))

            if install_target.startswith('dist/'):
                path = mozpath.join(self.environment.topobjdir, manifest)
                path = mozpath.relpath(path, dist_dir)
                prefix, subpath = path.split('/', 1)
                key = 'dist_%s' % prefix
                self._install_manifests[key].add_optional_exists(subpath)

        chrome_manifests = [mozpath.join('$(DEPTH)', m) for m in sorted(manager.chrome_manifests)]

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
            chrome_manifests = ' '.join(chrome_manifests),
            interfaces_manifests = ' '.join(interfaces_manifests),
            xpidl_rules=rules.getvalue(),
            xpidl_modules=' '.join(xpt_modules),
            xpt_files=' '.join(sorted(xpt_files - registered_xpt_files)),
            registered_xpt_files=' '.join(sorted(registered_xpt_files)),
        ))

    def _process_program(self, program, backend_file):
        backend_file.write('PROGRAM = %s\n' % program)

    def _process_host_program(self, program, backend_file):
        backend_file.write('HOST_PROGRAM = %s\n' % program)

    def _process_simple_program(self, obj, backend_file):
        if obj.is_unit_test:
            backend_file.write('CPP_UNIT_TESTS += %s\n' % obj.program)
        else:
            backend_file.write('SIMPLE_PROGRAMS += %s\n' % obj.program)

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
                self._install_manifests['_test_files'].add_symlink(source, dest)
            except ValueError:
                if not obj.dupe_manifest and is_test:
                    raise

        for base, pattern, dest in obj.pattern_installs:
            try:
                self._install_manifests['_test_files'].add_pattern_symlink(base,
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
                (target, ' '.join(sorted(set(jar.extra_jars)))))
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

    def _process_shared_library(self, libdef, backend_file):
        backend_file.write_once('LIBRARY_NAME := %s\n' % libdef.basename)
        backend_file.write('FORCE_SHARED_LIB := 1\n')
        backend_file.write('IMPORT_LIBRARY := %s\n' % libdef.import_name)
        backend_file.write('SHARED_LIBRARY := %s\n' % libdef.lib_name)
        if libdef.variant == libdef.COMPONENT:
            backend_file.write('IS_COMPONENT := 1\n')
        if libdef.soname:
            backend_file.write('DSO_SONAME := %s\n' % libdef.soname)
        if libdef.is_sdk:
            backend_file.write('SDK_LIBRARY := %s\n' % libdef.import_name)
        if libdef.symbols_file:
            backend_file.write('SYMBOLS_FILE := %s\n' % libdef.symbols_file)
        if not libdef.cxx_link:
            backend_file.write('LIB_IS_C_ONLY := 1\n')

    def _process_static_library(self, libdef, backend_file):
        backend_file.write_once('LIBRARY_NAME := %s\n' % libdef.basename)
        backend_file.write('FORCE_STATIC_LIB := 1\n')
        backend_file.write('REAL_LIBRARY := %s\n' % libdef.lib_name)
        if libdef.is_sdk:
            backend_file.write('SDK_LIBRARY := %s\n' % libdef.import_name)
        if libdef.no_expand_lib:
            backend_file.write('NO_EXPAND_LIBS := 1\n')

    def _process_rust_library(self, libdef, backend_file):
        backend_file.write_once('RUST_LIBRARY_FILE := %s\n' % libdef.import_name)
        backend_file.write('CARGO_FILE := $(srcdir)/Cargo.toml')

    def _process_host_library(self, libdef, backend_file):
        backend_file.write('HOST_LIBRARY_NAME = %s\n' % libdef.basename)

    def _build_target_for_obj(self, obj):
        return '%s/%s' % (mozpath.relpath(obj.objdir,
            self.environment.topobjdir), obj.KIND)

    def _process_linked_libraries(self, obj, backend_file):
        def write_shared_and_system_libs(lib):
            for l in lib.linked_libraries:
                if isinstance(l, (StaticLibrary, RustLibrary)):
                    write_shared_and_system_libs(l)
                else:
                    backend_file.write_once('SHARED_LIBS += %s/%s\n'
                        % (pretty_relpath(l), l.import_name))
            for l in lib.linked_system_libs:
                backend_file.write_once('OS_LIBS += %s\n' % l)

        def pretty_relpath(lib):
            return '$(DEPTH)/%s' % mozpath.relpath(lib.objdir, topobjdir)

        topobjdir = mozpath.normsep(obj.topobjdir)
        # This will create the node even if there aren't any linked libraries.
        build_target = self._build_target_for_obj(obj)
        self._compile_graph[build_target]

        for lib in obj.linked_libraries:
            if not isinstance(lib, ExternalLibrary):
                self._compile_graph[build_target].add(
                    self._build_target_for_obj(lib))
            relpath = pretty_relpath(lib)
            if isinstance(obj, Library):
                if isinstance(lib, RustLibrary):
                    # We don't need to do anything here; we will handle
                    # linkage for any RustLibrary elsewhere.
                    continue
                elif isinstance(lib, StaticLibrary):
                    backend_file.write_once('STATIC_LIBS += %s/%s\n'
                                        % (relpath, lib.import_name))
                    if isinstance(obj, SharedLibrary):
                        write_shared_and_system_libs(lib)
                elif isinstance(obj, SharedLibrary):
                    assert lib.variant != lib.COMPONENT
                    backend_file.write_once('SHARED_LIBS += %s/%s\n'
                                        % (relpath, lib.import_name))
            elif isinstance(obj, (Program, SimpleProgram)):
                if isinstance(lib, StaticLibrary):
                    backend_file.write_once('STATIC_LIBS += %s/%s\n'
                                        % (relpath, lib.import_name))
                    write_shared_and_system_libs(lib)
                else:
                    assert lib.variant != lib.COMPONENT
                    backend_file.write_once('SHARED_LIBS += %s/%s\n'
                                        % (relpath, lib.import_name))
            elif isinstance(obj, (HostLibrary, HostProgram, HostSimpleProgram)):
                assert isinstance(lib, HostLibrary)
                backend_file.write_once('HOST_LIBS += %s/%s\n'
                                   % (relpath, lib.import_name))

        # We have to link any Rust libraries after all intermediate static
        # libraries have been listed to ensure that the Rust libraries are
        # searched after the C/C++ objects that might reference Rust symbols.

        def find_rlibs(obj):
            if isinstance(obj, RustLibrary):
                yield obj
            elif isinstance(obj, StaticLibrary) and not obj.no_expand_lib:
                for l in obj.linked_libraries:
                    for rlib in find_rlibs(l):
                        yield rlib

        # Check if we have any rust libraries to prelink and include in our
        # final library. If we do, write out the RUST_PRELINK information
        rlibs = []
        if isinstance(obj, (SharedLibrary, StaticLibrary)):
            for l in obj.linked_libraries:
                rlibs += find_rlibs(l)
        if rlibs:
            prelink_libname = '%s/%s%s-rs-prelink%s' \
                              % (relpath,
                                 obj.config.lib_prefix,
                                 obj.basename,
                                 obj.config.lib_suffix)
            backend_file.write('RUST_PRELINK := %s\n' % prelink_libname)
            backend_file.write_once('STATIC_LIBS += %s\n' % prelink_libname)

            extern_crate_file = mozpath.join(
                obj.objdir, '%s-rs-prelink.rs' % obj.basename)
            with self._write_file(extern_crate_file) as f:
                f.write('// AUTOMATICALLY GENERATED.  DO NOT EDIT.\n\n')
                for rlib in rlibs:
                    f.write('extern crate %s;\n'
                            % rlib.basename.replace('-', '_'))
            backend_file.write('RUST_PRELINK_SRC := %s\n' % extern_crate_file)

            backend_file.write('RUST_PRELINK_FLAGS :=\n')
            backend_file.write('RUST_PRELINK_DEPS :=\n')
            for rlib in rlibs:
                rlib_relpath = pretty_relpath(rlib)
                backend_file.write('RUST_PRELINK_FLAGS += --extern %s=%s/%s\n'
                                   % (rlib.basename.replace('-', '_'), rlib_relpath, rlib.import_name))
                backend_file.write('RUST_PRELINK_FLAGS += -L %s/%s\n'
                                   % (rlib_relpath, rlib.deps_path))
                backend_file.write('RUST_PRELINK_DEPS += %s/%s\n'
                                   % (rlib_relpath, rlib.import_name))

        for lib in obj.linked_system_libs:
            if obj.KIND == 'target':
                backend_file.write_once('OS_LIBS += %s\n' % lib)
            else:
                backend_file.write_once('HOST_EXTRA_LIBS += %s\n' % lib)

        # Process library-based defines
        self._process_defines(obj.lib_defines, backend_file)

    def _process_final_target_files(self, obj, files, backend_file):
        target = obj.install_target
        path = mozpath.basedir(target, (
            'dist/bin',
            'dist/xpi-stage',
            '_tests',
            'dist/include',
            'dist/branding',
            'dist/sdk',
        ))
        if not path:
            raise Exception("Cannot install to " + target)

        manifest = path.replace('/', '_')
        install_manifest = self._install_manifests[manifest]
        reltarget = mozpath.relpath(target, path)

        # Also emit the necessary rules to create $(DIST)/branding during
        # partial tree builds. The locale makefiles rely on this working.
        if path == 'dist/branding':
            backend_file.write('NONRECURSIVE_TARGETS += export\n')
            backend_file.write('NONRECURSIVE_TARGETS_export += branding\n')
            backend_file.write('NONRECURSIVE_TARGETS_export_branding_DIRECTORY = $(DEPTH)\n')
            backend_file.write('NONRECURSIVE_TARGETS_export_branding_TARGETS += install-dist/branding\n')

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
                                raise Exception("Wildcards are only supported in the filename part of "
                                                "srcdir-relative or absolute paths.")

                            install_manifest.add_pattern_symlink(basepath, wild, path)
                        else:
                            install_manifest.add_pattern_symlink(f.srcdir, f, path)
                    else:
                        install_manifest.add_symlink(f.full_path, dest)
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

        self._no_skip['misc'].add(obj.relativedir)

    def _write_manifests(self, dest, manifests):
        man_dir = mozpath.join(self.environment.topobjdir, '_build_manifests',
            dest)

        for k, manifest in manifests.items():
            with self._write_file(mozpath.join(man_dir, k)) as fh:
                manifest.write(fileobj=fh)

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
            pp.handleLine(b'topobjdir := @topobjdir@\n')
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

        self._makefile_out_count += 1

    def _handle_linked_rust_crates(self, obj, extern_crate_file):
        backend_file = self._get_backend_file_for(obj)

        backend_file.write('RS_STATICLIB_CRATE_SRC := %s\n' % extern_crate_file)

    def _handle_ipdl_sources(self, ipdl_dir, sorted_ipdl_sources,
                             unified_ipdl_cppsrcs_mapping):
        # Write out a master list of all IPDL source files.
        mk = Makefile()

        mk.add_statement('ALL_IPDLSRCS := %s' % ' '.join(sorted_ipdl_sources))

        self._add_unified_build_rules(mk, unified_ipdl_cppsrcs_mapping,
                                      unified_files_makefile_variable='CPPSRCS')

        mk.add_statement('IPDLDIRS := %s' % ' '.join(sorted(set(mozpath.dirname(p)
            for p in self._ipdl_sources))))

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
            rule.add_dependencies([source, '$(GLOBAL_DEPS)'])
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
