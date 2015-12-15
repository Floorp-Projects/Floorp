# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import itertools
import json
import os
import re

import mozpack.path as mozpath
import mozwebidlcodegen

from .base import BuildBackend

from ..frontend.data import (
    ConfigFileSubstitution,
    ExampleWebIDLInterface,
    IPDLFile,
    GeneratedEventWebIDLFile,
    GeneratedWebIDLFile,
    PreprocessedTestWebIDLFile,
    PreprocessedWebIDLFile,
    TestManifest,
    TestWebIDLFile,
    UnifiedSources,
    XPIDLFile,
    WebIDLFile,
)

from collections import defaultdict

from ..util import (
    group_unified_files,
)

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

    def add(self, t, flavor=None, topsrcdir=None):
        t = dict(t)
        t['flavor'] = flavor

        if topsrcdir is None:
            topsrcdir = self.topsrcdir
        else:
            topsrcdir = mozpath.normpath(topsrcdir)

        path = mozpath.normpath(t['path'])
        assert mozpath.basedir(path, [topsrcdir])

        key = path[len(topsrcdir)+1:]
        t['file_relpath'] = key
        t['dir_relpath'] = mozpath.dirname(key)

        self.tests_by_path[key].append(t)


class CommonBackend(BuildBackend):
    """Holds logic common to all build backends."""

    def _init(self):
        self._idl_manager = XPIDLManager(self.environment)
        self._test_manager = TestManager(self.environment)
        self._webidls = WebIDLCollection()
        self._configs = set()
        self._ipdl_sources = set()

    def consume_object(self, obj):
        self._configs.add(obj.config)

        if isinstance(obj, TestManifest):
            for test in obj.tests:
                self._test_manager.add(test, flavor=obj.flavor,
                    topsrcdir=obj.topsrcdir)

        elif isinstance(obj, XPIDLFile):
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
            self._webidls.sources.add(mozpath.join(obj.srcdir, obj.basename))

        elif isinstance(obj, GeneratedEventWebIDLFile):
            self._webidls.generated_events_sources.add(mozpath.join(
                obj.srcdir, obj.basename))

        elif isinstance(obj, TestWebIDLFile):
            self._webidls.test_sources.add(mozpath.join(obj.srcdir,
                obj.basename))

        elif isinstance(obj, PreprocessedTestWebIDLFile):
            self._webidls.preprocessed_test_sources.add(mozpath.join(
                obj.srcdir, obj.basename))

        elif isinstance(obj, GeneratedWebIDLFile):
            self._webidls.generated_sources.add(mozpath.join(obj.srcdir,
                obj.basename))

        elif isinstance(obj, PreprocessedWebIDLFile):
            self._webidls.preprocessed_sources.add(mozpath.join(
                obj.srcdir, obj.basename))

        elif isinstance(obj, ExampleWebIDLInterface):
            self._webidls.example_interfaces.add(obj.name)

        elif isinstance(obj, IPDLFile):
            self._ipdl_sources.add(mozpath.join(obj.srcdir, obj.basename))

        elif isinstance(obj, UnifiedSources):
            if obj.have_unified_mapping:
                self._write_unified_files(obj.unified_source_mapping, obj.objdir)
            if hasattr(self, '_process_unified_sources'):
                self._process_unified_sources(obj)
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
        path = mozpath.join(self.environment.topobjdir, 'all-tests.json')
        with self._write_file(path) as fh:
            s = json.dumps(self._test_manager.tests_by_path)
            fh.write(s)

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
