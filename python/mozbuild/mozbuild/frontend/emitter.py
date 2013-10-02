# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import json
import logging
import os
import traceback
import sys

from mach.mixin.logging import LoggingMixin

import mozpack.path as mozpath
import manifestparser

from mozpack.files import FileFinder

from .data import (
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
    ReaderSummary,
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
        mozinfo_path = os.path.join(config.topobjdir, 'mozinfo.json')
        if os.path.exists(mozinfo_path):
            self.mozinfo = json.load(open(mozinfo_path, 'rt'))
        else:
            self.mozinfo = {}

    def emit(self, output):
        """Convert the BuildReader output into data structures.

        The return value from BuildReader.read_topsrcdir() (a generator) is
        typically fed into this function.
        """
        file_count = 0
        execution_time = 0.0

        for out in output:
            if isinstance(out, MozbuildSandbox):
                for o in self.emit_from_sandbox(out):
                    yield o

                # Update the stats.
                file_count += len(out.all_paths)
                execution_time += out.execution_time

            else:
                raise Exception('Unhandled output type: %s' % out)

        yield ReaderSummary(file_count, execution_time)

    def emit_from_sandbox(self, sandbox):
        """Convert a MozbuildSandbox to tree metadata objects.

        This is a generator of mozbuild.frontend.data.SandboxDerived instances.
        """
        # We always emit a directory traversal descriptor. This is needed by
        # the recursive make backend.
        for o in self._emit_directory_traversal_from_sandbox(sandbox): yield o

        for path in sandbox['CONFIGURE_SUBST_FILES']:
            if os.path.isabs(path):
                path = path[1:]

            sub = ConfigFileSubstitution(sandbox)
            sub.input_path = os.path.join(sandbox['SRCDIR'], '%s.in' % path)
            sub.output_path = os.path.join(sandbox['OBJDIR'], path)
            sub.relpath = path
            yield sub

        # XPIDL source files get processed and turned into .h and .xpt files.
        # If there are multiple XPIDL files in a directory, they get linked
        # together into a final .xpt, which has the name defined by either
        # MODULE or XPIDL_MODULE (if the latter is defined).
        xpidl_module = sandbox['MODULE']
        if sandbox['XPIDL_MODULE']:
            xpidl_module = sandbox['XPIDL_MODULE']

        if sandbox['XPIDL_SOURCES'] and not xpidl_module:
            raise SandboxValidationError('MODULE or XPIDL_MODULE must be '
                'defined if XPIDL_SOURCES is defined.')

        if sandbox['XPIDL_SOURCES'] and sandbox['NO_DIST_INSTALL']:
            self.log(logging.WARN, 'mozbuild_warning', dict(
                path=sandbox.main_path),
                '{path}: NO_DIST_INSTALL has no effect on XPIDL_SOURCES.')

        for idl in sandbox['XPIDL_SOURCES']:
            yield XPIDLFile(sandbox, mozpath.join(sandbox['SRCDIR'], idl),
                xpidl_module)

        # Proxy some variables as-is until we have richer classes to represent
        # them. We should aim to keep this set small because it violates the
        # desired abstraction of the build definition away from makefiles.
        passthru = VariablePassthru(sandbox)
        varmap = dict(
            # Makefile.in : moz.build
            ASFILES='ASFILES',
            CMMSRCS='CMMSRCS',
            CPPSRCS='CPP_SOURCES',
            CPP_UNIT_TESTS='CPP_UNIT_TESTS',
            CSRCS='CSRCS',
            EXPORT_LIBRARY='EXPORT_LIBRARY',
            EXTRA_COMPONENTS='EXTRA_COMPONENTS',
            EXTRA_JS_MODULES='EXTRA_JS_MODULES',
            EXTRA_PP_COMPONENTS='EXTRA_PP_COMPONENTS',
            EXTRA_PP_JS_MODULES='EXTRA_PP_JS_MODULES',
            FAIL_ON_WARNINGS='FAIL_ON_WARNINGS',
            FORCE_SHARED_LIB='FORCE_SHARED_LIB',
            FORCE_STATIC_LIB='FORCE_STATIC_LIB',
            GTEST_CMMSRCS='GTEST_CMM_SOURCES',
            GTEST_CPPSRCS='GTEST_CPP_SOURCES',
            GTEST_CSRCS='GTEST_C_SOURCES',
            HOST_CPPSRCS='HOST_CPPSRCS',
            HOST_CSRCS='HOST_CSRCS',
            HOST_LIBRARY_NAME='HOST_LIBRARY_NAME',
            IS_COMPONENT='IS_COMPONENT',
            JS_MODULES_PATH='JS_MODULES_PATH',
            LIBRARY_NAME='LIBRARY_NAME',
            LIBS='LIBS',
            LIBXUL_LIBRARY='LIBXUL_LIBRARY',
            MODULE='MODULE',
            MSVC_ENABLE_PGO='MSVC_ENABLE_PGO',
            NO_DIST_INSTALL='NO_DIST_INSTALL',
            OS_LIBS='OS_LIBS',
            SDK_LIBRARY='SDK_LIBRARY',
            SHARED_LIBRARY_LIBS='SHARED_LIBRARY_LIBS',
            SIMPLE_PROGRAMS='SIMPLE_PROGRAMS',
            SSRCS='SSRCS',
        )
        for mak, moz in varmap.items():
            if sandbox[moz]:
                passthru.variables[mak] = sandbox[moz]

        if passthru.variables:
            yield passthru

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

        simple_lists = [
            ('GENERATED_EVENTS_WEBIDL_FILES', GeneratedEventWebIDLFile),
            ('GENERATED_WEBIDL_FILES', GeneratedWebIDLFile),
            ('IPDL_SOURCES', IPDLFile),
            ('LOCAL_INCLUDES', LocalInclude),
            ('PREPROCESSED_TEST_WEBIDL_FILES', PreprocessedTestWebIDLFile),
            ('PREPROCESSED_WEBIDL_FILES', PreprocessedWebIDLFile),
            ('TEST_WEBIDL_FILES', TestWebIDLFile),
            ('WEBIDL_FILES', WebIDLFile),
        ]
        for sandbox_var, klass in simple_lists:
            for name in sandbox.get(sandbox_var, []):
                yield klass(sandbox, name)

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
            MOCHITEST=('mochitest', 'testing/mochitest/tests', True),
            MOCHITEST_CHROME=('chrome', 'testing/mochitest/chrome', True),
            WEBRTC_SIGNALLING_TEST=('steeplechase', 'steeplechase', True),
            XPCSHELL_TESTS=('xpcshell', 'xpcshell', False),
        )

        for prefix, info in test_manifests.items():
            for path in sandbox.get('%s_MANIFESTS' % prefix, []):
                for obj in self._process_test_manifest(sandbox, info, path):
                    yield obj

    def _process_test_manifest(self, sandbox, info, manifest_path):
        flavor, install_prefix, filter_inactive = info

        manifest_path = os.path.normpath(manifest_path)
        path = mozpath.normpath(mozpath.join(sandbox['SRCDIR'], manifest_path))
        manifest_dir = mozpath.dirname(path)
        manifest_reldir = mozpath.dirname(mozpath.relpath(path,
            sandbox['TOPSRCDIR']))

        try:
            m = manifestparser.TestManifest(manifests=[path], strict=True)

            if not m.tests:
                raise SandboxValidationError('Empty test manifest: %s'
                    % path)

            obj = TestManifest(sandbox, path, m, flavor=flavor,
                install_prefix=install_prefix,
                relpath=mozpath.join(manifest_reldir, mozpath.basename(path)),
                dupe_manifest='dupe-manifest' in m.tests[0])

            filtered = m.tests

            if filter_inactive:
                filtered = m.active_tests(disabled=False, **self.mozinfo)

            out_dir = mozpath.join(install_prefix, manifest_reldir)

            finder = FileFinder(base=manifest_dir, find_executables=False)

            for test in filtered:
                obj.installs[mozpath.normpath(test['path'])] = \
                    mozpath.join(out_dir, test['relpath'])

                # xpcshell defines extra files to install in the
                # "head" and "tail" lists.
                # All manifests support support-files.
                for thing in ('head', 'tail', 'support-files'):
                    for pattern in test.get(thing, '').split():
                        # We only support globbing on support-files because
                        # the harness doesn't support * for head and tail.
                        #
                        # While we could feed everything through the finder, we
                        # don't because we want explicitly listed files that
                        # no longer exist to raise an error. The finder is also
                        # slower than simple lookup.
                        if '*' in pattern and thing == 'support-files':
                            paths = [f[0] for f in finder.find(pattern)]
                            if not paths:
                                raise SandboxValidationError('%s support-files '
                                    'wildcard in %s returns no results.' % (
                                    pattern, path))

                            for f in paths:
                                full = mozpath.normpath(mozpath.join(manifest_dir, f))
                                obj.installs[full] = mozpath.join(out_dir, f)

                        else:
                            full = mozpath.normpath(mozpath.join(manifest_dir,
                                pattern))
                            # Only install paths in our directory. This
                            # rule is somewhat arbitrary and could be lifted.
                            if not full.startswith(manifest_dir):
                                continue

                            obj.installs[full] = mozpath.join(out_dir, pattern)

            # We also copy the manifest into the output directory.
            out_path = mozpath.join(out_dir, os.path.basename(manifest_path))
            obj.installs[path] = out_path

            # Some manifests reference files that are auto generated as
            # part of the build or shouldn't be installed for some
            # reason. Here, we prune those files from the install set.
            # FUTURE we should be able to detect autogenerated files from
            # other build metadata. Once we do that, we can get rid of this.
            for f in m.tests[0].get('generated-files', '').split():
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
        o.external_make_dirs = sandbox.get('EXTERNAL_MAKE_DIRS', [])
        o.parallel_external_make_dirs = sandbox.get('PARALLEL_EXTERNAL_MAKE_DIRS', [])
        o.is_tool_dir = sandbox.get('IS_TOOL_DIR', False)
        o.affected_tiers = sandbox.get_affected_tiers()

        if 'TIERS' in sandbox:
            for tier in sandbox['TIERS']:
                o.tier_dirs[tier] = sandbox['TIERS'][tier]['regular'] + \
                    sandbox['TIERS'][tier]['external']
                o.tier_static_dirs[tier] = sandbox['TIERS'][tier]['static']

        yield o
