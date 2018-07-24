# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import json
import os

import mozpack.path as mozpath

from mozbuild.backend.base import BuildBackend

from mozbuild.frontend.context import (
    Context,
    ObjDirPath,
    Path,
    RenamedSourcePath,
    VARIABLES,
)
from mozbuild.frontend.data import (
    BaseProgram,
    ChromeManifestEntry,
    ConfigFileSubstitution,
    Exports,
    FinalTargetPreprocessedFiles,
    FinalTargetFiles,
    GeneratedSources,
    GnProjectData,
    HostLibrary,
    HostGeneratedSources,
    HostRustLibrary,
    IPDLCollection,
    RustLibrary,
    SharedLibrary,
    StaticLibrary,
    UnifiedSources,
    XPIDLFile,
    WebIDLCollection,
)
from mozbuild.jar import (
    DeprecatedJarManifest,
    JarManifestParser,
)
from mozbuild.preprocessor import Preprocessor
from mozpack.chrome.manifest import parse_manifest_line

from mozbuild.util import (
    group_unified_files,
    mkdir,
)

class XPIDLManager(object):
    """Helps manage XPCOM IDLs in the context of the build system."""
    def __init__(self, config):
        self.config = config
        self.topsrcdir = config.topsrcdir
        self.topobjdir = config.topobjdir

        self.idls = {}
        self.modules = {}

    def register_idl(self, idl):
        """Registers an IDL file with this instance.

        The IDL file will be built, installed, etc.
        """
        basename = mozpath.basename(idl.source_path.full_path)
        dirname = mozpath.dirname(idl.source_path.full_path)
        root = mozpath.splitext(basename)[0]
        xpt = '%s.xpt' % idl.module

        entry = {
            'source': idl.source_path,
            'module': idl.module,
            'basename': basename,
            'root': root,
        }

        if entry['basename'] in self.idls:
            raise Exception('IDL already registered: %s' % entry['basename'])

        self.idls[entry['basename']] = entry
        # First element is a set of interface file basenames (no extension).
        #
        # Second element is a set of directory names where module IDLs
        # can be found.  Yes, we have XPIDL modules with files from
        # multiple directories.
        t = self.modules.setdefault(entry['module'], (set(), set()))
        t[0].add(entry['source'])
        t[1].add(dirname)

class BinariesCollection(object):
    """Tracks state of binaries produced by the build."""

    def __init__(self):
        self.shared_libraries = []
        self.programs = []

class CommonBackend(BuildBackend):
    """Holds logic common to all build backends."""

    def _init(self):
        self._idl_manager = XPIDLManager(self.environment)
        self._binaries = BinariesCollection()
        self._configs = set()
        self._generated_sources = set()

    def consume_object(self, obj):
        self._configs.add(obj.config)

        if isinstance(obj, XPIDLFile):
            # TODO bug 1240134 tracks not processing XPIDL files during
            # artifact builds.
            self._idl_manager.register_idl(obj)

        elif isinstance(obj, ConfigFileSubstitution):
            # Do not handle ConfigFileSubstitution for Makefiles. Leave that
            # to other
            if mozpath.basename(obj.output_path) == 'Makefile':
                return False
            with self._get_preprocessor(obj) as pp:
                pp.do_include(obj.input_path)
            self.backend_input_files.add(obj.input_path)

        elif isinstance(obj, WebIDLCollection):
            self._handle_webidl_collection(obj)

        elif isinstance(obj, IPDLCollection):
            self._handle_generated_sources(mozpath.join(obj.objdir, f)
                                           for f in obj.all_generated_sources())
            self._write_unified_files(obj.unified_source_mapping, obj.objdir,
                                      poison_windows_h=False)
            self._handle_ipdl_sources(obj.objdir,
                                      list(sorted(obj.all_sources())),
                                      list(sorted(obj.all_preprocessed_sources())),
                                      list(sorted(obj.all_regular_sources())),
                                      obj.unified_source_mapping)

        elif isinstance(obj, UnifiedSources):
            # Unified sources aren't relevant to artifact builds.
            if self.environment.is_artifact_build:
                return True

            if obj.have_unified_mapping:
                self._write_unified_files(obj.unified_source_mapping, obj.objdir)
            if hasattr(self, '_process_unified_sources'):
                self._process_unified_sources(obj)

        elif isinstance(obj, BaseProgram):
            self._binaries.programs.append(obj)
            return False

        elif isinstance(obj, SharedLibrary):
            self._binaries.shared_libraries.append(obj)
            return False

        elif isinstance(obj, (GeneratedSources, HostGeneratedSources)):
            self._handle_generated_sources(obj.files)
            return False

        elif isinstance(obj, Exports):
            objdir_files = [f.full_path for path, files in obj.files.walk() for f in files if isinstance(f, ObjDirPath)]
            if objdir_files:
                self._handle_generated_sources(objdir_files)
            return False

        elif isinstance(obj, GnProjectData):
            # These are only handled by special purpose build backends,
            # ignore them here.
            return True

        else:
            return False

        return True

    def consume_finished(self):
        if len(self._idl_manager.idls):
            self._write_rust_xpidl_summary(self._idl_manager)
            self._handle_idl_manager(self._idl_manager)
            self._handle_generated_sources(mozpath.join(self.environment.topobjdir, 'dist/include/%s.h' % idl['root']) for idl in self._idl_manager.idls.values())


        for config in self._configs:
            self.backend_input_files.add(config.source)

        # Write out a machine-readable file describing binaries.
        topobjdir = self.environment.topobjdir
        with self._write_file(mozpath.join(topobjdir, 'binaries.json')) as fh:
            d = {
                'shared_libraries': [s.to_dict() for s in self._binaries.shared_libraries],
                'programs': [p.to_dict() for p in self._binaries.programs],
            }
            json.dump(d, fh, sort_keys=True, indent=4)

        # Write out a file listing generated sources.
        with self._write_file(mozpath.join(topobjdir, 'generated-sources.json')) as fh:
            d = {
                'sources': sorted(self._generated_sources),
            }
            json.dump(d, fh, sort_keys=True, indent=4)

    def _expand_libs(self, input_bin):
        os_libs = []
        shared_libs = []
        static_libs = []
        objs = []
        no_pgo_objs = []

        seen_objs = set()
        seen_pgo_gen_only_objs = set()
        seen_libs = set()

        def add_objs(lib):
            seen_pgo_gen_only_objs.update(lib.pgo_gen_only_objs)

            for o in lib.objs:
                if o in seen_objs:
                    continue

                # The front end should keep pgo generate-only objects and
                # normal objects separate.
                assert o not in seen_pgo_gen_only_objs

                seen_objs.add(o)
                objs.append(o)
                # This is slightly odd, but for consistency with the
                # recursivemake backend we don't replace OBJ_SUFFIX if any
                # object in a library has `no_pgo` set.
                if lib.no_pgo_objs or lib.no_pgo:
                    no_pgo_objs.append(o)

        def expand(lib, recurse_objs, system_libs):
            if isinstance(lib, StaticLibrary):
                if lib.no_expand_lib:
                    static_libs.append(lib)
                    recurse_objs = False
                elif recurse_objs:
                    add_objs(lib)

                for l in lib.linked_libraries:
                    expand(l, recurse_objs, system_libs)

                if system_libs:
                    for l in lib.linked_system_libs:
                        if l not in seen_libs:
                            seen_libs.add(l)
                            os_libs.append(l)

            elif isinstance(lib, SharedLibrary):
                if lib not in seen_libs:
                    seen_libs.add(lib)
                    shared_libs.append(lib)

        add_objs(input_bin)

        system_libs = not isinstance(input_bin, StaticLibrary)
        for lib in input_bin.linked_libraries:
            if isinstance(lib, RustLibrary):
                continue
            elif isinstance(lib, StaticLibrary):
                expand(lib, True, system_libs)
            elif isinstance(lib, SharedLibrary):
                if lib not in seen_libs:
                    seen_libs.add(lib)
                    shared_libs.append(lib)

        for lib in input_bin.linked_system_libs:
            if lib not in seen_libs:
                seen_libs.add(lib)
                os_libs.append(lib)

        return (objs, sorted(seen_pgo_gen_only_objs), no_pgo_objs, \
                shared_libs, os_libs, static_libs)

    def _make_list_file(self, objdir, objs, name):
        if not objs:
            return None
        list_style = self.environment.substs.get('EXPAND_LIBS_LIST_STYLE')
        list_file_path = mozpath.join(objdir, name)
        objs = [os.path.relpath(o, objdir) for o in objs]
        if list_style == 'linkerscript':
            ref = list_file_path
            content = '\n'.join('INPUT("%s")' % o for o in objs)
        elif list_style == 'filelist':
            ref = "-Wl,-filelist," + list_file_path
            content = '\n'.join(objs)
        elif list_style == 'list':
            ref = "@" + list_file_path
            content = '\n'.join(objs)
        else:
            return None

        mkdir(objdir)
        with self._write_file(list_file_path) as fh:
            fh.write(content)

        return ref

    def _handle_generated_sources(self, files):
        self._generated_sources.update(mozpath.relpath(f, self.environment.topobjdir) for f in files)

    def _handle_webidl_collection(self, webidls):

        bindings_dir = mozpath.join(self.environment.topobjdir, 'dom', 'bindings')

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

        import mozwebidlcodegen

        manager = mozwebidlcodegen.create_build_system_manager(
            self.environment.topsrcdir,
            self.environment.topobjdir,
            mozpath.join(self.environment.topobjdir, 'dist')
        )
        self._handle_generated_sources(manager.expected_build_output_files())
        self._write_unified_files(webidls.unified_source_mapping, bindings_dir,
                                  poison_windows_h=True)
        self._handle_webidl_build(bindings_dir, webidls.unified_source_mapping,
                                  webidls,
                                  manager.expected_build_output_files(),
                                  manager.GLOBAL_DEFINE_FILES)

    def _write_unified_file(self, unified_file, source_filenames,
                            output_directory, poison_windows_h=False):
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
                '#ifdef INITGUID\n'
                '#error "%(cppfile)s defines INITGUID, '
                'so it cannot be built in unified mode."\n'
                '#undef INITGUID\n'
                '#endif')
            f.write('\n'.join(includeTemplate % { "cppfile": s } for
                              s in source_filenames))

    def _write_unified_files(self, unified_source_mapping, output_directory,
                             poison_windows_h=False):
        for unified_file, source_filenames in unified_source_mapping:
            self._write_unified_file(unified_file, source_filenames,
                                     output_directory, poison_windows_h)

    def _consume_jar_manifest(self, obj):
        # Ideally, this would all be handled somehow in the emitter, but
        # this would require all the magic surrounding l10n and addons in
        # the recursive make backend to die, which is not going to happen
        # any time soon enough.
        # Notably missing:
        # - DEFINES from config/config.mk
        # - L10n support
        # - The equivalent of -e when USE_EXTENSION_MANIFEST is set in
        #   moz.build, but it doesn't matter in dist/bin.
        pp = Preprocessor()
        if obj.defines:
            pp.context.update(obj.defines.defines)
        pp.context.update(self.environment.defines)
        pp.context.update(
            AB_CD='en-US',
            BUILD_FASTER=1,
        )
        pp.out = JarManifestParser()
        try:
            pp.do_include(obj.path.full_path)
        except DeprecatedJarManifest as e:
            raise DeprecatedJarManifest('Parsing error while processing %s: %s'
                                        % (obj.path.full_path, e.message))
        self.backend_input_files |= pp.includes

        for jarinfo in pp.out:
            jar_context = Context(
                allowed_variables=VARIABLES, config=obj._context.config)
            jar_context.push_source(obj._context.main_path)
            jar_context.push_source(obj.path.full_path)

            install_target = obj.install_target
            if jarinfo.base:
                install_target = mozpath.normpath(
                    mozpath.join(install_target, jarinfo.base))
            jar_context['FINAL_TARGET'] = install_target
            if obj.defines:
                jar_context['DEFINES'] = obj.defines.defines
            files = jar_context['FINAL_TARGET_FILES']
            files_pp = jar_context['FINAL_TARGET_PP_FILES']

            for e in jarinfo.entries:
                if e.is_locale:
                    if jarinfo.relativesrcdir:
                        src = '/%s' % jarinfo.relativesrcdir
                    else:
                        src = ''
                    src = mozpath.join(src, 'en-US', e.source)
                else:
                    src = e.source

                src = Path(jar_context, src)

                if '*' not in e.source and not os.path.exists(src.full_path):
                    if e.is_locale:
                        raise Exception(
                            '%s: Cannot find %s' % (obj.path, e.source))
                    if e.source.startswith('/'):
                        src = Path(jar_context, '!' + e.source)
                    else:
                        # This actually gets awkward if the jar.mn is not
                        # in the same directory as the moz.build declaring
                        # it, but it's how it works in the recursive make,
                        # not that anything relies on that, but it's simpler.
                        src = Path(obj._context, '!' + e.source)

                output_basename = mozpath.basename(e.output)
                if output_basename != src.target_basename:
                    src = RenamedSourcePath(jar_context,
                                            (src, output_basename))
                path = mozpath.dirname(mozpath.join(jarinfo.name, e.output))

                if e.preprocess:
                    if '*' in e.source:
                        raise Exception('%s: Wildcards are not supported with '
                                        'preprocessing' % obj.path)
                    files_pp[path] += [src]
                else:
                    files[path] += [src]

            if files:
                self.consume_object(FinalTargetFiles(jar_context, files))
            if files_pp:
                self.consume_object(
                    FinalTargetPreprocessedFiles(jar_context, files_pp))

            for m in jarinfo.chrome_manifests:
                entry = parse_manifest_line(
                    mozpath.dirname(jarinfo.name),
                    m.replace('%', mozpath.basename(jarinfo.name) + '/'))
                self.consume_object(ChromeManifestEntry(
                    jar_context, '%s.manifest' % jarinfo.name, entry))

    def _write_rust_xpidl_summary(self, manager):
        """Write out a rust file which includes the generated xpcom rust modules"""
        topobjdir = self.environment.topobjdir

        include_tmpl = "include!(concat!(env!(\"MOZ_TOPOBJDIR\"), \"/dist/xpcrs/%s/%s.rs\"))"

        with self._write_file(mozpath.join(topobjdir, 'dist', 'xpcrs', 'rt', 'all.rs')) as fh:
            fh.write("// THIS FILE IS GENERATED - DO NOT EDIT\n\n")
            for idl in manager.idls.values():
                fh.write(include_tmpl % ("rt", idl['root']))
                fh.write(";\n")

        with self._write_file(mozpath.join(topobjdir, 'dist', 'xpcrs', 'bt', 'all.rs')) as fh:
            fh.write("// THIS FILE IS GENERATED - DO NOT EDIT\n\n")
            fh.write("&[\n")
            for idl in manager.idls.values():
                fh.write(include_tmpl % ("bt", idl['root']))
                fh.write(",\n")
            fh.write("]\n")
