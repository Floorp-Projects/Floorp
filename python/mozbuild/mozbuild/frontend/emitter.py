# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import itertools
import logging
import os
import traceback
import sys
import time

from collections import defaultdict, OrderedDict
from mach.mixin.logging import LoggingMixin
from mozbuild.util import (
    memoize,
    OrderedDefaultDict,
)

import mozpack.path as mozpath
import mozinfo
import pytoml

from .data import (
    AndroidAssetsDirs,
    AndroidExtraPackages,
    AndroidExtraResDirs,
    AndroidResDirs,
    BaseSources,
    BrandingFiles,
    ChromeManifestEntry,
    ConfigFileSubstitution,
    ContextWrapped,
    Defines,
    DirectoryTraversal,
    Exports,
    FinalTargetFiles,
    FinalTargetPreprocessedFiles,
    GeneratedEventWebIDLFile,
    GeneratedFile,
    GeneratedSources,
    GeneratedWebIDLFile,
    ExampleWebIDLInterface,
    ExternalStaticLibrary,
    ExternalSharedLibrary,
    HostDefines,
    HostLibrary,
    HostProgram,
    HostSimpleProgram,
    HostSources,
    InstallationTarget,
    IPDLFile,
    JARManifest,
    Library,
    Linkable,
    LocalInclude,
    ObjdirFiles,
    ObjdirPreprocessedFiles,
    PerSourceFlag,
    PreprocessedTestWebIDLFile,
    PreprocessedWebIDLFile,
    Program,
    RustLibrary,
    SdkFiles,
    SharedLibrary,
    SimpleProgram,
    Sources,
    StaticLibrary,
    TestHarnessFiles,
    TestWebIDLFile,
    TestManifest,
    UnifiedSources,
    VariablePassthru,
    WebIDLFile,
    XPIDLFile,
)
from mozpack.chrome.manifest import (
    ManifestBinaryComponent,
    Manifest,
)

from .reader import SandboxValidationError

from ..testing import (
    TEST_MANIFESTS,
    REFTEST_FLAVORS,
    WEB_PLATFORM_TESTS_FLAVORS,
    SupportFilesConverter,
)

from .context import (
    Context,
    SourcePath,
    ObjDirPath,
    Path,
    SubContext,
    TemplateContext,
)

from mozbuild.base import ExecutionSummary


ALLOWED_XPCOM_GLUE = {
    ('TestStreamConv', 'netwerk/streamconv/test'),
    ('PropertiesTest', 'netwerk/test'),
    ('ReadNTLM', 'netwerk/test'),
    ('TestBlockingSocket', 'netwerk/test'),
    ('TestDNS', 'netwerk/test'),
    ('TestIncrementalDownload', 'netwerk/test'),
    ('TestNamedPipeService', 'netwerk/test'),
    ('TestOpen', 'netwerk/test'),
    ('TestProtocols', 'netwerk/test'),
    ('TestServ', 'netwerk/test'),
    ('TestStreamLoader', 'netwerk/test'),
    ('TestUpload', 'netwerk/test'),
    ('TestURLParser', 'netwerk/test'),
    ('urltest', 'netwerk/test'),
    ('TestBind', 'netwerk/test'),
    ('TestCookie', 'netwerk/test'),
    ('TestUDPSocket', 'netwerk/test'),
    ('xpcshell', 'js/xpconnect/shell'),
    ('test_AsXXX_helpers', 'storage/test'),
    ('test_async_callbacks_with_spun_event_loops', 'storage/test'),
    ('test_asyncStatementExecution_transaction', 'storage/test'),
    ('test_binding_params', 'storage/test'),
    ('test_deadlock_detector', 'storage/test'),
    ('test_file_perms', 'storage/test'),
    ('test_mutex', 'storage/test'),
    ('test_service_init_background_thread', 'storage/test'),
    ('test_statement_scoper', 'storage/test'),
    ('test_StatementCache', 'storage/test'),
    ('test_transaction_helper', 'storage/test'),
    ('test_true_async', 'storage/test'),
    ('test_unlock_notify', 'storage/test'),
    ('testcrasher', 'toolkit/crashreporter/test'),
    ('mediaconduit_unittests', 'media/webrtc/signaling/test'),
    ('mediapipeline_unittest', 'media/webrtc/signaling/test'),
    ('sdp_file_parser', 'media/webrtc/signaling/fuzztest'),
    ('signaling_unittests', 'media/webrtc/signaling/test'),
    ('TestMailCookie', 'mailnews/base/test'),
    ('calbasecomps', 'calendar/base/backend/libical/build'),
    ('purplexpcom', 'extensions/purple/purplexpcom/src'),
}


class TreeMetadataEmitter(LoggingMixin):
    """Converts the executed mozbuild files into data structures.

    This is a bridge between reader.py and data.py. It takes what was read by
    reader.BuildReader and converts it into the classes defined in the data
    module.
    """

    def __init__(self, config):
        self.populate_logger()

        self.config = config

        mozinfo.find_and_update_from_json(config.topobjdir)

        # Python 2.6 doesn't allow unicode keys to be used for keyword
        # arguments. This gross hack works around the problem until we
        # rid ourselves of 2.6.
        self.info = {}
        for k, v in mozinfo.info.items():
            if isinstance(k, unicode):
                k = k.encode('ascii')
            self.info[k] = v

        self._libs = OrderedDefaultDict(list)
        self._binaries = OrderedDict()
        self._linkage = []
        self._static_linking_shared = set()
        self._crate_verified_local = set()
        self._crate_directories = dict()

        # Keep track of external paths (third party build systems), starting
        # from what we run a subconfigure in. We'll eliminate some directories
        # as we traverse them with moz.build (e.g. js/src).
        subconfigures = os.path.join(self.config.topobjdir, 'subconfigures')
        paths = []
        if os.path.exists(subconfigures):
            paths = open(subconfigures).read().splitlines()
        self._external_paths = set(mozpath.normsep(d) for d in paths)

        self._emitter_time = 0.0
        self._object_count = 0
        self._test_files_converter = SupportFilesConverter()

    def summary(self):
        return ExecutionSummary(
            'Processed into {object_count:d} build config descriptors in '
            '{execution_time:.2f}s',
            execution_time=self._emitter_time,
            object_count=self._object_count)

    def emit(self, output):
        """Convert the BuildReader output into data structures.

        The return value from BuildReader.read_topsrcdir() (a generator) is
        typically fed into this function.
        """
        contexts = {}

        def emit_objs(objs):
            for o in objs:
                self._object_count += 1
                yield o

        for out in output:
            # Nothing in sub-contexts is currently of interest to us. Filter
            # them all out.
            if isinstance(out, SubContext):
                continue

            if isinstance(out, Context):
                # Keep all contexts around, we will need them later.
                contexts[out.objdir] = out

                start = time.time()
                # We need to expand the generator for the timings to work.
                objs = list(self.emit_from_context(out))
                self._emitter_time += time.time() - start

                for o in emit_objs(objs): yield o

            else:
                raise Exception('Unhandled output type: %s' % type(out))

        # Don't emit Linkable objects when COMPILE_ENVIRONMENT is not set
        if self.config.substs.get('COMPILE_ENVIRONMENT'):
            start = time.time()
            objs = list(self._emit_libs_derived(contexts))
            self._emitter_time += time.time() - start

            for o in emit_objs(objs): yield o

    def _emit_libs_derived(self, contexts):
        # First do FINAL_LIBRARY linkage.
        for lib in (l for libs in self._libs.values() for l in libs):
            if not isinstance(lib, (StaticLibrary, RustLibrary)) or not lib.link_into:
                continue
            if lib.link_into not in self._libs:
                raise SandboxValidationError(
                    'FINAL_LIBRARY ("%s") does not match any LIBRARY_NAME'
                    % lib.link_into, contexts[lib.objdir])
            candidates = self._libs[lib.link_into]

            # When there are multiple candidates, but all are in the same
            # directory and have a different type, we want all of them to
            # have the library linked. The typical usecase is when building
            # both a static and a shared library in a directory, and having
            # that as a FINAL_LIBRARY.
            if len(set(type(l) for l in candidates)) == len(candidates) and \
                   len(set(l.objdir for l in candidates)) == 1:
                for c in candidates:
                    c.link_library(lib)
            else:
                raise SandboxValidationError(
                    'FINAL_LIBRARY ("%s") matches a LIBRARY_NAME defined in '
                    'multiple places:\n    %s' % (lib.link_into,
                    '\n    '.join(l.objdir for l in candidates)),
                    contexts[lib.objdir])

        # Next, USE_LIBS linkage.
        for context, obj, variable in self._linkage:
            self._link_libraries(context, obj, variable)

        def recurse_refs(lib):
            for o in lib.refs:
                yield o
                if isinstance(o, StaticLibrary):
                    for q in recurse_refs(o):
                        yield q

        # Check that all static libraries refering shared libraries in
        # USE_LIBS are linked into a shared library or program.
        for lib in self._static_linking_shared:
            if all(isinstance(o, StaticLibrary) for o in recurse_refs(lib)):
                shared_libs = sorted(l.basename for l in lib.linked_libraries
                    if isinstance(l, SharedLibrary))
                raise SandboxValidationError(
                    'The static "%s" library is not used in a shared library '
                    'or a program, but USE_LIBS contains the following shared '
                    'library names:\n    %s\n\nMaybe you can remove the '
                    'static "%s" library?' % (lib.basename,
                    '\n    '.join(shared_libs), lib.basename),
                    contexts[lib.objdir])

        # Propagate LIBRARY_DEFINES to all child libraries recursively.
        def propagate_defines(outerlib, defines):
            outerlib.lib_defines.update(defines)
            for lib in outerlib.linked_libraries:
                # Propagate defines only along FINAL_LIBRARY paths, not USE_LIBS
                # paths.
                if (isinstance(lib, StaticLibrary) and
                        lib.link_into == outerlib.basename):
                    propagate_defines(lib, defines)

        for lib in (l for libs in self._libs.values() for l in libs):
            if isinstance(lib, Library):
                propagate_defines(lib, lib.lib_defines)
            yield lib

        for obj in self._binaries.values():
            yield obj

    LIBRARY_NAME_VAR = {
        'host': 'HOST_LIBRARY_NAME',
        'target': 'LIBRARY_NAME',
    }

    def _link_libraries(self, context, obj, variable):
        """Add linkage declarations to a given object."""
        assert isinstance(obj, Linkable)

        use_xpcom = False

        for path in context.get(variable, []):
            force_static = path.startswith('static:') and obj.KIND == 'target'
            if force_static:
                path = path[7:]
            name = mozpath.basename(path)
            if name in ('xpcomglue', 'xpcomglue_s'):
                use_xpcom = True
            dir = mozpath.dirname(path)
            candidates = [l for l in self._libs[name] if l.KIND == obj.KIND]
            if dir:
                if dir.startswith('/'):
                    dir = mozpath.normpath(
                        mozpath.join(obj.topobjdir, dir[1:]))
                else:
                    dir = mozpath.normpath(
                        mozpath.join(obj.objdir, dir))
                dir = mozpath.relpath(dir, obj.topobjdir)
                candidates = [l for l in candidates if l.relobjdir == dir]
                if not candidates:
                    # If the given directory is under one of the external
                    # (third party) paths, use a fake library reference to
                    # there.
                    for d in self._external_paths:
                        if dir.startswith('%s/' % d):
                            candidates = [self._get_external_library(dir, name,
                                force_static)]
                            break

                if not candidates:
                    raise SandboxValidationError(
                        '%s contains "%s", but there is no "%s" %s in %s.'
                        % (variable, path, name,
                        self.LIBRARY_NAME_VAR[obj.KIND], dir), context)

            if len(candidates) > 1:
                # If there's more than one remaining candidate, it could be
                # that there are instances for the same library, in static and
                # shared form.
                libs = {}
                for l in candidates:
                    key = mozpath.join(l.relobjdir, l.basename)
                    if force_static:
                        if isinstance(l, StaticLibrary):
                            libs[key] = l
                    else:
                        if key in libs and isinstance(l, SharedLibrary):
                            libs[key] = l
                        if key not in libs:
                            libs[key] = l
                candidates = libs.values()
                if force_static and not candidates:
                    if dir:
                        raise SandboxValidationError(
                            '%s contains "static:%s", but there is no static '
                            '"%s" %s in %s.' % (variable, path, name,
                            self.LIBRARY_NAME_VAR[obj.KIND], dir), context)
                    raise SandboxValidationError(
                        '%s contains "static:%s", but there is no static "%s" '
                        '%s in the tree' % (variable, name, name,
                        self.LIBRARY_NAME_VAR[obj.KIND]), context)

            if not candidates:
                raise SandboxValidationError(
                    '%s contains "%s", which does not match any %s in the tree.'
                    % (variable, path, self.LIBRARY_NAME_VAR[obj.KIND]),
                    context)

            elif len(candidates) > 1:
                paths = (mozpath.join(l.relativedir, 'moz.build')
                    for l in candidates)
                raise SandboxValidationError(
                    '%s contains "%s", which matches a %s defined in multiple '
                    'places:\n    %s' % (variable, path,
                    self.LIBRARY_NAME_VAR[obj.KIND],
                    '\n    '.join(paths)), context)

            elif force_static and not isinstance(candidates[0], StaticLibrary):
                raise SandboxValidationError(
                    '%s contains "static:%s", but there is only a shared "%s" '
                    'in %s. You may want to add FORCE_STATIC_LIB=True in '
                    '%s/moz.build, or remove "static:".' % (variable, path,
                    name, candidates[0].relobjdir, candidates[0].relobjdir),
                    context)

            elif isinstance(obj, StaticLibrary) and isinstance(candidates[0],
                    SharedLibrary):
                self._static_linking_shared.add(obj)
            obj.link_library(candidates[0])

        # Link system libraries from OS_LIBS/HOST_OS_LIBS.
        for lib in context.get(variable.replace('USE', 'OS'), []):
            obj.link_system_library(lib)

        key = (obj.name, obj.relativedir)
        substs = context.config.substs
        extra_allowed = []
        moz_build_app = substs.get('MOZ_BUILD_APP')
        if moz_build_app is not None: # None during some test_emitter.py tests.
            if moz_build_app.startswith('../'):
                # For comm-central builds, where topsrcdir is not the root
                # source dir.
                moz_build_app = moz_build_app[3:]
            extra_allowed = [
                (substs.get('MOZ_APP_NAME'), '%s/app' % moz_build_app),
                ('%s-bin' % substs.get('MOZ_APP_NAME'), '%s/app' % moz_build_app),
            ]
        if substs.get('MOZ_WIDGET_TOOLKIT') != 'android':
            extra_allowed.append((substs.get('MOZ_CHILD_PROCESS_NAME'), 'ipc/app'))

        if key in ALLOWED_XPCOM_GLUE or key in extra_allowed:
            if not use_xpcom:
                raise SandboxValidationError(
                    "%s is in the exception list for XPCOM glue dependency but "
                    "doesn't depend on the XPCOM glue. Please adjust the list "
                    "in %s." % (obj.name, __file__), context
                )
        elif use_xpcom:
            raise SandboxValidationError(
                "%s depends on the XPCOM glue. "
                "No new dependency on the XPCOM glue is allowed."
                % obj.name, context
            )

    @memoize
    def _get_external_library(self, dir, name, force_static):
        # Create ExternalStaticLibrary or ExternalSharedLibrary object with a
        # context more or less truthful about where the external library is.
        context = Context(config=self.config)
        context.add_source(mozpath.join(self.config.topsrcdir, dir, 'dummy'))
        if force_static:
            return ExternalStaticLibrary(context, name)
        else:
            return ExternalSharedLibrary(context, name)

    def _parse_cargo_file(self, toml_file):
        """Parse toml_file and return a Python object representation of it."""
        with open(toml_file, 'r') as f:
            return pytoml.load(f)

    def _verify_deps(self, context, crate_dir, crate_name, dependencies, description='Dependency'):
        """Verify that a crate's dependencies all specify local paths."""
        for dep_crate_name, values in dependencies.iteritems():
            # A simple version number.
            if isinstance(values, (str, unicode)):
                raise SandboxValidationError(
                    '%s %s of crate %s does not list a path' % (description, dep_crate_name, crate_name),
                    context)

            dep_path = values.get('path', None)
            if not dep_path:
                raise SandboxValidationError(
                    '%s %s of crate %s does not list a path' % (description, dep_crate_name, crate_name),
                    context)

            # Try to catch the case where somebody listed a
            # local path for development.
            if os.path.isabs(dep_path):
                raise SandboxValidationError(
                    '%s %s of crate %s has a non-relative path' % (description, dep_crate_name, crate_name),
                    context)

            if not os.path.exists(mozpath.join(context.config.topsrcdir, crate_dir, dep_path)):
                raise SandboxValidationError(
                    '%s %s of crate %s refers to a non-existent path' % (description, dep_crate_name, crate_name),
                    context)

    def _rust_library(self, context, libname, static_args):
        # We need to note any Rust library for linking purposes.
        cargo_file = mozpath.join(context.srcdir, 'Cargo.toml')
        if not os.path.exists(cargo_file):
            raise SandboxValidationError(
                'No Cargo.toml file found in %s' % cargo_file, context)

        config = self._parse_cargo_file(cargo_file)
        crate_name = config['package']['name']

        if crate_name != libname:
            raise SandboxValidationError(
                'library %s does not match Cargo.toml-defined package %s' % (libname, crate_name),
                context)

        # Check that the [lib.crate-type] field is correct
        lib_section = config.get('lib', None)
        if not lib_section:
            raise SandboxValidationError(
                'Cargo.toml for %s has no [lib] section' % libname,
                context)

        crate_type = lib_section.get('crate-type', None)
        if not crate_type:
            raise SandboxValidationError(
                'Can\'t determine a crate-type for %s from Cargo.toml' % libname,
                context)

        crate_type = crate_type[0]
        if crate_type != 'staticlib':
            raise SandboxValidationError(
                'crate-type %s is not permitted for %s' % (crate_type, libname),
                context)

        # Check that the [profile.{dev,release}.panic] field is "abort"
        profile_section = config.get('profile', None)
        if not profile_section:
            raise SandboxValidationError(
                'Cargo.toml for %s has no [profile] section' % libname,
                context)

        for profile_name in ['dev', 'release']:
            profile = profile_section.get(profile_name, None)
            if not profile:
                raise SandboxValidationError(
                    'Cargo.toml for %s has no [profile.%s] section' % (libname, profile_name),
                    context)

            panic = profile.get('panic', None)
            if panic != 'abort':
                raise SandboxValidationError(
                    ('Cargo.toml for %s does not specify `panic = "abort"`'
                     ' in [profile.%s] section') % (libname, profile_name),
                    context)

        dependencies = set(config.get('dependencies', {}).iterkeys())

        return RustLibrary(context, libname, cargo_file, crate_type,
                           dependencies, **static_args)

    def _handle_linkables(self, context, passthru, generated_files):
        linkables = []
        host_linkables = []
        def add_program(prog, var):
            if var.startswith('HOST_'):
                host_linkables.append(prog)
            else:
                linkables.append(prog)

        for kind, cls in [('PROGRAM', Program), ('HOST_PROGRAM', HostProgram)]:
            program = context.get(kind)
            if program:
                if program in self._binaries:
                    raise SandboxValidationError(
                        'Cannot use "%s" as %s name, '
                        'because it is already used in %s' % (program, kind,
                        self._binaries[program].relativedir), context)
                self._binaries[program] = cls(context, program)
                self._linkage.append((context, self._binaries[program],
                    kind.replace('PROGRAM', 'USE_LIBS')))
                add_program(self._binaries[program], kind)

        for kind, cls in [
                ('SIMPLE_PROGRAMS', SimpleProgram),
                ('CPP_UNIT_TESTS', SimpleProgram),
                ('HOST_SIMPLE_PROGRAMS', HostSimpleProgram)]:
            for program in context[kind]:
                if program in self._binaries:
                    raise SandboxValidationError(
                        'Cannot use "%s" in %s, '
                        'because it is already used in %s' % (program, kind,
                        self._binaries[program].relativedir), context)
                self._binaries[program] = cls(context, program,
                    is_unit_test=kind == 'CPP_UNIT_TESTS')
                self._linkage.append((context, self._binaries[program],
                    'HOST_USE_LIBS' if kind == 'HOST_SIMPLE_PROGRAMS'
                    else 'USE_LIBS'))
                add_program(self._binaries[program], kind)

        host_libname = context.get('HOST_LIBRARY_NAME')
        libname = context.get('LIBRARY_NAME')

        if host_libname:
            if host_libname == libname:
                raise SandboxValidationError('LIBRARY_NAME and '
                    'HOST_LIBRARY_NAME must have a different value', context)
            lib = HostLibrary(context, host_libname)
            self._libs[host_libname].append(lib)
            self._linkage.append((context, lib, 'HOST_USE_LIBS'))
            host_linkables.append(lib)

        final_lib = context.get('FINAL_LIBRARY')
        if not libname and final_lib:
            # If no LIBRARY_NAME is given, create one.
            libname = context.relsrcdir.replace('/', '_')

        static_lib = context.get('FORCE_STATIC_LIB')
        shared_lib = context.get('FORCE_SHARED_LIB')

        static_name = context.get('STATIC_LIBRARY_NAME')
        shared_name = context.get('SHARED_LIBRARY_NAME')

        is_framework = context.get('IS_FRAMEWORK')
        is_component = context.get('IS_COMPONENT')

        soname = context.get('SONAME')

        lib_defines = context.get('LIBRARY_DEFINES')

        shared_args = {}
        static_args = {}

        if final_lib:
            if static_lib:
                raise SandboxValidationError(
                    'FINAL_LIBRARY implies FORCE_STATIC_LIB. '
                    'Please remove the latter.', context)
            if shared_lib:
                raise SandboxValidationError(
                    'FINAL_LIBRARY conflicts with FORCE_SHARED_LIB. '
                    'Please remove one.', context)
            if is_framework:
                raise SandboxValidationError(
                    'FINAL_LIBRARY conflicts with IS_FRAMEWORK. '
                    'Please remove one.', context)
            if is_component:
                raise SandboxValidationError(
                    'FINAL_LIBRARY conflicts with IS_COMPONENT. '
                    'Please remove one.', context)
            static_args['link_into'] = final_lib
            static_lib = True

        if libname:
            if is_component:
                if static_lib:
                    raise SandboxValidationError(
                        'IS_COMPONENT conflicts with FORCE_STATIC_LIB. '
                        'Please remove one.', context)
                shared_lib = True
                shared_args['variant'] = SharedLibrary.COMPONENT

            if is_framework:
                if soname:
                    raise SandboxValidationError(
                        'IS_FRAMEWORK conflicts with SONAME. '
                        'Please remove one.', context)
                shared_lib = True
                shared_args['variant'] = SharedLibrary.FRAMEWORK

            if not static_lib and not shared_lib:
                static_lib = True

            if static_name:
                if not static_lib:
                    raise SandboxValidationError(
                        'STATIC_LIBRARY_NAME requires FORCE_STATIC_LIB',
                        context)
                static_args['real_name'] = static_name

            if shared_name:
                if not shared_lib:
                    raise SandboxValidationError(
                        'SHARED_LIBRARY_NAME requires FORCE_SHARED_LIB',
                        context)
                shared_args['real_name'] = shared_name

            if soname:
                if not shared_lib:
                    raise SandboxValidationError(
                        'SONAME requires FORCE_SHARED_LIB', context)
                shared_args['soname'] = soname

            # If both a shared and a static library are created, only the
            # shared library is meant to be a SDK library.
            if context.get('SDK_LIBRARY'):
                if shared_lib:
                    shared_args['is_sdk'] = True
                elif static_lib:
                    static_args['is_sdk'] = True

            if context.get('NO_EXPAND_LIBS'):
                if not static_lib:
                    raise SandboxValidationError(
                        'NO_EXPAND_LIBS can only be set for static libraries.',
                        context)
                static_args['no_expand_lib'] = True

            if shared_lib and static_lib:
                if not static_name and not shared_name:
                    raise SandboxValidationError(
                        'Both FORCE_STATIC_LIB and FORCE_SHARED_LIB are True, '
                        'but neither STATIC_LIBRARY_NAME or '
                        'SHARED_LIBRARY_NAME is set. At least one is required.',
                        context)
                if static_name and not shared_name and static_name == libname:
                    raise SandboxValidationError(
                        'Both FORCE_STATIC_LIB and FORCE_SHARED_LIB are True, '
                        'but STATIC_LIBRARY_NAME is the same as LIBRARY_NAME, '
                        'and SHARED_LIBRARY_NAME is unset. Please either '
                        'change STATIC_LIBRARY_NAME or LIBRARY_NAME, or set '
                        'SHARED_LIBRARY_NAME.', context)
                if shared_name and not static_name and shared_name == libname:
                    raise SandboxValidationError(
                        'Both FORCE_STATIC_LIB and FORCE_SHARED_LIB are True, '
                        'but SHARED_LIBRARY_NAME is the same as LIBRARY_NAME, '
                        'and STATIC_LIBRARY_NAME is unset. Please either '
                        'change SHARED_LIBRARY_NAME or LIBRARY_NAME, or set '
                        'STATIC_LIBRARY_NAME.', context)
                if shared_name and static_name and shared_name == static_name:
                    raise SandboxValidationError(
                        'Both FORCE_STATIC_LIB and FORCE_SHARED_LIB are True, '
                        'but SHARED_LIBRARY_NAME is the same as '
                        'STATIC_LIBRARY_NAME. Please change one of them.',
                        context)

            symbols_file = context.get('SYMBOLS_FILE')
            if symbols_file:
                if not shared_lib:
                    raise SandboxValidationError(
                        'SYMBOLS_FILE can only be used with a SHARED_LIBRARY.',
                        context)
                if context.get('DEFFILE') or context.get('LD_VERSION_SCRIPT'):
                    raise SandboxValidationError(
                        'SYMBOLS_FILE cannot be used along DEFFILE or '
                        'LD_VERSION_SCRIPT.', context)
                if isinstance(symbols_file, SourcePath):
                    if not os.path.exists(symbols_file.full_path):
                        raise SandboxValidationError(
                            'Path specified in SYMBOLS_FILE does not exist: %s '
                            '(resolved to %s)' % (symbols_file,
                            symbols_file.full_path), context)
                    shared_args['symbols_file'] = True
                else:
                    if symbols_file.target_basename not in generated_files:
                        raise SandboxValidationError(
                            ('Objdir file specified in SYMBOLS_FILE not in ' +
                             'GENERATED_FILES: %s') % (symbols_file,), context)
                    shared_args['symbols_file'] = symbols_file.target_basename

            if shared_lib:
                lib = SharedLibrary(context, libname, **shared_args)
                self._libs[libname].append(lib)
                self._linkage.append((context, lib, 'USE_LIBS'))
                linkables.append(lib)
                generated_files.add(lib.lib_name)
                if is_component and not context['NO_COMPONENTS_MANIFEST']:
                    yield ChromeManifestEntry(context,
                        'components/components.manifest',
                        ManifestBinaryComponent('components', lib.lib_name))
                if symbols_file and isinstance(symbols_file, SourcePath):
                    script = mozpath.join(
                        mozpath.dirname(mozpath.dirname(__file__)),
                        'action', 'generate_symbols_file.py')
                    defines = ()
                    if lib.defines:
                        defines = lib.defines.get_defines()
                    yield GeneratedFile(context, script,
                        'generate_symbols_file', lib.symbols_file,
                        [symbols_file], defines)
            if static_lib:
                is_rust_library = context.get('IS_RUST_LIBRARY')
                if is_rust_library:
                    lib = self._rust_library(context, libname, static_args)
                else:
                    lib = StaticLibrary(context, libname, **static_args)
                self._libs[libname].append(lib)
                self._linkage.append((context, lib, 'USE_LIBS'))
                linkables.append(lib)

            if lib_defines:
                if not libname:
                    raise SandboxValidationError('LIBRARY_DEFINES needs a '
                        'LIBRARY_NAME to take effect', context)
                lib.lib_defines.update(lib_defines)

        # Only emit sources if we have linkables defined in the same context.
        # Note the linkables are not emitted in this function, but much later,
        # after aggregation (because of e.g. USE_LIBS processing).
        if not (linkables or host_linkables):
            return

        sources = defaultdict(list)
        gen_sources = defaultdict(list)
        all_flags = {}
        for symbol in ('SOURCES', 'HOST_SOURCES', 'UNIFIED_SOURCES'):
            srcs = sources[symbol]
            gen_srcs = gen_sources[symbol]
            context_srcs = context.get(symbol, [])
            for f in context_srcs:
                full_path = f.full_path
                if isinstance(f, SourcePath):
                    srcs.append(full_path)
                else:
                    assert isinstance(f, Path)
                    gen_srcs.append(full_path)
                if symbol == 'SOURCES':
                    flags = context_srcs[f]
                    if flags:
                        all_flags[full_path] = flags

                if isinstance(f, SourcePath) and not os.path.exists(full_path):
                    raise SandboxValidationError('File listed in %s does not '
                        'exist: \'%s\'' % (symbol, full_path), context)

        # HOST_SOURCES and UNIFIED_SOURCES only take SourcePaths, so
        # there should be no generated source in here
        assert not gen_sources['HOST_SOURCES']
        assert not gen_sources['UNIFIED_SOURCES']

        no_pgo = context.get('NO_PGO')
        no_pgo_sources = [f for f, flags in all_flags.iteritems()
                          if flags.no_pgo]
        if no_pgo:
            if no_pgo_sources:
                raise SandboxValidationError('NO_PGO and SOURCES[...].no_pgo '
                    'cannot be set at the same time', context)
            passthru.variables['NO_PROFILE_GUIDED_OPTIMIZE'] = no_pgo
        if no_pgo_sources:
            passthru.variables['NO_PROFILE_GUIDED_OPTIMIZE'] = no_pgo_sources

        # A map from "canonical suffixes" for a particular source file
        # language to the range of suffixes associated with that language.
        #
        # We deliberately don't list the canonical suffix in the suffix list
        # in the definition; we'll add it in programmatically after defining
        # things.
        suffix_map = {
            '.s': set(['.asm']),
            '.c': set(),
            '.m': set(),
            '.mm': set(),
            '.cpp': set(['.cc', '.cxx']),
            '.S': set(),
        }

        # The inverse of the above, mapping suffixes to their canonical suffix.
        canonicalized_suffix_map = {}
        for suffix, alternatives in suffix_map.iteritems():
            alternatives.add(suffix)
            for a in alternatives:
                canonicalized_suffix_map[a] = suffix

        def canonical_suffix_for_file(f):
            return canonicalized_suffix_map[mozpath.splitext(f)[1]]

        # A map from moz.build variables to the canonical suffixes of file
        # kinds that can be listed therein.
        all_suffixes = list(suffix_map.keys())
        varmap = dict(
            SOURCES=(Sources, GeneratedSources, all_suffixes),
            HOST_SOURCES=(HostSources, None, ['.c', '.mm', '.cpp']),
            UNIFIED_SOURCES=(UnifiedSources, None, ['.c', '.mm', '.cpp']),
        )
        # Track whether there are any C++ source files.
        # Technically this won't do the right thing for SIMPLE_PROGRAMS in
        # a directory with mixed C and C++ source, but it's not that important.
        cxx_sources = defaultdict(bool)

        for variable, (klass, gen_klass, suffixes) in varmap.items():
            allowed_suffixes = set().union(*[suffix_map[s] for s in suffixes])

            # First ensure that we haven't been given filetypes that we don't
            # recognize.
            for f in itertools.chain(sources[variable], gen_sources[variable]):
                ext = mozpath.splitext(f)[1]
                if ext not in allowed_suffixes:
                    raise SandboxValidationError(
                        '%s has an unknown file type.' % f, context)

            for srcs, cls in ((sources[variable], klass),
                              (gen_sources[variable], gen_klass)):
                # Now sort the files to let groupby work.
                sorted_files = sorted(srcs, key=canonical_suffix_for_file)
                for canonical_suffix, files in itertools.groupby(
                        sorted_files, canonical_suffix_for_file):
                    if canonical_suffix in ('.cpp', '.mm'):
                        cxx_sources[variable] = True
                    arglist = [context, list(files), canonical_suffix]
                    if (variable.startswith('UNIFIED_') and
                            'FILES_PER_UNIFIED_FILE' in context):
                        arglist.append(context['FILES_PER_UNIFIED_FILE'])
                    obj = cls(*arglist)
                    yield obj

        for f, flags in all_flags.iteritems():
            if flags.flags:
                ext = mozpath.splitext(f)[1]
                yield PerSourceFlag(context, f, flags.flags)

        # If there are any C++ sources, set all the linkables defined here
        # to require the C++ linker.
        for vars, linkable_items in ((('SOURCES', 'UNIFIED_SOURCES'), linkables),
                                     (('HOST_SOURCES',), host_linkables)):
            for var in vars:
                if cxx_sources[var]:
                    for l in linkable_items:
                        l.cxx_link = True
                    break


    def emit_from_context(self, context):
        """Convert a Context to tree metadata objects.

        This is a generator of mozbuild.frontend.data.ContextDerived instances.
        """

        # We only want to emit an InstallationTarget if one of the consulted
        # variables is defined. Later on, we look up FINAL_TARGET, which has
        # the side-effect of populating it. So, we need to do this lookup
        # early.
        if any(k in context for k in ('FINAL_TARGET', 'XPI_NAME', 'DIST_SUBDIR')):
            yield InstallationTarget(context)

        # We always emit a directory traversal descriptor. This is needed by
        # the recursive make backend.
        for o in self._emit_directory_traversal_from_context(context): yield o

        for obj in self._process_xpidl(context):
            yield obj

        # Proxy some variables as-is until we have richer classes to represent
        # them. We should aim to keep this set small because it violates the
        # desired abstraction of the build definition away from makefiles.
        passthru = VariablePassthru(context)
        varlist = [
            'ALLOW_COMPILER_WARNINGS',
            'ANDROID_APK_NAME',
            'ANDROID_APK_PACKAGE',
            'ANDROID_GENERATED_RESFILES',
            'DISABLE_STL_WRAPPING',
            'EXTRA_DSO_LDOPTS',
            'RCFILE',
            'RESFILE',
            'RCINCLUDE',
            'DEFFILE',
            'WIN32_EXE_LDFLAGS',
            'LD_VERSION_SCRIPT',
            'USE_EXTENSION_MANIFEST',
            'NO_JS_MANIFEST',
            'HAS_MISC_RULE',
        ]
        for v in varlist:
            if v in context and context[v]:
                passthru.variables[v] = context[v]

        if context.config.substs.get('OS_TARGET') == 'WINNT' and \
                context['DELAYLOAD_DLLS']:
            context['LDFLAGS'].extend([('-DELAYLOAD:%s' % dll)
                for dll in context['DELAYLOAD_DLLS']])
            context['OS_LIBS'].append('delayimp')

        for v in ['CFLAGS', 'CXXFLAGS', 'CMFLAGS', 'CMMFLAGS', 'ASFLAGS',
                  'LDFLAGS', 'HOST_CFLAGS', 'HOST_CXXFLAGS']:
            if v in context and context[v]:
                passthru.variables['MOZBUILD_' + v] = context[v]

        # NO_VISIBILITY_FLAGS is slightly different
        if context['NO_VISIBILITY_FLAGS']:
            passthru.variables['VISIBILITY_FLAGS'] = ''

        if isinstance(context, TemplateContext) and context.template == 'Gyp':
            passthru.variables['IS_GYP_DIR'] = True

        dist_install = context['DIST_INSTALL']
        if dist_install is True:
            passthru.variables['DIST_INSTALL'] = True
        elif dist_install is False:
            passthru.variables['NO_DIST_INSTALL'] = True

        # Ideally, this should be done in templates, but this is difficult at
        # the moment because USE_STATIC_LIBS can be set after a template
        # returns. Eventually, with context-based templates, it will be
        # possible.
        if (context.config.substs.get('OS_ARCH') == 'WINNT' and
                not context.config.substs.get('GNU_CC')):
            use_static_lib = (context.get('USE_STATIC_LIBS') and
                              not context.config.substs.get('MOZ_ASAN'))
            rtl_flag = '-MT' if use_static_lib else '-MD'
            if (context.config.substs.get('MOZ_DEBUG') and
                    not context.config.substs.get('MOZ_NO_DEBUG_RTL')):
                rtl_flag += 'd'
            # Use a list, like MOZBUILD_*FLAGS variables
            passthru.variables['RTL_FLAGS'] = [rtl_flag]

        generated_files = set()
        for obj in self._process_generated_files(context):
            for f in obj.outputs:
                generated_files.add(f)
            yield obj

        for path in context['CONFIGURE_SUBST_FILES']:
            sub = self._create_substitution(ConfigFileSubstitution, context,
                path)
            generated_files.add(str(sub.relpath))
            yield sub

        defines = context.get('DEFINES')
        if defines:
            yield Defines(context, defines)

        host_defines = context.get('HOST_DEFINES')
        if host_defines:
            yield HostDefines(context, host_defines)

        simple_lists = [
            ('GENERATED_EVENTS_WEBIDL_FILES', GeneratedEventWebIDLFile),
            ('GENERATED_WEBIDL_FILES', GeneratedWebIDLFile),
            ('IPDL_SOURCES', IPDLFile),
            ('PREPROCESSED_TEST_WEBIDL_FILES', PreprocessedTestWebIDLFile),
            ('PREPROCESSED_WEBIDL_FILES', PreprocessedWebIDLFile),
            ('TEST_WEBIDL_FILES', TestWebIDLFile),
            ('WEBIDL_FILES', WebIDLFile),
            ('WEBIDL_EXAMPLE_INTERFACES', ExampleWebIDLInterface),
        ]
        for context_var, klass in simple_lists:
            for name in context.get(context_var, []):
                yield klass(context, name)

        for local_include in context.get('LOCAL_INCLUDES', []):
            if (not isinstance(local_include, ObjDirPath) and
                    not os.path.exists(local_include.full_path)):
                raise SandboxValidationError('Path specified in LOCAL_INCLUDES '
                    'does not exist: %s (resolved to %s)' % (local_include,
                    local_include.full_path), context)
            yield LocalInclude(context, local_include)

        for obj in self._handle_linkables(context, passthru, generated_files):
            yield obj

        generated_files.update(['%s%s' % (k, self.config.substs.get('BIN_SUFFIX', '')) for k in self._binaries.keys()])

        components = []
        for var, cls in (
            ('BRANDING_FILES', BrandingFiles),
            ('EXPORTS', Exports),
            ('FINAL_TARGET_FILES', FinalTargetFiles),
            ('FINAL_TARGET_PP_FILES', FinalTargetPreprocessedFiles),
            ('OBJDIR_FILES', ObjdirFiles),
            ('OBJDIR_PP_FILES', ObjdirPreprocessedFiles),
            ('SDK_FILES', SdkFiles),
            ('TEST_HARNESS_FILES', TestHarnessFiles),
        ):
            all_files = context.get(var)
            if not all_files:
                continue
            if dist_install is False and var != 'TEST_HARNESS_FILES':
                raise SandboxValidationError(
                    '%s cannot be used with DIST_INSTALL = False' % var,
                    context)
            has_prefs = False
            has_resources = False
            for base, files in all_files.walk():
                if var == 'TEST_HARNESS_FILES' and not base:
                    raise SandboxValidationError(
                        'Cannot install files to the root of TEST_HARNESS_FILES', context)
                if base == 'components':
                    components.extend(files)
                if base == 'defaults/pref':
                    has_prefs = True
                if mozpath.split(base)[0] == 'res':
                    has_resources = True
                for f in files:
                    if ((var == 'FINAL_TARGET_PP_FILES' or
                         var == 'OBJDIR_PP_FILES') and
                        not isinstance(f, SourcePath)):
                        raise SandboxValidationError(
                                ('Only source directory paths allowed in ' +
                                 '%s: %s')
                                % (var, f,), context)
                    if not isinstance(f, ObjDirPath):
                        path = f.full_path
                        if '*' not in path and not os.path.exists(path):
                            raise SandboxValidationError(
                                'File listed in %s does not exist: %s'
                                % (var, path), context)
                    else:
                        # TODO: Bug 1254682 - The '/' check is to allow
                        # installing files generated from other directories,
                        # which is done occasionally for tests. However, it
                        # means we don't fail early if the file isn't actually
                        # created by the other moz.build file.
                        if f.target_basename not in generated_files and '/' not in f:
                            raise SandboxValidationError(
                                ('Objdir file listed in %s not in ' +
                                 'GENERATED_FILES: %s') % (var, f), context)

            # Addons (when XPI_NAME is defined) and Applications (when
            # DIST_SUBDIR is defined) use a different preferences directory
            # (default/preferences) from the one the GRE uses (defaults/pref).
            # Hence, we move the files from the latter to the former in that
            # case.
            if has_prefs and (context.get('XPI_NAME') or
                              context.get('DIST_SUBDIR')):
                all_files.defaults.preferences += all_files.defaults.pref
                del all_files.defaults._children['pref']

            if has_resources and (context.get('DIST_SUBDIR') or
                                  context.get('XPI_NAME')):
                raise SandboxValidationError(
                    'RESOURCES_FILES cannot be used with DIST_SUBDIR or '
                    'XPI_NAME.', context)

            yield cls(context, all_files)

        # Check for manifest declarations in EXTRA_{PP_,}COMPONENTS.
        if any(e.endswith('.js') for e in components) and \
                not any(e.endswith('.manifest') for e in components) and \
                not context.get('NO_JS_MANIFEST', False):
            raise SandboxValidationError('A .js component was specified in EXTRA_COMPONENTS '
                                         'or EXTRA_PP_COMPONENTS without a matching '
                                         '.manifest file.  See '
                                         'https://developer.mozilla.org/en/XPCOM/XPCOM_changes_in_Gecko_2.0 .',
                                         context);

        for c in components:
            if c.endswith('.manifest'):
                yield ChromeManifestEntry(context, 'chrome.manifest',
                                          Manifest('components',
                                                   mozpath.basename(c)))

        for obj in self._process_test_manifests(context):
            yield obj

        for obj in self._process_jar_manifests(context):
            yield obj

        for name, jar in context.get('JAVA_JAR_TARGETS', {}).items():
            yield ContextWrapped(context, jar)

        for name, data in context.get('ANDROID_ECLIPSE_PROJECT_TARGETS', {}).items():
            yield ContextWrapped(context, data)

        if context.get('USE_YASM') is True:
            yasm = context.config.substs.get('YASM')
            if not yasm:
                raise SandboxValidationError('yasm is not available', context)
            passthru.variables['AS'] = yasm
            passthru.variables['ASFLAGS'] = context.config.substs.get('YASM_ASFLAGS')
            passthru.variables['AS_DASH_C_FLAG'] = ''

        for (symbol, cls) in [
                ('ANDROID_RES_DIRS', AndroidResDirs),
                ('ANDROID_EXTRA_RES_DIRS', AndroidExtraResDirs),
                ('ANDROID_ASSETS_DIRS', AndroidAssetsDirs)]:
            paths = context.get(symbol)
            if not paths:
                continue
            for p in paths:
                if isinstance(p, SourcePath) and not os.path.isdir(p.full_path):
                    raise SandboxValidationError('Directory listed in '
                        '%s is not a directory: \'%s\'' %
                            (symbol, p.full_path), context)
            yield cls(context, paths)

        android_extra_packages = context.get('ANDROID_EXTRA_PACKAGES')
        if android_extra_packages:
            yield AndroidExtraPackages(context, android_extra_packages)

        if passthru.variables:
            yield passthru

    def _create_substitution(self, cls, context, path):
        sub = cls(context)
        sub.input_path = '%s.in' % path.full_path
        sub.output_path = path.translated
        sub.relpath = path

        return sub

    def _process_xpidl(self, context):
        # XPIDL source files get processed and turned into .h and .xpt files.
        # If there are multiple XPIDL files in a directory, they get linked
        # together into a final .xpt, which has the name defined by
        # XPIDL_MODULE.
        xpidl_module = context['XPIDL_MODULE']

        if context['XPIDL_SOURCES'] and not xpidl_module:
            raise SandboxValidationError('XPIDL_MODULE must be defined if '
                'XPIDL_SOURCES is defined.', context)

        if xpidl_module and not context['XPIDL_SOURCES']:
            raise SandboxValidationError('XPIDL_MODULE cannot be defined '
                'unless there are XPIDL_SOURCES', context)

        if context['XPIDL_SOURCES'] and context['DIST_INSTALL'] is False:
            self.log(logging.WARN, 'mozbuild_warning', dict(
                path=context.main_path),
                '{path}: DIST_INSTALL = False has no effect on XPIDL_SOURCES.')

        for idl in context['XPIDL_SOURCES']:
            yield XPIDLFile(context, mozpath.join(context.srcdir, idl),
                xpidl_module, add_to_manifest=not context['XPIDL_NO_MANIFEST'])

    def _process_generated_files(self, context):
        for path in context['CONFIGURE_DEFINE_FILES']:
            script = mozpath.join(mozpath.dirname(mozpath.dirname(__file__)),
                                  'action', 'process_define_files.py')
            yield GeneratedFile(context, script, 'process_define_file',
                                unicode(path),
                                [Path(context, path + '.in')])

        generated_files = context.get('GENERATED_FILES')
        if not generated_files:
            return

        for f in generated_files:
            flags = generated_files[f]
            outputs = f
            inputs = []
            if flags.script:
                method = "main"
                script = SourcePath(context, flags.script).full_path

                # Deal with cases like "C:\\path\\to\\script.py:function".
                if '.py:' in script:
                    script, method = script.rsplit('.py:', 1)
                    script += '.py'

                if not os.path.exists(script):
                    raise SandboxValidationError(
                        'Script for generating %s does not exist: %s'
                        % (f, script), context)
                if os.path.splitext(script)[1] != '.py':
                    raise SandboxValidationError(
                        'Script for generating %s does not end in .py: %s'
                        % (f, script), context)

                for i in flags.inputs:
                    p = Path(context, i)
                    if (isinstance(p, SourcePath) and
                            not os.path.exists(p.full_path)):
                        raise SandboxValidationError(
                            'Input for generating %s does not exist: %s'
                            % (f, p.full_path), context)
                    inputs.append(p)
            else:
                script = None
                method = None
            yield GeneratedFile(context, script, method, outputs, inputs)

    def _process_test_manifests(self, context):
        for prefix, info in TEST_MANIFESTS.items():
            for path, manifest in context.get('%s_MANIFESTS' % prefix, []):
                for obj in self._process_test_manifest(context, info, path, manifest):
                    yield obj

        for flavor in REFTEST_FLAVORS:
            for path, manifest in context.get('%s_MANIFESTS' % flavor.upper(), []):
                for obj in self._process_reftest_manifest(context, flavor, path, manifest):
                    yield obj

        for flavor in WEB_PLATFORM_TESTS_FLAVORS:
            for path, manifest in context.get("%s_MANIFESTS" % flavor.upper().replace('-', '_'), []):
                for obj in self._process_web_platform_tests_manifest(context, path, manifest):
                    yield obj

    def _process_test_manifest(self, context, info, manifest_path, mpmanifest):
        flavor, install_root, install_subdir, package_tests = info

        path = mozpath.normpath(mozpath.join(context.srcdir, manifest_path))
        manifest_dir = mozpath.dirname(path)
        manifest_reldir = mozpath.dirname(mozpath.relpath(path,
            context.config.topsrcdir))
        manifest_sources = [mozpath.relpath(pth, context.config.topsrcdir)
                            for pth in mpmanifest.source_files]
        install_prefix = mozpath.join(install_root, install_subdir)

        try:
            if not mpmanifest.tests:
                raise SandboxValidationError('Empty test manifest: %s'
                    % path, context)

            defaults = mpmanifest.manifest_defaults[os.path.normpath(path)]
            obj = TestManifest(context, path, mpmanifest, flavor=flavor,
                install_prefix=install_prefix,
                relpath=mozpath.join(manifest_reldir, mozpath.basename(path)),
                sources=manifest_sources,
                dupe_manifest='dupe-manifest' in defaults)

            filtered = mpmanifest.tests

            # Jetpack add-on tests are expected to be generated during the
            # build process so they won't exist here.
            if flavor != 'jetpack-addon':
                missing = [t['name'] for t in filtered if not os.path.exists(t['path'])]
                if missing:
                    raise SandboxValidationError('Test manifest (%s) lists '
                        'test that does not exist: %s' % (
                        path, ', '.join(missing)), context)

            out_dir = mozpath.join(install_prefix, manifest_reldir)
            if 'install-to-subdir' in defaults:
                # This is terrible, but what are you going to do?
                out_dir = mozpath.join(out_dir, defaults['install-to-subdir'])
                obj.manifest_obj_relpath = mozpath.join(manifest_reldir,
                                                        defaults['install-to-subdir'],
                                                        mozpath.basename(path))

            def process_support_files(test):
                install_info = self._test_files_converter.convert_support_files(
                    test, install_root, manifest_dir, out_dir)

                obj.pattern_installs.extend(install_info.pattern_installs)
                for source, dest in install_info.installs:
                    obj.installs[source] = (dest, False)
                obj.external_installs |= install_info.external_installs
                for install_path in install_info.deferred_installs:
                    if all(['*' not in install_path,
                            not os.path.isfile(mozpath.join(context.config.topsrcdir,
                                                            install_path[2:])),
                            install_path not in install_info.external_installs]):
                        raise SandboxValidationError('Error processing test '
                           'manifest %s: entry in support-files not present '
                           'in the srcdir: %s' % (path, install_path), context)

                obj.deferred_installs |= install_info.deferred_installs

            for test in filtered:
                obj.tests.append(test)

                # Some test files are compiled and should not be copied into the
                # test package. They function as identifiers rather than files.
                if package_tests:
                    manifest_relpath = mozpath.relpath(test['path'],
                        mozpath.dirname(test['manifest']))
                    obj.installs[mozpath.normpath(test['path'])] = \
                        ((mozpath.join(out_dir, manifest_relpath)), True)

                process_support_files(test)

            for path, m_defaults in mpmanifest.manifest_defaults.items():
                process_support_files(m_defaults)

            # We also copy manifests into the output directory,
            # including manifests from [include:foo] directives.
            for mpath in mpmanifest.manifests():
                mpath = mozpath.normpath(mpath)
                out_path = mozpath.join(out_dir, mozpath.basename(mpath))
                obj.installs[mpath] = (out_path, False)

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
                        'elsewhere in manifest: %s' % (path, f), context)

            yield obj
        except (AssertionError, Exception):
            raise SandboxValidationError('Error processing test '
                'manifest file %s: %s' % (path,
                    '\n'.join(traceback.format_exception(*sys.exc_info()))),
                context)

    def _process_reftest_manifest(self, context, flavor, manifest_path, manifest):
        manifest_full_path = mozpath.normpath(mozpath.join(
            context.srcdir, manifest_path))
        manifest_reldir = mozpath.dirname(mozpath.relpath(manifest_full_path,
            context.config.topsrcdir))

        # reftest manifests don't come from manifest parser. But they are
        # similar enough that we can use the same emitted objects. Note
        # that we don't perform any installs for reftests.
        obj = TestManifest(context, manifest_full_path, manifest,
                flavor=flavor, install_prefix='%s/' % flavor,
                relpath=mozpath.join(manifest_reldir,
                    mozpath.basename(manifest_path)))

        for test, source_manifest in sorted(manifest.tests):
            obj.tests.append({
                'path': test,
                'here': mozpath.dirname(test),
                'manifest': source_manifest,
                'name': mozpath.basename(test),
                'head': '',
                'tail': '',
                'support-files': '',
                'subsuite': '',
            })

        yield obj

    def _process_web_platform_tests_manifest(self, context, paths, manifest):
        manifest_path, tests_root = paths
        manifest_full_path = mozpath.normpath(mozpath.join(
            context.srcdir, manifest_path))
        manifest_reldir = mozpath.dirname(mozpath.relpath(manifest_full_path,
            context.config.topsrcdir))
        tests_root = mozpath.normpath(mozpath.join(context.srcdir, tests_root))

        # Create a equivalent TestManifest object
        obj = TestManifest(context, manifest_full_path, manifest,
                           flavor="web-platform-tests",
                           relpath=mozpath.join(manifest_reldir,
                                                mozpath.basename(manifest_path)),
                           install_prefix="web-platform/")


        for path, tests in manifest:
            path = mozpath.join(tests_root, path)
            for test in tests:
                if test.item_type not in ["testharness", "reftest"]:
                    continue

                obj.tests.append({
                    'path': path,
                    'here': mozpath.dirname(path),
                    'manifest': manifest_path,
                    'name': test.id,
                    'head': '',
                    'tail': '',
                    'support-files': '',
                    'subsuite': '',
                })

        yield obj

    def _process_jar_manifests(self, context):
        jar_manifests = context.get('JAR_MANIFESTS', [])
        if len(jar_manifests) > 1:
            raise SandboxValidationError('While JAR_MANIFESTS is a list, '
                'it is currently limited to one value.', context)

        for path in jar_manifests:
            yield JARManifest(context, path)

        # Temporary test to look for jar.mn files that creep in without using
        # the new declaration. Before, we didn't require jar.mn files to
        # declared anywhere (they were discovered). This will detect people
        # relying on the old behavior.
        if os.path.exists(os.path.join(context.srcdir, 'jar.mn')):
            if 'jar.mn' not in jar_manifests:
                raise SandboxValidationError('A jar.mn exists but it '
                    'is not referenced in the moz.build file. '
                    'Please define JAR_MANIFESTS.', context)

    def _emit_directory_traversal_from_context(self, context):
        o = DirectoryTraversal(context)
        o.dirs = context.get('DIRS', [])

        # Some paths have a subconfigure, yet also have a moz.build. Those
        # shouldn't end up in self._external_paths.
        if o.objdir:
            self._external_paths -= { o.relobjdir }

        yield o
