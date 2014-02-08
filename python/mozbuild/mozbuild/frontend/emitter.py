# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import json
import logging
import os
import traceback
import sys
import time

from mach.mixin.logging import LoggingMixin

import mozpack.path as mozpath
import manifestparser

from .data import (
    ConfigFileSubstitution,
    Defines,
    DirectoryTraversal,
    Exports,
    GeneratedEventWebIDLFile,
    GeneratedInclude,
    GeneratedWebIDLFile,
    ExampleWebIDLInterface,
    HeaderFileSubstitution,
    HostProgram,
    HostSimpleProgram,
    InstallationTarget,
    IPDLFile,
    JARManifest,
    LibraryDefinition,
    LocalInclude,
    PreprocessedTestWebIDLFile,
    PreprocessedWebIDLFile,
    Program,
    ReaderSummary,
    SandboxWrapped,
    SimpleProgram,
    TestWebIDLFile,
    TestManifest,
    VariablePassthru,
    WebIDLFile,
    XPIDLFile,
)

from .reader import (
    MozbuildSandbox,
    SandboxValidationError,
)

from .gyp_reader import GypSandbox


class TreeMetadataEmitter(LoggingMixin):
    """Converts the executed mozbuild files into data structures.

    This is a bridge between reader.py and data.py. It takes what was read by
    reader.BuildReader and converts it into the classes defined in the data
    module.
    """

    def __init__(self, config):
        self.populate_logger()

        self.config = config

        # TODO add mozinfo into config or somewhere else.
        mozinfo_path = mozpath.join(config.topobjdir, 'mozinfo.json')
        if os.path.exists(mozinfo_path):
            self.mozinfo = json.load(open(mozinfo_path, 'rt'))
        else:
            self.mozinfo = {}

        self._libs = {}
        self._final_libs = []

    def emit(self, output):
        """Convert the BuildReader output into data structures.

        The return value from BuildReader.read_topsrcdir() (a generator) is
        typically fed into this function.
        """
        file_count = 0
        sandbox_execution_time = 0.0
        emitter_time = 0.0
        sandboxes = {}

        def emit_objs(objs):
            for o in objs:
                yield o
                if not o._ack:
                    raise Exception('Unhandled object of type %s' % type(o))

        for out in output:
            if isinstance(out, (MozbuildSandbox, GypSandbox)):
                # Keep all sandboxes around, we will need them later.
                sandboxes[out['OBJDIR']] = out

                start = time.time()
                # We need to expand the generator for the timings to work.
                objs = list(self.emit_from_sandbox(out))
                emitter_time += time.time() - start

                for o in emit_objs(objs): yield o

                # Update the stats.
                file_count += len(out.all_paths)
                sandbox_execution_time += out.execution_time

            else:
                raise Exception('Unhandled output type: %s' % type(out))

        start = time.time()
        objs = list(self._emit_libs_derived(sandboxes))
        emitter_time += time.time() - start

        for o in emit_objs(objs): yield o

        yield ReaderSummary(file_count, sandbox_execution_time, emitter_time)

    def _emit_libs_derived(self, sandboxes):
        for objdir, libname, final_lib in self._final_libs:
            if final_lib not in self._libs:
                raise Exception('FINAL_LIBRARY in %s (%s) does not match any '
                                'LIBRARY_NAME' % (objdir, final_lib))
            libs = self._libs[final_lib]
            if len(libs) > 1:
                raise Exception('FINAL_LIBRARY in %s (%s) matches a '
                                'LIBRARY_NAME defined in multiple places (%s)' %
                                (objdir, final_lib, ', '.join(libs.keys())))
            libs.values()[0].link_static_lib(objdir, libname)
            self._libs[libname][objdir].refcount += 1
            # The refcount can't go above 1 right now. It might in the future,
            # but that will have to be specifically handled. At which point the
            # refcount might have to be a list of referencees, for better error
            # reporting.
            assert self._libs[libname][objdir].refcount <= 1

        def recurse_libs(path, name):
            for p, n in self._libs[name][path].static_libraries:
                yield p
                for q in recurse_libs(p, n):
                    yield q

        for basename, libs in self._libs.items():
            for path, libdef in libs.items():
                # For all root libraries (i.e. libraries that don't have a
                # FINAL_LIBRARY), record, for each static library it links
                # (recursively), that its FINAL_LIBRARY is that root library.
                if not libdef.refcount:
                    for p in recurse_libs(path, basename):
                        passthru = VariablePassthru(sandboxes[p])
                        passthru.variables['FINAL_LIBRARY'] = basename
                        yield passthru
                yield libdef

    def emit_from_sandbox(self, sandbox):
        """Convert a MozbuildSandbox to tree metadata objects.

        This is a generator of mozbuild.frontend.data.SandboxDerived instances.
        """
        # We always emit a directory traversal descriptor. This is needed by
        # the recursive make backend.
        for o in self._emit_directory_traversal_from_sandbox(sandbox): yield o

        for path in sandbox['CONFIGURE_SUBST_FILES']:
            yield self._create_substitution(ConfigFileSubstitution, sandbox,
                path)

        for path in sandbox['CONFIGURE_DEFINE_FILES']:
            yield self._create_substitution(HeaderFileSubstitution, sandbox,
                path)

        # XPIDL source files get processed and turned into .h and .xpt files.
        # If there are multiple XPIDL files in a directory, they get linked
        # together into a final .xpt, which has the name defined by
        # XPIDL_MODULE.
        xpidl_module = sandbox['XPIDL_MODULE']

        if sandbox['XPIDL_SOURCES'] and not xpidl_module:
            raise SandboxValidationError('XPIDL_MODULE must be defined if '
                'XPIDL_SOURCES is defined.')

        if xpidl_module and not sandbox['XPIDL_SOURCES']:
            raise SandboxValidationError('XPIDL_MODULE cannot be defined '
                'unless there are XPIDL_SOURCES: %s' % sandbox['RELATIVEDIR'])

        if sandbox['XPIDL_SOURCES'] and sandbox['NO_DIST_INSTALL']:
            self.log(logging.WARN, 'mozbuild_warning', dict(
                path=sandbox.main_path),
                '{path}: NO_DIST_INSTALL has no effect on XPIDL_SOURCES.')

        for idl in sandbox['XPIDL_SOURCES']:
            yield XPIDLFile(sandbox, mozpath.join(sandbox['SRCDIR'], idl),
                xpidl_module)

        for symbol in ('SOURCES', 'HOST_SOURCES', 'UNIFIED_SOURCES'):
            for src in (sandbox[symbol] or []):
                if not os.path.exists(mozpath.join(sandbox['SRCDIR'], src)):
                    raise SandboxValidationError('Reference to a file that '
                        'doesn\'t exist in %s (%s) in %s'
                        % (symbol, src, sandbox['RELATIVEDIR']))

        if sandbox.get('LIBXUL_LIBRARY') and sandbox.get('FORCE_STATIC_LIB'):
            raise SandboxValidationError('LIBXUL_LIBRARY implies FORCE_STATIC_LIB')

        # Proxy some variables as-is until we have richer classes to represent
        # them. We should aim to keep this set small because it violates the
        # desired abstraction of the build definition away from makefiles.
        passthru = VariablePassthru(sandbox)
        varlist = [
            'ANDROID_GENERATED_RESFILES',
            'ANDROID_RES_DIRS',
            'CPP_UNIT_TESTS',
            'EXPORT_LIBRARY',
            'EXTRA_ASSEMBLER_FLAGS',
            'EXTRA_COMPILE_FLAGS',
            'EXTRA_COMPONENTS',
            'EXTRA_JS_MODULES',
            'EXTRA_PP_COMPONENTS',
            'EXTRA_PP_JS_MODULES',
            'FAIL_ON_WARNINGS',
            'FILES_PER_UNIFIED_FILE',
            'FORCE_SHARED_LIB',
            'FORCE_STATIC_LIB',
            'GENERATED_FILES',
            'HOST_LIBRARY_NAME',
            'IS_COMPONENT',
            'IS_GYP_DIR',
            'JS_MODULES_PATH',
            'LIBS',
            'LIBXUL_LIBRARY',
            'MSVC_ENABLE_PGO',
            'NO_DIST_INSTALL',
            'OS_LIBS',
            'RCFILE',
            'RESFILE',
            'SDK_LIBRARY',
        ]
        for v in varlist:
            if v in sandbox and sandbox[v]:
                passthru.variables[v] = sandbox[v]

        # NO_VISIBILITY_FLAGS is slightly different
        if sandbox['NO_VISIBILITY_FLAGS']:
            passthru.variables['VISIBILITY_FLAGS'] = ''

        if sandbox['DELAYLOAD_DLLS']:
            passthru.variables['DELAYLOAD_LDFLAGS'] = [('-DELAYLOAD:%s' % dll) for dll in sandbox['DELAYLOAD_DLLS']]
            passthru.variables['USE_DELAYIMP'] = True

        varmap = dict(
            SOURCES={
                '.s': 'ASFILES',
                '.asm': 'ASFILES',
                '.c': 'CSRCS',
                '.m': 'CMSRCS',
                '.mm': 'CMMSRCS',
                '.cc': 'CPPSRCS',
                '.cpp': 'CPPSRCS',
                '.S': 'SSRCS',
            },
            HOST_SOURCES={
                '.c': 'HOST_CSRCS',
                '.mm': 'HOST_CMMSRCS',
                '.cc': 'HOST_CPPSRCS',
                '.cpp': 'HOST_CPPSRCS',
            },
            UNIFIED_SOURCES={
                '.c': 'UNIFIED_CSRCS',
                '.mm': 'UNIFIED_CMMSRCS',
                '.cc': 'UNIFIED_CPPSRCS',
                '.cpp': 'UNIFIED_CPPSRCS',
            }
        )
        varmap.update(dict(('GENERATED_%s' % k, v) for k, v in varmap.items()
                           if k in ('SOURCES', 'UNIFIED_SOURCES')))
        for variable, mapping in varmap.items():
            for f in sandbox[variable]:
                ext = mozpath.splitext(f)[1]
                if ext not in mapping:
                    raise SandboxValidationError('%s has an unknown file type in %s' % (f, sandbox['RELATIVEDIR']))
                l = passthru.variables.setdefault(mapping[ext], [])
                l.append(f)
                if variable.startswith('GENERATED_'):
                    l = passthru.variables.setdefault('GARBAGE', [])
                    l.append(f)

        no_pgo = sandbox.get('NO_PGO')
        sources = sandbox.get('SOURCES', [])
        no_pgo_sources = [f for f in sources if sources[f].no_pgo]
        if no_pgo:
            if no_pgo_sources:
                raise SandboxValidationError('NO_PGO and SOURCES[...].no_pgo cannot be set at the same time')
            passthru.variables['NO_PROFILE_GUIDED_OPTIMIZE'] = no_pgo
        if no_pgo_sources:
            passthru.variables['NO_PROFILE_GUIDED_OPTIMIZE'] = no_pgo_sources

        exports = sandbox.get('EXPORTS')
        if exports:
            yield Exports(sandbox, exports,
                dist_install=not sandbox.get('NO_DIST_INSTALL', False))

        defines = sandbox.get('DEFINES')
        if defines:
            yield Defines(sandbox, defines)

        program = sandbox.get('PROGRAM')
        if program:
            yield Program(sandbox, program, sandbox['CONFIG']['BIN_SUFFIX'])

        program = sandbox.get('HOST_PROGRAM')
        if program:
            yield HostProgram(sandbox, program, sandbox['CONFIG']['HOST_BIN_SUFFIX'])

        for program in sandbox['SIMPLE_PROGRAMS']:
            yield SimpleProgram(sandbox, program, sandbox['CONFIG']['BIN_SUFFIX'])

        for program in sandbox['HOST_SIMPLE_PROGRAMS']:
            yield HostSimpleProgram(sandbox, program, sandbox['CONFIG']['HOST_BIN_SUFFIX'])

        simple_lists = [
            ('GENERATED_EVENTS_WEBIDL_FILES', GeneratedEventWebIDLFile),
            ('GENERATED_WEBIDL_FILES', GeneratedWebIDLFile),
            ('IPDL_SOURCES', IPDLFile),
            ('LOCAL_INCLUDES', LocalInclude),
            ('GENERATED_INCLUDES', GeneratedInclude),
            ('PREPROCESSED_TEST_WEBIDL_FILES', PreprocessedTestWebIDLFile),
            ('PREPROCESSED_WEBIDL_FILES', PreprocessedWebIDLFile),
            ('TEST_WEBIDL_FILES', TestWebIDLFile),
            ('WEBIDL_FILES', WebIDLFile),
            ('WEBIDL_EXAMPLE_INTERFACES', ExampleWebIDLInterface),
        ]
        for sandbox_var, klass in simple_lists:
            for name in sandbox.get(sandbox_var, []):
                yield klass(sandbox, name)

        if sandbox.get('FINAL_TARGET') or sandbox.get('XPI_NAME') or \
                sandbox.get('DIST_SUBDIR'):
            yield InstallationTarget(sandbox)

        libname = sandbox.get('LIBRARY_NAME')
        final_lib = sandbox.get('FINAL_LIBRARY')
        if not libname and final_lib:
            # If no LIBRARY_NAME is given, create one.
            libname = sandbox['RELATIVEDIR'].replace('/', '_')
        if libname:
            self._libs.setdefault(libname, {})[sandbox['OBJDIR']] = \
                LibraryDefinition(sandbox, libname)

        if final_lib:
            if isinstance(sandbox, MozbuildSandbox) and sandbox.get('FORCE_STATIC_LIB'):
                raise SandboxValidationError('FINAL_LIBRARY implies FORCE_STATIC_LIB')
            self._final_libs.append((sandbox['OBJDIR'], libname, final_lib))
            passthru.variables['FORCE_STATIC_LIB'] = True

        # While there are multiple test manifests, the behavior is very similar
        # across them. We enforce this by having common handling of all
        # manifests and outputting a single class type with the differences
        # described inside the instance.
        #
        # Keys are variable prefixes and values are tuples describing how these
        # manifests should be handled:
        #
        #    (flavor, install_prefix, active)
        #
        # flavor identifies the flavor of this test.
        # install_prefix is the path prefix of where to install the files in
        #     the tests directory.
        # active indicates whether to filter out inactive tests from the
        #     manifest.
        #
        # We ideally don't filter out inactive tests. However, not every test
        # harness can yet deal with test filtering. Once all harnesses can do
        # this, this feature can be dropped.
        test_manifests = dict(
            A11Y=('a11y', 'testing/mochitest/a11y', True),
            BROWSER_CHROME=('browser-chrome', 'testing/mochitest/browser', True),
            METRO_CHROME=('metro-chrome', 'testing/mochitest/metro', True),
            MOCHITEST=('mochitest', 'testing/mochitest/tests', True),
            MOCHITEST_CHROME=('chrome', 'testing/mochitest/chrome', True),
            MOCHITEST_WEBAPPRT_CHROME=('webapprt-chrome', 'testing/mochitest/webapprtChrome', True),
            WEBRTC_SIGNALLING_TEST=('steeplechase', 'steeplechase', True),
            XPCSHELL_TESTS=('xpcshell', 'xpcshell', False),
        )

        for prefix, info in test_manifests.items():
            for path in sandbox.get('%s_MANIFESTS' % prefix, []):
                for obj in self._process_test_manifest(sandbox, info, path):
                    yield obj

        jar_manifests = sandbox.get('JAR_MANIFESTS', [])
        if len(jar_manifests) > 1:
            raise SandboxValidationError('While JAR_MANIFESTS is a list, '
                'it is currently limited to one value.')

        for path in jar_manifests:
            yield JARManifest(sandbox, mozpath.join(sandbox['SRCDIR'], path))

        # Temporary test to look for jar.mn files that creep in without using
        # the new declaration. Before, we didn't require jar.mn files to
        # declared anywhere (they were discovered). This will detect people
        # relying on the old behavior.
        if os.path.exists(os.path.join(sandbox['SRCDIR'], 'jar.mn')):
            if 'jar.mn' not in jar_manifests:
                raise SandboxValidationError('A jar.mn exists in %s but it '
                    'is not referenced in the corresponding moz.build file. '
                    'Please define JAR_MANIFESTS in the moz.build file.' %
                    sandbox['SRCDIR'])

        for name, jar in sandbox.get('JAVA_JAR_TARGETS', {}).items():
            yield SandboxWrapped(sandbox, jar)

        if passthru.variables:
            yield passthru

    def _create_substitution(self, cls, sandbox, path):
        if os.path.isabs(path):
            path = path[1:]

        sub = cls(sandbox)
        sub.input_path = mozpath.join(sandbox['SRCDIR'], '%s.in' % path)
        sub.output_path = mozpath.join(sandbox['OBJDIR'], path)
        sub.relpath = path

        return sub

    def _process_test_manifest(self, sandbox, info, manifest_path):
        flavor, install_prefix, filter_inactive = info

        manifest_path = mozpath.normpath(manifest_path)
        path = mozpath.normpath(mozpath.join(sandbox['SRCDIR'], manifest_path))
        manifest_dir = mozpath.dirname(path)
        manifest_reldir = mozpath.dirname(mozpath.relpath(path,
            sandbox['TOPSRCDIR']))

        try:
            m = manifestparser.TestManifest(manifests=[path], strict=True)
            defaults = m.manifest_defaults[os.path.normpath(path)]
            if not m.tests and not 'support-files' in defaults:
                raise SandboxValidationError('Empty test manifest: %s'
                    % path)

            obj = TestManifest(sandbox, path, m, flavor=flavor,
                install_prefix=install_prefix,
                relpath=mozpath.join(manifest_reldir, mozpath.basename(path)),
                dupe_manifest='dupe-manifest' in defaults)

            filtered = m.tests

            if filter_inactive:
                filtered = m.active_tests(disabled=False, **self.mozinfo)

            out_dir = mozpath.join(install_prefix, manifest_reldir)

            # "head" and "tail" lists.
            # All manifests support support-files.
            #
            # Keep a set of already seen support file patterns, because
            # repeatedly processing the patterns from the default section
            # for every test is quite costly (see bug 922517).
            extras = (('head', set()),
                      ('tail', set()),
                      ('support-files', set()))
            def process_support_files(test):
                for thing, seen in extras:
                    value = test.get(thing, '')
                    if value in seen:
                        continue
                    seen.add(value)
                    for pattern in value.split():
                        # We only support globbing on support-files because
                        # the harness doesn't support * for head and tail.
                        if '*' in pattern and thing == 'support-files':
                            obj.pattern_installs.append(
                                (manifest_dir, pattern, out_dir))
                        else:
                            full = mozpath.normpath(mozpath.join(manifest_dir,
                                pattern))
                            # Only install paths in our directory. This
                            # rule is somewhat arbitrary and could be lifted.
                            if not full.startswith(manifest_dir):
                                continue

                            obj.installs[full] = mozpath.join(out_dir, pattern)

            for test in filtered:
                obj.tests.append(test)

                obj.installs[mozpath.normpath(test['path'])] = \
                    mozpath.join(out_dir, test['relpath'])

                process_support_files(test)

            if not m.tests:
                # If there are no tests, look for support-files under DEFAULT.
                process_support_files(defaults)

            # We also copy the manifest into the output directory.
            out_path = mozpath.join(out_dir, mozpath.basename(manifest_path))
            obj.installs[path] = out_path

            # Some manifests reference files that are auto generated as
            # part of the build or shouldn't be installed for some
            # reason. Here, we prune those files from the install set.
            # FUTURE we should be able to detect autogenerated files from
            # other build metadata. Once we do that, we can get rid of this.
            for f in defaults.get('generated-files', '').split():
                # We re-raise otherwise the stack trace isn't informative.
                try:
                    del obj.installs[mozpath.join(manifest_dir, f)]
                except KeyError:
                    raise SandboxValidationError('Error processing test '
                        'manifest %s: entry in generated-files not present '
                        'elsewhere in manifest: %s' % (path, f))

                obj.external_installs.add(mozpath.join(out_dir, f))

            yield obj
        except (AssertionError, Exception):
            raise SandboxValidationError('Error processing test '
                'manifest file %s: %s' % (path,
                    '\n'.join(traceback.format_exception(*sys.exc_info()))))

    def _emit_directory_traversal_from_sandbox(self, sandbox):
        o = DirectoryTraversal(sandbox)
        o.dirs = sandbox.get('DIRS', [])
        o.parallel_dirs = sandbox.get('PARALLEL_DIRS', [])
        o.tool_dirs = sandbox.get('TOOL_DIRS', [])
        o.test_dirs = sandbox.get('TEST_DIRS', [])
        o.test_tool_dirs = sandbox.get('TEST_TOOL_DIRS', [])
        o.is_tool_dir = sandbox.get('IS_TOOL_DIR', False)
        o.affected_tiers = sandbox.get_affected_tiers()

        if 'TIERS' in sandbox:
            for tier in sandbox['TIERS']:
                o.tier_dirs[tier] = sandbox['TIERS'][tier]['regular'] + \
                    sandbox['TIERS'][tier]['external']
                o.tier_static_dirs[tier] = sandbox['TIERS'][tier]['static']

        yield o
