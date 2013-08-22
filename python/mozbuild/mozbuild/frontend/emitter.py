# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import logging
import os

from mach.mixin.logging import LoggingMixin

import mozpack.path as mozpath

from .data import (
    ConfigFileSubstitution,
    DirectoryTraversal,
    Exports,
    IPDLFile,
    Program,
    ReaderSummary,
    VariablePassthru,
    XPIDLFile,
    XpcshellManifests,
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
            DEFINES='DEFINES',
            EXTRA_COMPONENTS='EXTRA_COMPONENTS',
            EXTRA_JS_MODULES='EXTRA_JS_MODULES',
            EXTRA_PP_COMPONENTS='EXTRA_PP_COMPONENTS',
            EXTRA_PP_JS_MODULES='EXTRA_PP_JS_MODULES',
            GTEST_CMMSRCS='GTEST_CMM_SOURCES',
            GTEST_CPPSRCS='GTEST_CPP_SOURCES',
            GTEST_CSRCS='GTEST_C_SOURCES',
            HOST_CPPSRCS='HOST_CPPSRCS',
            HOST_CSRCS='HOST_CSRCS',
            HOST_LIBRARY_NAME='HOST_LIBRARY_NAME',
            JS_MODULES_PATH='JS_MODULES_PATH',
            LIBRARY_NAME='LIBRARY_NAME',
            LIBS='LIBS',
            MODULE='MODULE',
            SDK_LIBRARY='SDK_LIBRARY',
            SHARED_LIBRARY_LIBS='SHARED_LIBRARY_LIBS',
            SIMPLE_PROGRAMS='SIMPLE_PROGRAMS',
            SSRCS='SSRCS',
        )
        for mak, moz in varmap.items():
            if sandbox[moz]:
                passthru.variables[mak] = sandbox[moz]

        if sandbox['NO_DIST_INSTALL']:
            passthru.variables['NO_DIST_INSTALL'] = '1'

        if passthru.variables:
            yield passthru

        exports = sandbox.get('EXPORTS')
        if exports:
            yield Exports(sandbox, exports)

        program = sandbox.get('PROGRAM')
        if program:
            yield Program(sandbox, program, sandbox['CONFIG']['BIN_SUFFIX'])

        for manifest in sandbox.get('XPCSHELL_TESTS_MANIFESTS', []):
            yield XpcshellManifests(sandbox, manifest)

        for ipdl in sandbox.get('IPDL_SOURCES', []):
            yield IPDLFile(sandbox, ipdl)

    def _emit_directory_traversal_from_sandbox(self, sandbox):
        o = DirectoryTraversal(sandbox)
        o.dirs = sandbox.get('DIRS', [])
        o.parallel_dirs = sandbox.get('PARALLEL_DIRS', [])
        o.tool_dirs = sandbox.get('TOOL_DIRS', [])
        o.test_dirs = sandbox.get('TEST_DIRS', [])
        o.test_tool_dirs = sandbox.get('TEST_TOOL_DIRS', [])
        o.external_make_dirs = sandbox.get('EXTERNAL_MAKE_DIRS', [])
        o.parallel_external_make_dirs = sandbox.get('PARALLEL_EXTERNAL_MAKE_DIRS', [])

        if 'TIERS' in sandbox:
            for tier in sandbox['TIERS']:
                o.tier_dirs[tier] = sandbox['TIERS'][tier]['regular']
                o.tier_static_dirs[tier] = sandbox['TIERS'][tier]['static']

        yield o
