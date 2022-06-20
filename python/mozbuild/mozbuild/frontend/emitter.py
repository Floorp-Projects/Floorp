# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import os
import six
import sys
import time
import traceback

from collections import defaultdict, OrderedDict
from mach.mixin.logging import LoggingMixin
from mozbuild.util import memoize, OrderedDefaultDict

import mozpack.path as mozpath
import mozinfo
import pytoml

from .data import (
    BaseRustProgram,
    ChromeManifestEntry,
    ComputedFlags,
    ConfigFileSubstitution,
    Defines,
    DirectoryTraversal,
    Exports,
    FinalTargetFiles,
    FinalTargetPreprocessedFiles,
    GeneratedFile,
    GnProjectData,
    ExternalStaticLibrary,
    ExternalSharedLibrary,
    HostDefines,
    HostLibrary,
    HostProgram,
    HostRustProgram,
    HostSharedLibrary,
    HostSimpleProgram,
    HostSources,
    InstallationTarget,
    IPDLCollection,
    JARManifest,
    Library,
    Linkable,
    LocalInclude,
    LocalizedFiles,
    LocalizedPreprocessedFiles,
    ObjdirFiles,
    ObjdirPreprocessedFiles,
    PerSourceFlag,
    WebIDLCollection,
    Program,
    RustLibrary,
    HostRustLibrary,
    RustProgram,
    RustTests,
    SandboxedWasmLibrary,
    SharedLibrary,
    SimpleProgram,
    Sources,
    StaticLibrary,
    TestHarnessFiles,
    TestManifest,
    UnifiedSources,
    VariablePassthru,
    WasmDefines,
    WasmSources,
    XPCOMComponentManifests,
    XPIDLModule,
)
from mozpack.chrome.manifest import Manifest

from .reader import SandboxValidationError

from ..testing import TEST_MANIFESTS, REFTEST_FLAVORS, SupportFilesConverter

from .context import Context, SourcePath, ObjDirPath, Path, SubContext

from mozbuild.base import ExecutionSummary


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

        self.info = dict(mozinfo.info)

        self._libs = OrderedDefaultDict(list)
        self._binaries = OrderedDict()
        self._compile_dirs = set()
        self._host_compile_dirs = set()
        self._wasm_compile_dirs = set()
        self._asm_compile_dirs = set()
        self._compile_flags = dict()
        self._compile_as_flags = dict()
        self._linkage = []
        self._static_linking_shared = set()
        self._crate_verified_local = set()
        self._crate_directories = dict()
        self._idls = defaultdict(set)

        # Keep track of external paths (third party build systems), starting
        # from what we run a subconfigure in. We'll eliminate some directories
        # as we traverse them with moz.build (e.g. js/src).
        subconfigures = os.path.join(self.config.topobjdir, "subconfigures")
        paths = []
        if os.path.exists(subconfigures):
            paths = open(subconfigures).read().splitlines()
        self._external_paths = set(mozpath.normsep(d) for d in paths)

        self._emitter_time = 0.0
        self._object_count = 0
        self._test_files_converter = SupportFilesConverter()

    def summary(self):
        return ExecutionSummary(
            "Processed into {object_count:d} build config descriptors in "
            "{execution_time:.2f}s",
            execution_time=self._emitter_time,
            object_count=self._object_count,
        )

    def emit(self, output, emitfn=None):
        """Convert the BuildReader output into data structures.

        The return value from BuildReader.read_topsrcdir() (a generator) is
        typically fed into this function.
        """
        contexts = {}
        emitfn = emitfn or self.emit_from_context

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
                contexts[os.path.normcase(out.objdir)] = out

                start = time.time()
                # We need to expand the generator for the timings to work.
                objs = list(emitfn(out))
                self._emitter_time += time.time() - start

                for o in emit_objs(objs):
                    yield o

            else:
                raise Exception("Unhandled output type: %s" % type(out))

        # Don't emit Linkable objects when COMPILE_ENVIRONMENT is not set
        if self.config.substs.get("COMPILE_ENVIRONMENT"):
            start = time.time()
            objs = list(self._emit_libs_derived(contexts))
            self._emitter_time += time.time() - start

            for o in emit_objs(objs):
                yield o

    def _emit_libs_derived(self, contexts):

        # First aggregate idl sources.
        webidl_attrs = [
            ("GENERATED_EVENTS_WEBIDL_FILES", lambda c: c.generated_events_sources),
            ("GENERATED_WEBIDL_FILES", lambda c: c.generated_sources),
            ("PREPROCESSED_TEST_WEBIDL_FILES", lambda c: c.preprocessed_test_sources),
            ("PREPROCESSED_WEBIDL_FILES", lambda c: c.preprocessed_sources),
            ("TEST_WEBIDL_FILES", lambda c: c.test_sources),
            ("WEBIDL_FILES", lambda c: c.sources),
            ("WEBIDL_EXAMPLE_INTERFACES", lambda c: c.example_interfaces),
        ]
        ipdl_attrs = [
            ("IPDL_SOURCES", lambda c: c.sources),
            ("PREPROCESSED_IPDL_SOURCES", lambda c: c.preprocessed_sources),
        ]
        xpcom_attrs = [("XPCOM_MANIFESTS", lambda c: c.manifests)]

        idl_sources = {}
        for root, cls, attrs in (
            (self.config.substs.get("WEBIDL_ROOT"), WebIDLCollection, webidl_attrs),
            (self.config.substs.get("IPDL_ROOT"), IPDLCollection, ipdl_attrs),
            (
                self.config.substs.get("XPCOM_ROOT"),
                XPCOMComponentManifests,
                xpcom_attrs,
            ),
        ):
            if root:
                collection = cls(contexts[os.path.normcase(root)])
                for var, src_getter in attrs:
                    src_getter(collection).update(self._idls[var])

                idl_sources[root] = collection.all_source_files()
                if isinstance(collection, WebIDLCollection):
                    # Test webidl sources are added here as a somewhat special
                    # case.
                    idl_sources[mozpath.join(root, "test")] = [
                        s for s in collection.all_test_cpp_basenames()
                    ]

                yield collection

        # Next do FINAL_LIBRARY linkage.
        for lib in (l for libs in self._libs.values() for l in libs):
            if not isinstance(lib, (StaticLibrary, RustLibrary)) or not lib.link_into:
                continue
            if lib.link_into not in self._libs:
                raise SandboxValidationError(
                    'FINAL_LIBRARY ("%s") does not match any LIBRARY_NAME'
                    % lib.link_into,
                    contexts[os.path.normcase(lib.objdir)],
                )
            candidates = self._libs[lib.link_into]

            # When there are multiple candidates, but all are in the same
            # directory and have a different type, we want all of them to
            # have the library linked. The typical usecase is when building
            # both a static and a shared library in a directory, and having
            # that as a FINAL_LIBRARY.
            if (
                len(set(type(l) for l in candidates)) == len(candidates)
                and len(set(l.objdir for l in candidates)) == 1
            ):
                for c in candidates:
                    c.link_library(lib)
            else:
                raise SandboxValidationError(
                    'FINAL_LIBRARY ("%s") matches a LIBRARY_NAME defined in '
                    "multiple places:\n    %s"
                    % (lib.link_into, "\n    ".join(l.objdir for l in candidates)),
                    contexts[os.path.normcase(lib.objdir)],
                )

        # ...and USE_LIBS linkage.
        for context, obj, variable in self._linkage:
            self._link_libraries(context, obj, variable, idl_sources)

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
                shared_libs = sorted(
                    l.basename
                    for l in lib.linked_libraries
                    if isinstance(l, SharedLibrary)
                )
                raise SandboxValidationError(
                    'The static "%s" library is not used in a shared library '
                    "or a program, but USE_LIBS contains the following shared "
                    "library names:\n    %s\n\nMaybe you can remove the "
                    'static "%s" library?'
                    % (lib.basename, "\n    ".join(shared_libs), lib.basename),
                    contexts[os.path.normcase(lib.objdir)],
                )

        @memoize
        def rust_libraries(obj):
            libs = []
            for o in obj.linked_libraries:
                if isinstance(o, (HostRustLibrary, RustLibrary)):
                    libs.append(o)
                elif isinstance(o, (HostLibrary, StaticLibrary, SandboxedWasmLibrary)):
                    libs.extend(rust_libraries(o))
            return libs

        def check_rust_libraries(obj):
            rust_libs = set(rust_libraries(obj))
            if len(rust_libs) <= 1:
                return
            if isinstance(obj, (Library, HostLibrary)):
                what = '"%s" library' % obj.basename
            else:
                what = '"%s" program' % obj.name
            raise SandboxValidationError(
                "Cannot link the following Rust libraries into the %s:\n"
                "%s\nOnly one is allowed."
                % (
                    what,
                    "\n".join(
                        "  - %s" % r.basename
                        for r in sorted(rust_libs, key=lambda r: r.basename)
                    ),
                ),
                contexts[os.path.normcase(obj.objdir)],
            )

        # Propagate LIBRARY_DEFINES to all child libraries recursively.
        def propagate_defines(outerlib, defines):
            outerlib.lib_defines.update(defines)
            for lib in outerlib.linked_libraries:
                # Propagate defines only along FINAL_LIBRARY paths, not USE_LIBS
                # paths.
                if (
                    isinstance(lib, StaticLibrary)
                    and lib.link_into == outerlib.basename
                ):
                    propagate_defines(lib, defines)

        for lib in (l for libs in self._libs.values() for l in libs):
            if isinstance(lib, Library):
                propagate_defines(lib, lib.lib_defines)
            check_rust_libraries(lib)
            yield lib

        for lib in (l for libs in self._libs.values() for l in libs):
            lib_defines = list(lib.lib_defines.get_defines())
            if lib_defines:
                objdir_flags = self._compile_flags[lib.objdir]
                objdir_flags.resolve_flags("LIBRARY_DEFINES", lib_defines)

                objdir_flags = self._compile_as_flags.get(lib.objdir)
                if objdir_flags:
                    objdir_flags.resolve_flags("LIBRARY_DEFINES", lib_defines)

        for flags_obj in self._compile_flags.values():
            yield flags_obj

        for flags_obj in self._compile_as_flags.values():
            yield flags_obj

        for obj in self._binaries.values():
            if isinstance(obj, Linkable):
                check_rust_libraries(obj)
            yield obj

    LIBRARY_NAME_VAR = {
        "host": "HOST_LIBRARY_NAME",
        "target": "LIBRARY_NAME",
        "wasm": "SANDBOXED_WASM_LIBRARY_NAME",
    }

    ARCH_VAR = {"host": "HOST_OS_ARCH", "target": "OS_TARGET"}

    STDCXXCOMPAT_NAME = {"host": "host_stdc++compat", "target": "stdc++compat"}

    def _link_libraries(self, context, obj, variable, extra_sources):
        """Add linkage declarations to a given object."""
        assert isinstance(obj, Linkable)

        if context.objdir in extra_sources:
            # All "extra sources" are .cpp for the moment, and happen to come
            # first in order.
            obj.sources[".cpp"] = extra_sources[context.objdir] + obj.sources[".cpp"]

        for path in context.get(variable, []):
            self._link_library(context, obj, variable, path)

        # Link system libraries from OS_LIBS/HOST_OS_LIBS.
        for lib in context.get(variable.replace("USE", "OS"), []):
            obj.link_system_library(lib)

        # We have to wait for all the self._link_library calls above to have
        # happened for obj.cxx_link to be final.
        # FIXME: Theoretically, HostSharedLibrary shouldn't be here (bug
        # 1474022).
        if (
            not isinstance(
                obj, (StaticLibrary, HostLibrary, HostSharedLibrary, BaseRustProgram)
            )
            and obj.cxx_link
        ):
            if (
                context.config.substs.get("MOZ_STDCXX_COMPAT")
                and context.config.substs.get(self.ARCH_VAR.get(obj.KIND)) == "Linux"
            ):
                self._link_library(
                    context, obj, variable, self.STDCXXCOMPAT_NAME[obj.KIND]
                )
            if obj.KIND == "target":
                for lib in context.config.substs.get("STLPORT_LIBS", []):
                    obj.link_system_library(lib)

    def _link_library(self, context, obj, variable, path):
        force_static = path.startswith("static:") and obj.KIND == "target"
        if force_static:
            path = path[7:]
        name = mozpath.basename(path)
        dir = mozpath.dirname(path)
        candidates = [l for l in self._libs[name] if l.KIND == obj.KIND]
        if dir:
            if dir.startswith("/"):
                dir = mozpath.normpath(mozpath.join(obj.topobjdir, dir[1:]))
            else:
                dir = mozpath.normpath(mozpath.join(obj.objdir, dir))
            dir = mozpath.relpath(dir, obj.topobjdir)
            candidates = [l for l in candidates if l.relobjdir == dir]
            if not candidates:
                # If the given directory is under one of the external
                # (third party) paths, use a fake library reference to
                # there.
                for d in self._external_paths:
                    if dir.startswith("%s/" % d):
                        candidates = [
                            self._get_external_library(dir, name, force_static)
                        ]
                        break

            if not candidates:
                raise SandboxValidationError(
                    '%s contains "%s", but there is no "%s" %s in %s.'
                    % (variable, path, name, self.LIBRARY_NAME_VAR[obj.KIND], dir),
                    context,
                )

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
            candidates = list(libs.values())
            if force_static and not candidates:
                if dir:
                    raise SandboxValidationError(
                        '%s contains "static:%s", but there is no static '
                        '"%s" %s in %s.'
                        % (variable, path, name, self.LIBRARY_NAME_VAR[obj.KIND], dir),
                        context,
                    )
                raise SandboxValidationError(
                    '%s contains "static:%s", but there is no static "%s" '
                    "%s in the tree"
                    % (variable, name, name, self.LIBRARY_NAME_VAR[obj.KIND]),
                    context,
                )

        if not candidates:
            raise SandboxValidationError(
                '%s contains "%s", which does not match any %s in the tree.'
                % (variable, path, self.LIBRARY_NAME_VAR[obj.KIND]),
                context,
            )

        elif len(candidates) > 1:
            paths = (mozpath.join(l.relsrcdir, "moz.build") for l in candidates)
            raise SandboxValidationError(
                '%s contains "%s", which matches a %s defined in multiple '
                "places:\n    %s"
                % (
                    variable,
                    path,
                    self.LIBRARY_NAME_VAR[obj.KIND],
                    "\n    ".join(paths),
                ),
                context,
            )

        elif force_static and not isinstance(candidates[0], StaticLibrary):
            raise SandboxValidationError(
                '%s contains "static:%s", but there is only a shared "%s" '
                "in %s. You may want to add FORCE_STATIC_LIB=True in "
                '%s/moz.build, or remove "static:".'
                % (
                    variable,
                    path,
                    name,
                    candidates[0].relobjdir,
                    candidates[0].relobjdir,
                ),
                context,
            )

        elif isinstance(obj, StaticLibrary) and isinstance(
            candidates[0], SharedLibrary
        ):
            self._static_linking_shared.add(obj)
        obj.link_library(candidates[0])

    @memoize
    def _get_external_library(self, dir, name, force_static):
        # Create ExternalStaticLibrary or ExternalSharedLibrary object with a
        # context more or less truthful about where the external library is.
        context = Context(config=self.config)
        context.add_source(mozpath.join(self.config.topsrcdir, dir, "dummy"))
        if force_static:
            return ExternalStaticLibrary(context, name)
        else:
            return ExternalSharedLibrary(context, name)

    def _parse_cargo_file(self, context):
        """Parse the Cargo.toml file in context and return a Python object
        representation of it.  Raise a SandboxValidationError if the Cargo.toml
        file does not exist.  Return a tuple of (config, cargo_file)."""
        cargo_file = mozpath.join(context.srcdir, "Cargo.toml")
        if not os.path.exists(cargo_file):
            raise SandboxValidationError(
                "No Cargo.toml file found in %s" % cargo_file, context
            )
        with open(cargo_file, "r") as f:
            return pytoml.load(f), cargo_file

    def _verify_deps(
        self, context, crate_dir, crate_name, dependencies, description="Dependency"
    ):
        """Verify that a crate's dependencies all specify local paths."""
        for dep_crate_name, values in six.iteritems(dependencies):
            # A simple version number.
            if isinstance(values, (six.binary_type, six.text_type)):
                raise SandboxValidationError(
                    "%s %s of crate %s does not list a path"
                    % (description, dep_crate_name, crate_name),
                    context,
                )

            dep_path = values.get("path", None)
            if not dep_path:
                raise SandboxValidationError(
                    "%s %s of crate %s does not list a path"
                    % (description, dep_crate_name, crate_name),
                    context,
                )

            # Try to catch the case where somebody listed a
            # local path for development.
            if os.path.isabs(dep_path):
                raise SandboxValidationError(
                    "%s %s of crate %s has a non-relative path"
                    % (description, dep_crate_name, crate_name),
                    context,
                )

            if not os.path.exists(
                mozpath.join(context.config.topsrcdir, crate_dir, dep_path)
            ):
                raise SandboxValidationError(
                    "%s %s of crate %s refers to a non-existent path"
                    % (description, dep_crate_name, crate_name),
                    context,
                )

    def _rust_library(
        self, context, libname, static_args, is_gkrust=False, cls=RustLibrary
    ):
        # We need to note any Rust library for linking purposes.
        config, cargo_file = self._parse_cargo_file(context)
        crate_name = config["package"]["name"]

        if crate_name != libname:
            raise SandboxValidationError(
                "library %s does not match Cargo.toml-defined package %s"
                % (libname, crate_name),
                context,
            )

        # Check that the [lib.crate-type] field is correct
        lib_section = config.get("lib", None)
        if not lib_section:
            raise SandboxValidationError(
                "Cargo.toml for %s has no [lib] section" % libname, context
            )

        crate_type = lib_section.get("crate-type", None)
        if not crate_type:
            raise SandboxValidationError(
                "Can't determine a crate-type for %s from Cargo.toml" % libname, context
            )

        crate_type = crate_type[0]
        if crate_type != "staticlib":
            raise SandboxValidationError(
                "crate-type %s is not permitted for %s" % (crate_type, libname), context
            )

        dependencies = set(six.iterkeys(config.get("dependencies", {})))

        features = context.get(cls.FEATURES_VAR, [])
        unique_features = set(features)
        if len(features) != len(unique_features):
            raise SandboxValidationError(
                "features for %s should not contain duplicates: %s"
                % (libname, features),
                context,
            )

        return cls(
            context,
            libname,
            cargo_file,
            crate_type,
            dependencies,
            features,
            is_gkrust,
            **static_args,
        )

    def _handle_gn_dirs(self, context):
        for target_dir in context.get("GN_DIRS", []):
            context["DIRS"] += [target_dir]
            gn_dir = context["GN_DIRS"][target_dir]
            for v in ("variables",):
                if not getattr(gn_dir, "variables"):
                    raise SandboxValidationError(
                        "Missing value for " 'GN_DIRS["%s"].%s' % (target_dir, v),
                        context,
                    )

            non_unified_sources = set()
            for s in gn_dir.non_unified_sources:
                source = SourcePath(context, s)
                if not os.path.exists(source.full_path):
                    raise SandboxValidationError("Cannot find %s." % source, context)
                non_unified_sources.add(mozpath.join(context.relsrcdir, s))

            yield GnProjectData(context, target_dir, gn_dir, non_unified_sources)

    def _handle_linkables(self, context, passthru, generated_files):
        linkables = []
        host_linkables = []
        wasm_linkables = []

        def add_program(prog, var):
            if var.startswith("HOST_"):
                host_linkables.append(prog)
            else:
                linkables.append(prog)

        def check_unique_binary(program, kind):
            if program in self._binaries:
                raise SandboxValidationError(
                    'Cannot use "%s" as %s name, '
                    "because it is already used in %s"
                    % (program, kind, self._binaries[program].relsrcdir),
                    context,
                )

        for kind, cls in [("PROGRAM", Program), ("HOST_PROGRAM", HostProgram)]:
            program = context.get(kind)
            if program:
                check_unique_binary(program, kind)
                self._binaries[program] = cls(context, program)
                self._linkage.append(
                    (
                        context,
                        self._binaries[program],
                        kind.replace("PROGRAM", "USE_LIBS"),
                    )
                )
                add_program(self._binaries[program], kind)

        all_rust_programs = []
        for kind, cls in [
            ("RUST_PROGRAMS", RustProgram),
            ("HOST_RUST_PROGRAMS", HostRustProgram),
        ]:
            programs = context[kind]
            if not programs:
                continue

            all_rust_programs.append((programs, kind, cls))

        # Verify Rust program definitions.
        if all_rust_programs:
            config, cargo_file = self._parse_cargo_file(context)
            bin_section = config.get("bin", None)
            if not bin_section:
                raise SandboxValidationError(
                    "Cargo.toml in %s has no [bin] section" % context.srcdir, context
                )

            defined_binaries = {b["name"] for b in bin_section}

            for programs, kind, cls in all_rust_programs:
                for program in programs:
                    if program not in defined_binaries:
                        raise SandboxValidationError(
                            "Cannot find Cargo.toml definition for %s" % program,
                            context,
                        )

                    check_unique_binary(program, kind)
                    self._binaries[program] = cls(context, program, cargo_file)
                    add_program(self._binaries[program], kind)

        for kind, cls in [
            ("SIMPLE_PROGRAMS", SimpleProgram),
            ("CPP_UNIT_TESTS", SimpleProgram),
            ("HOST_SIMPLE_PROGRAMS", HostSimpleProgram),
        ]:
            for program in context[kind]:
                if program in self._binaries:
                    raise SandboxValidationError(
                        'Cannot use "%s" in %s, '
                        "because it is already used in %s"
                        % (program, kind, self._binaries[program].relsrcdir),
                        context,
                    )
                self._binaries[program] = cls(
                    context, program, is_unit_test=kind == "CPP_UNIT_TESTS"
                )
                self._linkage.append(
                    (
                        context,
                        self._binaries[program],
                        "HOST_USE_LIBS"
                        if kind == "HOST_SIMPLE_PROGRAMS"
                        else "USE_LIBS",
                    )
                )
                add_program(self._binaries[program], kind)

        host_libname = context.get("HOST_LIBRARY_NAME")
        libname = context.get("LIBRARY_NAME")

        if host_libname:
            if host_libname == libname:
                raise SandboxValidationError(
                    "LIBRARY_NAME and HOST_LIBRARY_NAME must have a different value",
                    context,
                )

            is_rust_library = context.get("IS_RUST_LIBRARY")
            if is_rust_library:
                lib = self._rust_library(context, host_libname, {}, cls=HostRustLibrary)
            elif context.get("FORCE_SHARED_LIB"):
                lib = HostSharedLibrary(context, host_libname)
            else:
                lib = HostLibrary(context, host_libname)
            self._libs[host_libname].append(lib)
            self._linkage.append((context, lib, "HOST_USE_LIBS"))
            host_linkables.append(lib)

        final_lib = context.get("FINAL_LIBRARY")
        if not libname and final_lib:
            # If no LIBRARY_NAME is given, create one.
            libname = context.relsrcdir.replace("/", "_")

        static_lib = context.get("FORCE_STATIC_LIB")
        shared_lib = context.get("FORCE_SHARED_LIB")

        static_name = context.get("STATIC_LIBRARY_NAME")
        shared_name = context.get("SHARED_LIBRARY_NAME")

        is_framework = context.get("IS_FRAMEWORK")

        soname = context.get("SONAME")

        lib_defines = context.get("LIBRARY_DEFINES")

        wasm_lib = context.get("SANDBOXED_WASM_LIBRARY_NAME")

        shared_args = {}
        static_args = {}

        if final_lib:
            if static_lib:
                raise SandboxValidationError(
                    "FINAL_LIBRARY implies FORCE_STATIC_LIB. "
                    "Please remove the latter.",
                    context,
                )
            if shared_lib:
                raise SandboxValidationError(
                    "FINAL_LIBRARY conflicts with FORCE_SHARED_LIB. "
                    "Please remove one.",
                    context,
                )
            if is_framework:
                raise SandboxValidationError(
                    "FINAL_LIBRARY conflicts with IS_FRAMEWORK. " "Please remove one.",
                    context,
                )
            static_args["link_into"] = final_lib
            static_lib = True

        if libname:
            if is_framework:
                if soname:
                    raise SandboxValidationError(
                        "IS_FRAMEWORK conflicts with SONAME. " "Please remove one.",
                        context,
                    )
                shared_lib = True
                shared_args["variant"] = SharedLibrary.FRAMEWORK

            if not static_lib and not shared_lib:
                static_lib = True

            if static_name:
                if not static_lib:
                    raise SandboxValidationError(
                        "STATIC_LIBRARY_NAME requires FORCE_STATIC_LIB", context
                    )
                static_args["real_name"] = static_name

            if shared_name:
                if not shared_lib:
                    raise SandboxValidationError(
                        "SHARED_LIBRARY_NAME requires FORCE_SHARED_LIB", context
                    )
                shared_args["real_name"] = shared_name

            if soname:
                if not shared_lib:
                    raise SandboxValidationError(
                        "SONAME requires FORCE_SHARED_LIB", context
                    )
                shared_args["soname"] = soname

            if context.get("NO_EXPAND_LIBS"):
                if not static_lib:
                    raise SandboxValidationError(
                        "NO_EXPAND_LIBS can only be set for static libraries.", context
                    )
                static_args["no_expand_lib"] = True

            if shared_lib and static_lib:
                if not static_name and not shared_name:
                    raise SandboxValidationError(
                        "Both FORCE_STATIC_LIB and FORCE_SHARED_LIB are True, "
                        "but neither STATIC_LIBRARY_NAME or "
                        "SHARED_LIBRARY_NAME is set. At least one is required.",
                        context,
                    )
                if static_name and not shared_name and static_name == libname:
                    raise SandboxValidationError(
                        "Both FORCE_STATIC_LIB and FORCE_SHARED_LIB are True, "
                        "but STATIC_LIBRARY_NAME is the same as LIBRARY_NAME, "
                        "and SHARED_LIBRARY_NAME is unset. Please either "
                        "change STATIC_LIBRARY_NAME or LIBRARY_NAME, or set "
                        "SHARED_LIBRARY_NAME.",
                        context,
                    )
                if shared_name and not static_name and shared_name == libname:
                    raise SandboxValidationError(
                        "Both FORCE_STATIC_LIB and FORCE_SHARED_LIB are True, "
                        "but SHARED_LIBRARY_NAME is the same as LIBRARY_NAME, "
                        "and STATIC_LIBRARY_NAME is unset. Please either "
                        "change SHARED_LIBRARY_NAME or LIBRARY_NAME, or set "
                        "STATIC_LIBRARY_NAME.",
                        context,
                    )
                if shared_name and static_name and shared_name == static_name:
                    raise SandboxValidationError(
                        "Both FORCE_STATIC_LIB and FORCE_SHARED_LIB are True, "
                        "but SHARED_LIBRARY_NAME is the same as "
                        "STATIC_LIBRARY_NAME. Please change one of them.",
                        context,
                    )

            symbols_file = context.get("SYMBOLS_FILE")
            if symbols_file:
                if not shared_lib:
                    raise SandboxValidationError(
                        "SYMBOLS_FILE can only be used with a SHARED_LIBRARY.", context
                    )
                if context.get("DEFFILE"):
                    raise SandboxValidationError(
                        "SYMBOLS_FILE cannot be used along DEFFILE.", context
                    )
                if isinstance(symbols_file, SourcePath):
                    if not os.path.exists(symbols_file.full_path):
                        raise SandboxValidationError(
                            "Path specified in SYMBOLS_FILE does not exist: %s "
                            "(resolved to %s)" % (symbols_file, symbols_file.full_path),
                            context,
                        )
                    shared_args["symbols_file"] = True
                else:
                    if symbols_file.target_basename not in generated_files:
                        raise SandboxValidationError(
                            (
                                "Objdir file specified in SYMBOLS_FILE not in "
                                + "GENERATED_FILES: %s"
                            )
                            % (symbols_file,),
                            context,
                        )
                    shared_args["symbols_file"] = symbols_file.target_basename

            if shared_lib:
                lib = SharedLibrary(context, libname, **shared_args)
                self._libs[libname].append(lib)
                self._linkage.append((context, lib, "USE_LIBS"))
                linkables.append(lib)
                if not lib.installed:
                    generated_files.add(lib.lib_name)
                if symbols_file and isinstance(symbols_file, SourcePath):
                    script = mozpath.join(
                        mozpath.dirname(mozpath.dirname(__file__)),
                        "action",
                        "generate_symbols_file.py",
                    )
                    defines = ()
                    if lib.defines:
                        defines = lib.defines.get_defines()
                    yield GeneratedFile(
                        context,
                        script,
                        "generate_symbols_file",
                        lib.symbols_file,
                        [symbols_file],
                        defines,
                        required_during_compile=[lib.symbols_file],
                    )
            if static_lib:
                is_rust_library = context.get("IS_RUST_LIBRARY")
                if is_rust_library:
                    lib = self._rust_library(
                        context,
                        libname,
                        static_args,
                        is_gkrust=bool(context.get("IS_GKRUST")),
                    )
                else:
                    lib = StaticLibrary(context, libname, **static_args)
                self._libs[libname].append(lib)
                self._linkage.append((context, lib, "USE_LIBS"))
                linkables.append(lib)

            if lib_defines:
                if not libname:
                    raise SandboxValidationError(
                        "LIBRARY_DEFINES needs a " "LIBRARY_NAME to take effect",
                        context,
                    )
                lib.lib_defines.update(lib_defines)

        if wasm_lib:
            if wasm_lib == libname:
                raise SandboxValidationError(
                    "SANDBOXED_WASM_LIBRARY_NAME and LIBRARY_NAME must have a "
                    "different value.",
                    context,
                )
            if wasm_lib == host_libname:
                raise SandboxValidationError(
                    "SANDBOXED_WASM_LIBRARY_NAME and HOST_LIBRARY_NAME must "
                    "have a different value.",
                    context,
                )
            if wasm_lib == shared_name:
                raise SandboxValidationError(
                    "SANDBOXED_WASM_LIBRARY_NAME and SHARED_NAME must have a "
                    "different value.",
                    context,
                )
            if wasm_lib == static_name:
                raise SandboxValidationError(
                    "SANDBOXED_WASM_LIBRARY_NAME and STATIC_NAME must have a "
                    "different value.",
                    context,
                )
            lib = SandboxedWasmLibrary(context, wasm_lib)
            self._libs[libname].append(lib)
            wasm_linkables.append(lib)
            self._wasm_compile_dirs.add(context.objdir)

        seen = {}
        for symbol in ("SOURCES", "UNIFIED_SOURCES"):
            for src in context.get(symbol, []):
                basename = os.path.splitext(os.path.basename(src))[0]
                if basename in seen:
                    other_src, where = seen[basename]
                    extra = ""
                    if "UNIFIED_SOURCES" in (symbol, where):
                        extra = " in non-unified builds"
                    raise SandboxValidationError(
                        f"{src} from {symbol} would have the same object name "
                        f"as {other_src} from {where}{extra}.",
                        context,
                    )
                seen[basename] = (src, symbol)

        # Only emit sources if we have linkables defined in the same context.
        # Note the linkables are not emitted in this function, but much later,
        # after aggregation (because of e.g. USE_LIBS processing).
        if not (linkables or host_linkables or wasm_linkables):
            return

        self._compile_dirs.add(context.objdir)

        if host_linkables and not all(
            isinstance(l, HostRustLibrary) for l in host_linkables
        ):
            self._host_compile_dirs.add(context.objdir)
            # TODO: objdirs with only host things in them shouldn't need target
            # flags, but there's at least one Makefile.in (in
            # build/unix/elfhack) that relies on the value of LDFLAGS being
            # passed to one-off rules.
            self._compile_dirs.add(context.objdir)

        sources = defaultdict(list)
        gen_sources = defaultdict(list)
        all_flags = {}
        for symbol in ("SOURCES", "HOST_SOURCES", "UNIFIED_SOURCES", "WASM_SOURCES"):
            srcs = sources[symbol]
            gen_srcs = gen_sources[symbol]
            context_srcs = context.get(symbol, [])
            seen_sources = set()
            for f in context_srcs:
                if f in seen_sources:
                    raise SandboxValidationError(
                        "Source file should only "
                        "be added to %s once: %s" % (symbol, f),
                        context,
                    )
                seen_sources.add(f)
                full_path = f.full_path
                if isinstance(f, SourcePath):
                    srcs.append(full_path)
                else:
                    assert isinstance(f, Path)
                    gen_srcs.append(full_path)
                if symbol == "SOURCES":
                    context_flags = context_srcs[f]
                    if context_flags:
                        all_flags[full_path] = context_flags

                if isinstance(f, SourcePath) and not os.path.exists(full_path):
                    raise SandboxValidationError(
                        "File listed in %s does not "
                        "exist: '%s'" % (symbol, full_path),
                        context,
                    )

        # Process the .cpp files generated by IPDL as generated sources within
        # the context which declared the IPDL_SOURCES attribute.
        ipdl_root = self.config.substs.get("IPDL_ROOT")
        for symbol in ("IPDL_SOURCES", "PREPROCESSED_IPDL_SOURCES"):
            context_srcs = context.get(symbol, [])
            for f in context_srcs:
                root, ext = mozpath.splitext(mozpath.basename(f))

                suffix_map = {
                    ".ipdlh": [".cpp"],
                    ".ipdl": [".cpp", "Child.cpp", "Parent.cpp"],
                }
                if ext not in suffix_map:
                    raise SandboxValidationError(
                        "Unexpected extension for IPDL source %s" % ext
                    )

                gen_sources["UNIFIED_SOURCES"].extend(
                    mozpath.join(ipdl_root, root + suffix) for suffix in suffix_map[ext]
                )

        no_pgo = context.get("NO_PGO")
        no_pgo_sources = [f for f, flags in six.iteritems(all_flags) if flags.no_pgo]
        if no_pgo:
            if no_pgo_sources:
                raise SandboxValidationError(
                    "NO_PGO and SOURCES[...].no_pgo " "cannot be set at the same time",
                    context,
                )
            passthru.variables["NO_PROFILE_GUIDED_OPTIMIZE"] = no_pgo
        if no_pgo_sources:
            passthru.variables["NO_PROFILE_GUIDED_OPTIMIZE"] = no_pgo_sources

        # A map from "canonical suffixes" for a particular source file
        # language to the range of suffixes associated with that language.
        #
        # We deliberately don't list the canonical suffix in the suffix list
        # in the definition; we'll add it in programmatically after defining
        # things.
        suffix_map = {
            ".s": set([".asm"]),
            ".c": set(),
            ".m": set(),
            ".mm": set(),
            ".cpp": set([".cc", ".cxx"]),
            ".S": set(),
        }

        # The inverse of the above, mapping suffixes to their canonical suffix.
        canonicalized_suffix_map = {}
        for suffix, alternatives in six.iteritems(suffix_map):
            alternatives.add(suffix)
            for a in alternatives:
                canonicalized_suffix_map[a] = suffix

        # A map from moz.build variables to the canonical suffixes of file
        # kinds that can be listed therein.
        all_suffixes = list(suffix_map.keys())
        varmap = dict(
            SOURCES=(Sources, all_suffixes),
            HOST_SOURCES=(HostSources, [".c", ".mm", ".cpp"]),
            UNIFIED_SOURCES=(UnifiedSources, [".c", ".mm", ".m", ".cpp"]),
        )
        # Only include a WasmSources context if there are any WASM_SOURCES.
        # (This is going to matter later because we inject an extra .c file to
        # compile with the wasm compiler if, and only if, there are any WASM
        # sources.)
        if sources["WASM_SOURCES"] or gen_sources["WASM_SOURCES"]:
            varmap["WASM_SOURCES"] = (WasmSources, [".c", ".cpp"])
        # Track whether there are any C++ source files.
        # Technically this won't do the right thing for SIMPLE_PROGRAMS in
        # a directory with mixed C and C++ source, but it's not that important.
        cxx_sources = defaultdict(bool)

        # Source files to track for linkables associated with this context.
        ctxt_sources = defaultdict(lambda: defaultdict(list))

        for variable, (klass, suffixes) in varmap.items():
            # Group static and generated files by their canonical suffixes, and
            # ensure we haven't been given filetypes that we don't recognize.
            by_canonical_suffix = defaultdict(lambda: {"static": [], "generated": []})
            for srcs, key in (
                (sources[variable], "static"),
                (gen_sources[variable], "generated"),
            ):
                for f in srcs:
                    canonical_suffix = canonicalized_suffix_map.get(
                        mozpath.splitext(f)[1]
                    )
                    if canonical_suffix not in suffixes:
                        raise SandboxValidationError(
                            "%s has an unknown file type." % f, context
                        )
                    by_canonical_suffix[canonical_suffix][key].append(f)

            # Yield an object for each canonical suffix, grouping generated and
            # static sources together to allow them to be unified together.
            for canonical_suffix in sorted(by_canonical_suffix.keys()):
                if canonical_suffix in (".cpp", ".mm"):
                    cxx_sources[variable] = True
                elif canonical_suffix in (".s", ".S"):
                    self._asm_compile_dirs.add(context.objdir)
                src_group = by_canonical_suffix[canonical_suffix]
                obj = klass(
                    context,
                    src_group["static"],
                    src_group["generated"],
                    canonical_suffix,
                )
                srcs = list(obj.files)
                if isinstance(obj, UnifiedSources) and obj.have_unified_mapping:
                    srcs = sorted(dict(obj.unified_source_mapping).keys())
                ctxt_sources[variable][canonical_suffix] += srcs
                yield obj

        if ctxt_sources:
            for linkable in linkables:
                for target_var in ("SOURCES", "UNIFIED_SOURCES"):
                    for suffix, srcs in ctxt_sources[target_var].items():
                        linkable.sources[suffix] += srcs
            for host_linkable in host_linkables:
                for suffix, srcs in ctxt_sources["HOST_SOURCES"].items():
                    host_linkable.sources[suffix] += srcs
            for wasm_linkable in wasm_linkables:
                for suffix, srcs in ctxt_sources["WASM_SOURCES"].items():
                    wasm_linkable.sources[suffix] += srcs

        for f, flags in sorted(six.iteritems(all_flags)):
            if flags.flags:
                ext = mozpath.splitext(f)[1]
                yield PerSourceFlag(context, f, flags.flags)

        # If there are any C++ sources, set all the linkables defined here
        # to require the C++ linker.
        for vars, linkable_items in (
            (("SOURCES", "UNIFIED_SOURCES"), linkables),
            (("HOST_SOURCES",), host_linkables),
        ):
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
        if any(k in context for k in ("FINAL_TARGET", "XPI_NAME", "DIST_SUBDIR")):
            yield InstallationTarget(context)

        for obj in self._handle_gn_dirs(context):
            yield obj

        # We always emit a directory traversal descriptor. This is needed by
        # the recursive make backend.
        for o in self._emit_directory_traversal_from_context(context):
            yield o

        for obj in self._process_xpidl(context):
            yield obj

        computed_flags = ComputedFlags(context, context["COMPILE_FLAGS"])
        computed_link_flags = ComputedFlags(context, context["LINK_FLAGS"])
        computed_host_flags = ComputedFlags(context, context["HOST_COMPILE_FLAGS"])
        computed_as_flags = ComputedFlags(context, context["ASM_FLAGS"])
        computed_wasm_flags = ComputedFlags(context, context["WASM_FLAGS"])

        # Proxy some variables as-is until we have richer classes to represent
        # them. We should aim to keep this set small because it violates the
        # desired abstraction of the build definition away from makefiles.
        passthru = VariablePassthru(context)
        varlist = [
            "EXTRA_DSO_LDOPTS",
            "RCFILE",
            "RCINCLUDE",
            "WIN32_EXE_LDFLAGS",
            "USE_EXTENSION_MANIFEST",
        ]
        for v in varlist:
            if v in context and context[v]:
                passthru.variables[v] = context[v]

        if (
            context.config.substs.get("OS_TARGET") == "WINNT"
            and context["DELAYLOAD_DLLS"]
        ):
            if context.config.substs.get("CC_TYPE") != "clang":
                context["LDFLAGS"].extend(
                    [("-DELAYLOAD:%s" % dll) for dll in context["DELAYLOAD_DLLS"]]
                )
            else:
                context["LDFLAGS"].extend(
                    [
                        ("-Wl,-Xlink=-DELAYLOAD:%s" % dll)
                        for dll in context["DELAYLOAD_DLLS"]
                    ]
                )
            context["OS_LIBS"].append("delayimp")

        for v in ["CMFLAGS", "CMMFLAGS"]:
            if v in context and context[v]:
                passthru.variables["MOZBUILD_" + v] = context[v]

        for v in ["CXXFLAGS", "CFLAGS"]:
            if v in context and context[v]:
                computed_flags.resolve_flags("MOZBUILD_%s" % v, context[v])

        for v in ["WASM_CFLAGS", "WASM_CXXFLAGS"]:
            if v in context and context[v]:
                computed_wasm_flags.resolve_flags("MOZBUILD_%s" % v, context[v])

        for v in ["HOST_CXXFLAGS", "HOST_CFLAGS"]:
            if v in context and context[v]:
                computed_host_flags.resolve_flags("MOZBUILD_%s" % v, context[v])

        if "LDFLAGS" in context and context["LDFLAGS"]:
            computed_link_flags.resolve_flags("MOZBUILD", context["LDFLAGS"])

        deffile = context.get("DEFFILE")
        if deffile and context.config.substs.get("OS_TARGET") == "WINNT":
            if isinstance(deffile, SourcePath):
                if not os.path.exists(deffile.full_path):
                    raise SandboxValidationError(
                        "Path specified in DEFFILE does not exist: %s "
                        "(resolved to %s)" % (deffile, deffile.full_path),
                        context,
                    )
                path = mozpath.relpath(deffile.full_path, context.objdir)
            else:
                path = deffile.target_basename

            if context.config.substs.get("GNU_CC"):
                computed_link_flags.resolve_flags("DEFFILE", [path])
            else:
                computed_link_flags.resolve_flags("DEFFILE", ["-DEF:" + path])

        dist_install = context["DIST_INSTALL"]
        if dist_install is True:
            passthru.variables["DIST_INSTALL"] = True
        elif dist_install is False:
            passthru.variables["NO_DIST_INSTALL"] = True

        # Ideally, this should be done in templates, but this is difficult at
        # the moment because USE_STATIC_LIBS can be set after a template
        # returns. Eventually, with context-based templates, it will be
        # possible.
        if context.config.substs.get(
            "OS_ARCH"
        ) == "WINNT" and not context.config.substs.get("GNU_CC"):
            use_static_lib = context.get(
                "USE_STATIC_LIBS"
            ) and not context.config.substs.get("MOZ_ASAN")
            rtl_flag = "-MT" if use_static_lib else "-MD"
            if context.config.substs.get("MOZ_DEBUG") and not context.config.substs.get(
                "MOZ_NO_DEBUG_RTL"
            ):
                rtl_flag += "d"
            computed_flags.resolve_flags("RTL", [rtl_flag])
            if not context.config.substs.get("CROSS_COMPILE"):
                computed_host_flags.resolve_flags("RTL", [rtl_flag])

        generated_files = set()
        localized_generated_files = set()
        for obj in self._process_generated_files(context):
            for f in obj.outputs:
                generated_files.add(f)
                if obj.localized:
                    localized_generated_files.add(f)
            yield obj

        for path in context["CONFIGURE_SUBST_FILES"]:
            sub = self._create_substitution(ConfigFileSubstitution, context, path)
            generated_files.add(str(sub.relpath))
            yield sub

        for defines_var, cls, backend_flags in (
            ("DEFINES", Defines, (computed_flags, computed_as_flags)),
            ("HOST_DEFINES", HostDefines, (computed_host_flags,)),
            ("WASM_DEFINES", WasmDefines, (computed_wasm_flags,)),
        ):
            defines = context.get(defines_var)
            if defines:
                defines_obj = cls(context, defines)
                if isinstance(defines_obj, Defines):
                    # DEFINES have consumers outside the compile command line,
                    # HOST_DEFINES do not.
                    yield defines_obj
            else:
                # If we don't have explicitly set defines we need to make sure
                # initialized values if present end up in computed flags.
                defines_obj = cls(context, context[defines_var])

            defines_from_obj = list(defines_obj.get_defines())
            if defines_from_obj:
                for flags in backend_flags:
                    flags.resolve_flags(defines_var, defines_from_obj)

        idl_vars = (
            "GENERATED_EVENTS_WEBIDL_FILES",
            "GENERATED_WEBIDL_FILES",
            "PREPROCESSED_TEST_WEBIDL_FILES",
            "PREPROCESSED_WEBIDL_FILES",
            "TEST_WEBIDL_FILES",
            "WEBIDL_FILES",
            "IPDL_SOURCES",
            "PREPROCESSED_IPDL_SOURCES",
            "XPCOM_MANIFESTS",
        )
        for context_var in idl_vars:
            for name in context.get(context_var, []):
                self._idls[context_var].add(mozpath.join(context.srcdir, name))
        # WEBIDL_EXAMPLE_INTERFACES do not correspond to files.
        for name in context.get("WEBIDL_EXAMPLE_INTERFACES", []):
            self._idls["WEBIDL_EXAMPLE_INTERFACES"].add(name)

        local_includes = []
        for local_include in context.get("LOCAL_INCLUDES", []):
            full_path = local_include.full_path
            if not isinstance(local_include, ObjDirPath):
                if not os.path.exists(full_path):
                    raise SandboxValidationError(
                        "Path specified in LOCAL_INCLUDES does not exist: %s (resolved to %s)"
                        % (local_include, full_path),
                        context,
                    )
                if not os.path.isdir(full_path):
                    raise SandboxValidationError(
                        "Path specified in LOCAL_INCLUDES "
                        "is a filename, but a directory is required: %s "
                        "(resolved to %s)" % (local_include, full_path),
                        context,
                    )
            if (
                full_path == context.config.topsrcdir
                or full_path == context.config.topobjdir
            ):
                raise SandboxValidationError(
                    "Path specified in LOCAL_INCLUDES "
                    "(%s) resolves to the topsrcdir or topobjdir (%s), which is "
                    "not allowed" % (local_include, full_path),
                    context,
                )
            include_obj = LocalInclude(context, local_include)
            local_includes.append(include_obj.path.full_path)
            yield include_obj

        computed_flags.resolve_flags(
            "LOCAL_INCLUDES", ["-I%s" % p for p in local_includes]
        )
        computed_as_flags.resolve_flags(
            "LOCAL_INCLUDES", ["-I%s" % p for p in local_includes]
        )
        computed_host_flags.resolve_flags(
            "LOCAL_INCLUDES", ["-I%s" % p for p in local_includes]
        )
        computed_wasm_flags.resolve_flags(
            "LOCAL_INCLUDES", ["-I%s" % p for p in local_includes]
        )

        for obj in self._handle_linkables(context, passthru, generated_files):
            yield obj

        generated_files.update(
            [
                "%s%s" % (k, self.config.substs.get("BIN_SUFFIX", ""))
                for k in self._binaries.keys()
            ]
        )

        components = []
        for var, cls in (
            ("EXPORTS", Exports),
            ("FINAL_TARGET_FILES", FinalTargetFiles),
            ("FINAL_TARGET_PP_FILES", FinalTargetPreprocessedFiles),
            ("LOCALIZED_FILES", LocalizedFiles),
            ("LOCALIZED_PP_FILES", LocalizedPreprocessedFiles),
            ("OBJDIR_FILES", ObjdirFiles),
            ("OBJDIR_PP_FILES", ObjdirPreprocessedFiles),
            ("TEST_HARNESS_FILES", TestHarnessFiles),
        ):
            all_files = context.get(var)
            if not all_files:
                continue
            if dist_install is False and var != "TEST_HARNESS_FILES":
                raise SandboxValidationError(
                    "%s cannot be used with DIST_INSTALL = False" % var, context
                )
            has_prefs = False
            has_resources = False
            for base, files in all_files.walk():
                if var == "TEST_HARNESS_FILES" and not base:
                    raise SandboxValidationError(
                        "Cannot install files to the root of TEST_HARNESS_FILES",
                        context,
                    )
                if base == "components":
                    components.extend(files)
                if base == "defaults/pref":
                    has_prefs = True
                if mozpath.split(base)[0] == "res":
                    has_resources = True
                for f in files:
                    if (
                        var
                        in (
                            "FINAL_TARGET_PP_FILES",
                            "OBJDIR_PP_FILES",
                            "LOCALIZED_PP_FILES",
                        )
                        and not isinstance(f, SourcePath)
                    ):
                        raise SandboxValidationError(
                            ("Only source directory paths allowed in " + "%s: %s")
                            % (var, f),
                            context,
                        )
                    if var.startswith("LOCALIZED_"):
                        if isinstance(f, SourcePath):
                            if f.startswith("en-US/"):
                                pass
                            elif "locales/en-US/" in f:
                                pass
                            else:
                                raise SandboxValidationError(
                                    "%s paths must start with `en-US/` or "
                                    "contain `locales/en-US/`: %s" % (var, f),
                                    context,
                                )

                    if not isinstance(f, ObjDirPath):
                        path = f.full_path
                        if "*" not in path and not os.path.exists(path):
                            raise SandboxValidationError(
                                "File listed in %s does not exist: %s" % (var, path),
                                context,
                            )
                    else:
                        # TODO: Bug 1254682 - The '/' check is to allow
                        # installing files generated from other directories,
                        # which is done occasionally for tests. However, it
                        # means we don't fail early if the file isn't actually
                        # created by the other moz.build file.
                        if f.target_basename not in generated_files and "/" not in f:
                            raise SandboxValidationError(
                                (
                                    "Objdir file listed in %s not in "
                                    + "GENERATED_FILES: %s"
                                )
                                % (var, f),
                                context,
                            )

                        if var.startswith("LOCALIZED_"):
                            # Further require that LOCALIZED_FILES are from
                            # LOCALIZED_GENERATED_FILES.
                            if f.target_basename not in localized_generated_files:
                                raise SandboxValidationError(
                                    (
                                        "Objdir file listed in %s not in "
                                        + "LOCALIZED_GENERATED_FILES: %s"
                                    )
                                    % (var, f),
                                    context,
                                )
                        else:
                            # Additionally, don't allow LOCALIZED_GENERATED_FILES to be used
                            # in anything *but* LOCALIZED_FILES.
                            if f.target_basename in localized_generated_files:
                                raise SandboxValidationError(
                                    (
                                        "Outputs of LOCALIZED_GENERATED_FILES cannot "
                                        "be used in %s: %s"
                                    )
                                    % (var, f),
                                    context,
                                )

            # Addons (when XPI_NAME is defined) and Applications (when
            # DIST_SUBDIR is defined) use a different preferences directory
            # (default/preferences) from the one the GRE uses (defaults/pref).
            # Hence, we move the files from the latter to the former in that
            # case.
            if has_prefs and (context.get("XPI_NAME") or context.get("DIST_SUBDIR")):
                all_files.defaults.preferences += all_files.defaults.pref
                del all_files.defaults._children["pref"]

            if has_resources and (
                context.get("DIST_SUBDIR") or context.get("XPI_NAME")
            ):
                raise SandboxValidationError(
                    "RESOURCES_FILES cannot be used with DIST_SUBDIR or " "XPI_NAME.",
                    context,
                )

            yield cls(context, all_files)

        for c in components:
            if c.endswith(".manifest"):
                yield ChromeManifestEntry(
                    context,
                    "chrome.manifest",
                    Manifest("components", mozpath.basename(c)),
                )

        rust_tests = context.get("RUST_TESTS", [])
        if rust_tests:
            # TODO: more sophisticated checking of the declared name vs.
            # contents of the Cargo.toml file.
            features = context.get("RUST_TEST_FEATURES", [])

            yield RustTests(context, rust_tests, features)

        for obj in self._process_test_manifests(context):
            yield obj

        for obj in self._process_jar_manifests(context):
            yield obj

        computed_as_flags.resolve_flags("MOZBUILD", context.get("ASFLAGS"))

        if context.get("USE_NASM") is True:
            nasm = context.config.substs.get("NASM")
            if not nasm:
                raise SandboxValidationError("nasm is not available", context)
            passthru.variables["AS"] = nasm
            passthru.variables["AS_DASH_C_FLAG"] = ""
            passthru.variables["ASOUTOPTION"] = "-o "
            computed_as_flags.resolve_flags(
                "OS", context.config.substs.get("NASM_ASFLAGS", [])
            )

        if context.get("USE_INTEGRATED_CLANGCL_AS") is True:
            if context.config.substs.get("CC_TYPE") != "clang-cl":
                raise SandboxValidationError("clang-cl is not available", context)
            passthru.variables["AS"] = context.config.substs.get("CC")
            passthru.variables["AS_DASH_C_FLAG"] = "-c"
            passthru.variables["ASOUTOPTION"] = "-o "

        if passthru.variables:
            yield passthru

        if context.objdir in self._compile_dirs:
            self._compile_flags[context.objdir] = computed_flags
            yield computed_link_flags

        if context.objdir in self._asm_compile_dirs:
            self._compile_as_flags[context.objdir] = computed_as_flags

        if context.objdir in self._host_compile_dirs:
            yield computed_host_flags

        if context.objdir in self._wasm_compile_dirs:
            yield computed_wasm_flags

    def _create_substitution(self, cls, context, path):
        sub = cls(context)
        sub.input_path = "%s.in" % path.full_path
        sub.output_path = path.translated
        sub.relpath = path

        return sub

    def _process_xpidl(self, context):
        # XPIDL source files get processed and turned into .h and .xpt files.
        # If there are multiple XPIDL files in a directory, they get linked
        # together into a final .xpt, which has the name defined by
        # XPIDL_MODULE.
        xpidl_module = context["XPIDL_MODULE"]

        if not xpidl_module:
            if context["XPIDL_SOURCES"]:
                raise SandboxValidationError(
                    "XPIDL_MODULE must be defined if " "XPIDL_SOURCES is defined.",
                    context,
                )
            return

        if not context["XPIDL_SOURCES"]:
            raise SandboxValidationError(
                "XPIDL_MODULE cannot be defined " "unless there are XPIDL_SOURCES",
                context,
            )

        if context["DIST_INSTALL"] is False:
            self.log(
                logging.WARN,
                "mozbuild_warning",
                dict(path=context.main_path),
                "{path}: DIST_INSTALL = False has no effect on XPIDL_SOURCES.",
            )

        for idl in context["XPIDL_SOURCES"]:
            if not os.path.exists(idl.full_path):
                raise SandboxValidationError(
                    "File %s from XPIDL_SOURCES " "does not exist" % idl.full_path,
                    context,
                )

        yield XPIDLModule(context, xpidl_module, context["XPIDL_SOURCES"])

    def _process_generated_files(self, context):
        for path in context["CONFIGURE_DEFINE_FILES"]:
            script = mozpath.join(
                mozpath.dirname(mozpath.dirname(__file__)),
                "action",
                "process_define_files.py",
            )
            yield GeneratedFile(
                context,
                script,
                "process_define_file",
                six.text_type(path),
                [Path(context, path + ".in")],
            )

        generated_files = context.get("GENERATED_FILES") or []
        localized_generated_files = context.get("LOCALIZED_GENERATED_FILES") or []
        if not (generated_files or localized_generated_files):
            return

        for (localized, gen) in (
            (False, generated_files),
            (True, localized_generated_files),
        ):
            for f in gen:
                flags = gen[f]
                outputs = f
                inputs = []
                if flags.script:
                    method = "main"
                    script = SourcePath(context, flags.script).full_path

                    # Deal with cases like "C:\\path\\to\\script.py:function".
                    if ".py:" in script:
                        script, method = script.rsplit(".py:", 1)
                        script += ".py"

                    if not os.path.exists(script):
                        raise SandboxValidationError(
                            "Script for generating %s does not exist: %s" % (f, script),
                            context,
                        )
                    if os.path.splitext(script)[1] != ".py":
                        raise SandboxValidationError(
                            "Script for generating %s does not end in .py: %s"
                            % (f, script),
                            context,
                        )
                else:
                    script = None
                    method = None

                for i in flags.inputs:
                    p = Path(context, i)
                    if isinstance(p, SourcePath) and not os.path.exists(p.full_path):
                        raise SandboxValidationError(
                            "Input for generating %s does not exist: %s"
                            % (f, p.full_path),
                            context,
                        )
                    inputs.append(p)

                yield GeneratedFile(
                    context,
                    script,
                    method,
                    outputs,
                    inputs,
                    flags.flags,
                    localized=localized,
                    force=flags.force,
                )

    def _process_test_manifests(self, context):
        for prefix, info in TEST_MANIFESTS.items():
            for path, manifest in context.get("%s_MANIFESTS" % prefix, []):
                for obj in self._process_test_manifest(context, info, path, manifest):
                    yield obj

        for flavor in REFTEST_FLAVORS:
            for path, manifest in context.get("%s_MANIFESTS" % flavor.upper(), []):
                for obj in self._process_reftest_manifest(
                    context, flavor, path, manifest
                ):
                    yield obj

    def _process_test_manifest(self, context, info, manifest_path, mpmanifest):
        flavor, install_root, install_subdir, package_tests = info

        path = manifest_path.full_path
        manifest_dir = mozpath.dirname(path)
        manifest_reldir = mozpath.dirname(
            mozpath.relpath(path, context.config.topsrcdir)
        )
        manifest_sources = [
            mozpath.relpath(pth, context.config.topsrcdir)
            for pth in mpmanifest.source_files
        ]
        install_prefix = mozpath.join(install_root, install_subdir)

        try:
            if not mpmanifest.tests:
                raise SandboxValidationError("Empty test manifest: %s" % path, context)

            defaults = mpmanifest.manifest_defaults[os.path.normpath(path)]
            obj = TestManifest(
                context,
                path,
                mpmanifest,
                flavor=flavor,
                install_prefix=install_prefix,
                relpath=mozpath.join(manifest_reldir, mozpath.basename(path)),
                sources=manifest_sources,
                dupe_manifest="dupe-manifest" in defaults,
            )

            filtered = mpmanifest.tests

            missing = [t["name"] for t in filtered if not os.path.exists(t["path"])]
            if missing:
                raise SandboxValidationError(
                    "Test manifest (%s) lists "
                    "test that does not exist: %s" % (path, ", ".join(missing)),
                    context,
                )

            out_dir = mozpath.join(install_prefix, manifest_reldir)

            def process_support_files(test):
                install_info = self._test_files_converter.convert_support_files(
                    test, install_root, manifest_dir, out_dir
                )

                obj.pattern_installs.extend(install_info.pattern_installs)
                for source, dest in install_info.installs:
                    obj.installs[source] = (dest, False)
                obj.external_installs |= install_info.external_installs
                for install_path in install_info.deferred_installs:
                    if all(
                        [
                            "*" not in install_path,
                            not os.path.isfile(
                                mozpath.join(context.config.topsrcdir, install_path[2:])
                            ),
                            install_path not in install_info.external_installs,
                        ]
                    ):
                        raise SandboxValidationError(
                            "Error processing test "
                            "manifest %s: entry in support-files not present "
                            "in the srcdir: %s" % (path, install_path),
                            context,
                        )

                obj.deferred_installs |= install_info.deferred_installs

            for test in filtered:
                obj.tests.append(test)

                # Some test files are compiled and should not be copied into the
                # test package. They function as identifiers rather than files.
                if package_tests:
                    manifest_relpath = mozpath.relpath(
                        test["path"], mozpath.dirname(test["manifest"])
                    )
                    obj.installs[mozpath.normpath(test["path"])] = (
                        (mozpath.join(out_dir, manifest_relpath)),
                        True,
                    )

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
            for f in defaults.get("generated-files", "").split():
                # We re-raise otherwise the stack trace isn't informative.
                try:
                    del obj.installs[mozpath.join(manifest_dir, f)]
                except KeyError:
                    raise SandboxValidationError(
                        "Error processing test "
                        "manifest %s: entry in generated-files not present "
                        "elsewhere in manifest: %s" % (path, f),
                        context,
                    )

            yield obj
        except (AssertionError, Exception):
            raise SandboxValidationError(
                "Error processing test "
                "manifest file %s: %s"
                % (path, "\n".join(traceback.format_exception(*sys.exc_info()))),
                context,
            )

    def _process_reftest_manifest(self, context, flavor, manifest_path, manifest):
        manifest_full_path = manifest_path.full_path
        manifest_reldir = mozpath.dirname(
            mozpath.relpath(manifest_full_path, context.config.topsrcdir)
        )

        # reftest manifests don't come from manifest parser. But they are
        # similar enough that we can use the same emitted objects. Note
        # that we don't perform any installs for reftests.
        obj = TestManifest(
            context,
            manifest_full_path,
            manifest,
            flavor=flavor,
            install_prefix="%s/" % flavor,
            relpath=mozpath.join(manifest_reldir, mozpath.basename(manifest_path)),
        )
        obj.tests = list(sorted(manifest.tests, key=lambda t: t["path"]))

        yield obj

    def _process_jar_manifests(self, context):
        jar_manifests = context.get("JAR_MANIFESTS", [])
        if len(jar_manifests) > 1:
            raise SandboxValidationError(
                "While JAR_MANIFESTS is a list, "
                "it is currently limited to one value.",
                context,
            )

        for path in jar_manifests:
            yield JARManifest(context, path)

        # Temporary test to look for jar.mn files that creep in without using
        # the new declaration. Before, we didn't require jar.mn files to
        # declared anywhere (they were discovered). This will detect people
        # relying on the old behavior.
        if os.path.exists(os.path.join(context.srcdir, "jar.mn")):
            if "jar.mn" not in jar_manifests:
                raise SandboxValidationError(
                    "A jar.mn exists but it "
                    "is not referenced in the moz.build file. "
                    "Please define JAR_MANIFESTS.",
                    context,
                )

    def _emit_directory_traversal_from_context(self, context):
        o = DirectoryTraversal(context)
        o.dirs = context.get("DIRS", [])

        # Some paths have a subconfigure, yet also have a moz.build. Those
        # shouldn't end up in self._external_paths.
        if o.objdir:
            self._external_paths -= {o.relobjdir}

        yield o
