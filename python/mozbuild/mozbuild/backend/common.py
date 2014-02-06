# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import json
import os
import re

import mozpack.path as mozpath

from .base import BuildBackend

from ..frontend.data import (
    ConfigFileSubstitution,
    ExampleWebIDLInterface,
    HeaderFileSubstitution,
    GeneratedEventWebIDLFile,
    GeneratedWebIDLFile,
    PreprocessedTestWebIDLFile,
    PreprocessedWebIDLFile,
    TestManifest,
    TestWebIDLFile,
    XPIDLFile,
    WebIDLFile,
)
from ..mozinfo import write_mozinfo
from ..util import DefaultOnReadDict


class XPIDLManager(object):
    """Helps manage XPCOM IDLs in the context of the build system."""
    def __init__(self, config):
        self.config = config
        self.topsrcdir = config.topsrcdir
        self.topobjdir = config.topobjdir

        self.idls = {}
        self.modules = {}

    def register_idl(self, source, module, allow_existing=False):
        """Registers an IDL file with this instance.

        The IDL file will be built, installed, etc.
        """
        basename = mozpath.basename(source)
        root = mozpath.splitext(basename)[0]

        entry = {
            'source': source,
            'module': module,
            'basename': basename,
            'root': root,
        }

        if not allow_existing and entry['basename'] in self.idls:
            raise Exception('IDL already registered: %' % entry['basename'])

        self.idls[entry['basename']] = entry
        self.modules.setdefault(entry['module'], set()).add(entry['root'])


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

        self.tests_by_path = DefaultOnReadDict({}, global_default=[])

    def add(self, t, flavor=None, topsrcdir=None):
        t = dict(t)
        t['flavor'] = flavor

        if topsrcdir is None:
            topsrcdir = self.topsrcdir
        else:
            topsrcdir = mozpath.normpath(topsrcdir)

        path = mozpath.normpath(t['path'])
        assert path.startswith(topsrcdir)

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

        self._write_mozinfo = True # For testing

    def consume_object(self, obj):
        self._configs.add(obj.config)

        if isinstance(obj, TestManifest):
            for test in obj.tests:
                self._test_manager.add(test, flavor=obj.flavor,
                    topsrcdir=obj.topsrcdir)

        elif isinstance(obj, XPIDLFile):
            self._idl_manager.register_idl(obj.source_path, obj.module)

        elif isinstance(obj, ConfigFileSubstitution):
            # Do not handle ConfigFileSubstitution for Makefiles. Leave that
            # to other
            if mozpath.basename(obj.output_path) == 'Makefile':
                return
            with self._get_preprocessor(obj) as pp:
                pp.do_include(obj.input_path)
            self.backend_input_files.add(obj.input_path)

        elif isinstance(obj, HeaderFileSubstitution):
            self._create_config_header(obj)
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

        else:
            return

        obj.ack()

    def consume_finished(self):
        if len(self._idl_manager.idls):
            self._handle_idl_manager(self._idl_manager)

        self._handle_webidl_collection(self._webidls)

        for config in self._configs:
            self.backend_input_files.add(config.source)

        # Write out a JSON file used by test harnesses, other parts of
        # automation.
        if self._write_mozinfo and 'JS_STANDALONE' not in self.environment.substs:
            path = mozpath.join(self.environment.topobjdir, 'mozinfo.json')
            with self._write_file(path) as fh:
                write_mozinfo(fh, self.environment, os.environ)

        # Write out a machine-readable file describing every test.
        path = mozpath.join(self.environment.topobjdir, 'all-tests.json')
        with self._write_file(path) as fh:
            json.dump(self._test_manager.tests_by_path, fh, sort_keys=True,
                indent=2)

    def _create_config_header(self, obj):
        '''Creates the given config header. A config header is generated by
        taking the corresponding source file and replacing some #define/#undef
        occurences:
            "#undef NAME" is turned into "#define NAME VALUE"
            "#define NAME" is unchanged
            "#define NAME ORIGINAL_VALUE" is turned into "#define NAME VALUE"
            "#undef UNKNOWN_NAME" is turned into "/* #undef UNKNOWN_NAME */"
            Whitespaces are preserved.
        '''
        with self._write_file(obj.output_path) as fh, \
             open(obj.input_path, 'rU') as input:
            r = re.compile('^\s*#\s*(?P<cmd>[a-z]+)(?:\s+(?P<name>\S+)(?:\s+(?P<value>\S+))?)?', re.U)
            for l in input:
                m = r.match(l)
                if m:
                    cmd = m.group('cmd')
                    name = m.group('name')
                    value = m.group('value')
                    if name:
                        if name in obj.config.defines:
                            if cmd == 'define' and value:
                                l = l[:m.start('value')] \
                                    + str(obj.config.defines[name]) \
                                    + l[m.end('value'):]
                            elif cmd == 'undef':
                                l = l[:m.start('cmd')] \
                                    + 'define' \
                                    + l[m.end('cmd'):m.end('name')] \
                                    + ' ' \
                                    + str(obj.config.defines[name]) \
                                    + l[m.end('name'):]
                        elif cmd == 'undef':
                           l = '/* ' + l[:m.end('name')] + ' */' + l[m.end('name'):]

                fh.write(l)
