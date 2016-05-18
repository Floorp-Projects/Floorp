# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import itertools
import json
import os

import mozpack.path as mozpath

from mozbuild.backend.base import BuildBackend

from mozbuild.frontend.context import (
    Context,
    Path,
    RenamedSourcePath,
    VARIABLES,
)
from mozbuild.frontend.data import (
    BaseProgram,
    ChromeManifestEntry,
    ConfigFileSubstitution,
    ExampleWebIDLInterface,
    IPDLFile,
    FinalTargetPreprocessedFiles,
    FinalTargetFiles,
    GeneratedEventWebIDLFile,
    GeneratedWebIDLFile,
    PreprocessedTestWebIDLFile,
    PreprocessedWebIDLFile,
    RustRlibLibrary,
    SharedLibrary,
    TestManifest,
    TestWebIDLFile,
    UnifiedSources,
    XPIDLFile,
    WebIDLFile,
)
from mozbuild.jar import (
    DeprecatedJarManifest,
    JarManifestParser,
)
from mozbuild.preprocessor import Preprocessor
from mozpack.chrome.manifest import parse_manifest_line

from collections import defaultdict

from mozbuild.util import group_unified_files

class XPIDLManager(object):
    """Helps manage XPCOM IDLs in the context of the build system."""
    def __init__(self, config):
        self.config = config
        self.topsrcdir = config.topsrcdir
        self.topobjdir = config.topobjdir

        self.idls = {}
        self.modules = {}
        self.interface_manifests = {}
        self.chrome_manifests = set()

    def register_idl(self, idl, allow_existing=False):
        """Registers an IDL file with this instance.

        The IDL file will be built, installed, etc.
        """
        basename = mozpath.basename(idl.source_path)
        root = mozpath.splitext(basename)[0]
        xpt = '%s.xpt' % idl.module
        manifest = mozpath.join(idl.install_target, 'components', 'interfaces.manifest')
        chrome_manifest = mozpath.join(idl.install_target, 'chrome.manifest')

        entry = {
            'source': idl.source_path,
            'module': idl.module,
            'basename': basename,
            'root': root,
            'manifest': manifest,
        }

        if not allow_existing and entry['basename'] in self.idls:
            raise Exception('IDL already registered: %s' % entry['basename'])

        self.idls[entry['basename']] = entry
        t = self.modules.setdefault(entry['module'], (idl.install_target, set()))
        t[1].add(entry['root'])

        if idl.add_to_manifest:
            self.interface_manifests.setdefault(manifest, set()).add(xpt)
            self.chrome_manifests.add(chrome_manifest)


class WebIDLCollection(object):
    """Collects WebIDL info referenced during the build."""

    def __init__(self):
        self.sources = set()
        self.generated_sources = set()
        self.generated_events_sources = set()
        self.preprocessed_sources = set()
        self.test_sources = set()
        self.preprocessed_test_sources = set()
        self.example_interfaces = set()

    def all_regular_sources(self):
        return self.sources | self.generated_sources | \
            self.generated_events_sources | self.preprocessed_sources

    def all_regular_basenames(self):
        return [os.path.basename(source) for source in self.all_regular_sources()]

    def all_regular_stems(self):
        return [os.path.splitext(b)[0] for b in self.all_regular_basenames()]

    def all_regular_bindinggen_stems(self):
        for stem in self.all_regular_stems():
            yield '%sBinding' % stem

        for source in self.generated_events_sources:
            yield os.path.splitext(os.path.basename(source))[0]

    def all_regular_cpp_basenames(self):
        for stem in self.all_regular_bindinggen_stems():
            yield '%s.cpp' % stem

    def all_test_sources(self):
        return self.test_sources | self.preprocessed_test_sources

    def all_test_basenames(self):
        return [os.path.basename(source) for source in self.all_test_sources()]

    def all_test_stems(self):
        return [os.path.splitext(b)[0] for b in self.all_test_basenames()]

    def all_test_cpp_basenames(self):
        return ['%sBinding.cpp' % s for s in self.all_test_stems()]

    def all_static_sources(self):
        return self.sources | self.generated_events_sources | \
            self.test_sources

    def all_non_static_sources(self):
        return self.generated_sources | self.all_preprocessed_sources()

    def all_non_static_basenames(self):
        return [os.path.basename(s) for s in self.all_non_static_sources()]

    def all_preprocessed_sources(self):
        return self.preprocessed_sources | self.preprocessed_test_sources

    def all_sources(self):
        return set(self.all_regular_sources()) | set(self.all_test_sources())

    def all_basenames(self):
        return [os.path.basename(source) for source in self.all_sources()]

    def all_stems(self):
        return [os.path.splitext(b)[0] for b in self.all_basenames()]

    def generated_events_basenames(self):
        return [os.path.basename(s) for s in self.generated_events_sources]

    def generated_events_stems(self):
        return [os.path.splitext(b)[0] for b in self.generated_events_basenames()]


class TestManager(object):
    """Helps hold state related to tests."""

    def __init__(self, config):
        self.config = config
        self.topsrcdir = mozpath.normpath(config.topsrcdir)

        self.tests_by_path = defaultdict(list)
        self.installs_by_path = defaultdict(list)
        self.deferred_installs = set()
        self.manifest_default_support_files = {}

    def add(self, t, flavor, topsrcdir, default_supp_files):
        t = dict(t)
        t['flavor'] = flavor

        path = mozpath.normpath(t['path'])
        assert mozpath.basedir(path, [topsrcdir])

        key = path[len(topsrcdir)+1:]
        t['file_relpath'] = key
        t['dir_relpath'] = mozpath.dirname(key)

        # Support files are propagated from the default section to individual
        # tests by the manifest parser, but we end up storing a lot of
        # redundant data due to the huge number of support files.
        # So if we have support files that are the same as the manifest default
        # we track that separately, per-manifest instead of per-test, to save
        # space.
        supp_files = t.get('support-files')
        if supp_files and supp_files == default_supp_files:
            self.manifest_default_support_files[t['manifest']] = default_supp_files
            del t['support-files']

        self.tests_by_path[key].append(t)

    def add_installs(self, obj, topsrcdir):
        for src, (dest, _) in obj.installs.iteritems():
            key = src[len(topsrcdir)+1:]
            self.installs_by_path[key].append((src, dest))
        for src, pat, dest in obj.pattern_installs:
            key = mozpath.join(src[len(topsrcdir)+1:], pat)
            self.installs_by_path[key].append((src, pat, dest))
        for path in obj.deferred_installs:
            self.deferred_installs.add(path[2:])


class BinariesCollection(object):
    """Tracks state of binaries produced by the build."""

    def __init__(self):
        self.shared_libraries = []
        self.programs = []


class CommonBackend(BuildBackend):
    """Holds logic common to all build backends."""

    def _init(self):
        self._idl_manager = XPIDLManager(self.environment)
        self._test_manager = TestManager(self.environment)
        self._webidls = WebIDLCollection()
        self._binaries = BinariesCollection()
        self._configs = set()
        self._ipdl_sources = set()

    def consume_object(self, obj):
        self._configs.add(obj.config)

        if isinstance(obj, TestManifest):
            for test in obj.tests:
                self._test_manager.add(test, obj.flavor, obj.topsrcdir,
                                       obj.default_support_files)
            self._test_manager.add_installs(obj, obj.topsrcdir)

        elif isinstance(obj, XPIDLFile):
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

        # We should consider aggregating WebIDL types in emitter.py.
        elif isinstance(obj, WebIDLFile):
            # WebIDL isn't relevant to artifact builds.
            if self.environment.is_artifact_build:
                return True

            self._webidls.sources.add(mozpath.join(obj.srcdir, obj.basename))

        elif isinstance(obj, GeneratedEventWebIDLFile):
            # WebIDL isn't relevant to artifact builds.
            if self.environment.is_artifact_build:
                return True

            self._webidls.generated_events_sources.add(mozpath.join(
                obj.srcdir, obj.basename))

        elif isinstance(obj, TestWebIDLFile):
            # WebIDL isn't relevant to artifact builds.
            if self.environment.is_artifact_build:
                return True

            self._webidls.test_sources.add(mozpath.join(obj.srcdir,
                obj.basename))

        elif isinstance(obj, PreprocessedTestWebIDLFile):
            # WebIDL isn't relevant to artifact builds.
            if self.environment.is_artifact_build:
                return True

            self._webidls.preprocessed_test_sources.add(mozpath.join(
                obj.srcdir, obj.basename))

        elif isinstance(obj, GeneratedWebIDLFile):
            # WebIDL isn't relevant to artifact builds.
            if self.environment.is_artifact_build:
                return True

            self._webidls.generated_sources.add(mozpath.join(obj.srcdir,
                obj.basename))

        elif isinstance(obj, PreprocessedWebIDLFile):
            # WebIDL isn't relevant to artifact builds.
            if self.environment.is_artifact_build:
                return True

            self._webidls.preprocessed_sources.add(mozpath.join(
                obj.srcdir, obj.basename))

        elif isinstance(obj, ExampleWebIDLInterface):
            # WebIDL isn't relevant to artifact builds.
            if self.environment.is_artifact_build:
                return True

            self._webidls.example_interfaces.add(obj.name)

        elif isinstance(obj, IPDLFile):
            # IPDL isn't relevant to artifact builds.
            if self.environment.is_artifact_build:
                return True

            self._ipdl_sources.add(mozpath.join(obj.srcdir, obj.basename))

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

        else:
            return False

        return True

    def consume_finished(self):
        if len(self._idl_manager.idls):
            self._handle_idl_manager(self._idl_manager)

        self._handle_webidl_collection(self._webidls)

        sorted_ipdl_sources = list(sorted(self._ipdl_sources))

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

        ipdl_dir = mozpath.join(self.environment.topobjdir, 'ipc', 'ipdl')

        ipdl_cppsrcs = list(itertools.chain(*[files_from(p) for p in sorted_ipdl_sources]))
        unified_source_mapping = list(group_unified_files(ipdl_cppsrcs,
                                                          unified_prefix='UnifiedProtocols',
                                                          unified_suffix='cpp',
                                                          files_per_unified_file=16))

        self._write_unified_files(unified_source_mapping, ipdl_dir, poison_windows_h=False)
        self._handle_ipdl_sources(ipdl_dir, sorted_ipdl_sources, unified_source_mapping)

        for config in self._configs:
            self.backend_input_files.add(config.source)

        # Write out a machine-readable file describing every test.
        topobjdir = self.environment.topobjdir
        with self._write_file(mozpath.join(topobjdir, 'all-tests.json')) as fh:
            json.dump([self._test_manager.tests_by_path,
                       self._test_manager.manifest_default_support_files], fh)

        path = mozpath.join(self.environment.topobjdir, 'test-installs.json')
        with self._write_file(path) as fh:
            json.dump({k: v for k, v in self._test_manager.installs_by_path.items()
                       if k in self._test_manager.deferred_installs},
                      fh,
                      sort_keys=True,
                      indent=4)

        # Write out a machine-readable file describing binaries.
        with self._write_file(mozpath.join(topobjdir, 'binaries.json')) as fh:
            d = {
                'shared_libraries': [s.to_dict() for s in self._binaries.shared_libraries],
                'programs': [p.to_dict() for p in self._binaries.programs],
            }
            json.dump(d, fh, sort_keys=True, indent=4)

    def _handle_webidl_collection(self, webidls):
        if not webidls.all_stems():
            return

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

        # Bindings are compiled in unified mode to speed up compilation and
        # to reduce linker memory size. Note that test bindings are separated
        # from regular ones so tests bindings aren't shipped.
        unified_source_mapping = list(group_unified_files(webidls.all_regular_cpp_basenames(),
                                                          unified_prefix='UnifiedBindings',
                                                          unified_suffix='cpp',
                                                          files_per_unified_file=32))
        self._write_unified_files(unified_source_mapping, bindings_dir,
                                  poison_windows_h=True)
        self._handle_webidl_build(bindings_dir, unified_source_mapping,
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
