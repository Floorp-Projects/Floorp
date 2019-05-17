# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import shutil
import tempfile
import unittest

from copy import deepcopy

from mozunit import main

from mozbuild.test.backend.common import BackendTester

import mozpack.path as mozpath

from mozbuild.gn_processor import (
    GnMozbuildWriterBackend,
    find_common_attrs,
)
from mozbuild.backend.recursivemake import RecursiveMakeBackend

from mozbuild.frontend.data import (
    ComputedFlags,
    StaticLibrary,
    Sources,
    UnifiedSources,
)

class TestGnMozbuildWriter(BackendTester):

    def setUp(self):
        self._backup_dir = tempfile.mkdtemp(prefix='mozbuild')
        self._gn_dir = os.path.join(os.path.dirname(__file__),
                                    'data', 'gn-processor', 'trunk')
        shutil.copytree(os.path.join(self._gn_dir),
                        os.path.join(self._backup_dir, 'trunk'))
        super(TestGnMozbuildWriter, self).setUp()

    def tearDown(self):
        shutil.rmtree(self._gn_dir)
        shutil.copytree(os.path.join(self._backup_dir, 'trunk'),
                        self._gn_dir)
        shutil.rmtree(self._backup_dir)
        super(TestGnMozbuildWriter, self).tearDown()

    def test_gn_processor(self):
        # Generate our moz.build files.
        env = self._get_environment('gn-processor')
        self._consume('gn-processor', GnMozbuildWriterBackend, env=env)

        # Read the resulting tree.
        _, objs = self._emit('gn-processor', env=env)

        # The config for this srcdir sets OS_TARGET = Darwin, for which our gn config
        # generates two trivial libraries that link in to the main webrtc library,
        # one source file, one unified source file, and one directory with some
        # local defines and includes set. The following asserts this is what we see when
        # we read the generated moz.build files.
        objs = list(objs)
        expected_srcdirs = set([
            mozpath.join(env.topsrcdir, p)
            for p in ['trunk',
                      'trunk/webrtc/webrtc_gn',
                      'trunk/webrtc/base/mac_base_gn']
        ] + [env.topsrcdir])
        actual_srcdirs = set([
            o.srcdir for o in objs
        ])
        self.assertEqual(expected_srcdirs, actual_srcdirs)

        libs = {o.lib_name: o for o in objs if isinstance(o, StaticLibrary)}
        expected_libs = set(['libwebrtc.a', 'libwebrtc_gn.a', 'libmac_base_gn.a'])
        self.assertEqual(expected_libs, set(libs.keys()))
        # All libs are linked into 'libwebrtc.a'.
        self.assertEqual(set(libs['libwebrtc.a'].linked_libraries),
                         set(libs.values()) - set([libs['libwebrtc.a']]))

        [sources] = [o for o in objs if isinstance(o, Sources)]
        [unified_sources] = [o for o in objs if isinstance(o, UnifiedSources)]
        self.assertEqual(sources.files,
                         [mozpath.join(sources.topsrcdir, 'trunk', 'webrtc',
                                       'build', 'function.cc')])
        self.assertEqual(unified_sources.files,
                         [mozpath.join(unified_sources.topsrcdir, 'trunk', 'webrtc',
                                       'build', 'no_op_function.cc')])

        [flags_obj] = [o for o in objs if isinstance(o, ComputedFlags)
                       if 'DEFINES' in o.flags and o.flags['DEFINES']]
        self.assertEqual(flags_obj.srcdir, mozpath.join(flags_obj.topsrcdir,
                                                        'trunk', 'webrtc', 'webrtc_gn'))
        expected_defines = set([
            '-DUSE_NSS_CERTS=1',
            '-DCHROMIUM_BUILD',
            '-DUSE_X11=1',
            '-DNDEBUG',
            '-DNVALGRIND',
            '-DWEBRTC_MAC',
        ])
        self.assertEqual(set(flags_obj.flags['DEFINES']), expected_defines)

        expected_includes = set([
            '-I' + mozpath.join(flags_obj.topobjdir, 'ipc', 'ipdl', '_ipdlheaders'),
            '-I' + mozpath.join(flags_obj.topsrcdir, 'ipc', 'chromium', 'src'),
            '-I' + mozpath.join(flags_obj.topsrcdir, 'ipc', 'glue'),
            '-I' + mozpath.join(flags_obj.topsrcdir, 'trunk', 'webrtc', 'modules', 'include'),
        ])
        self.assertEqual(set(flags_obj.flags['LOCAL_INCLUDES']), expected_includes)
        self.assertEqual(flags_obj.flags['MOZBUILD_CXXFLAGS'], ['-fstack-protector'])


class TestGnMozbuildFactoring(unittest.TestCase):

    test_attrs = {
        'LOCAL_INCLUDES': [
            '!/ipc/ipdl/_ipdlheaders',
            '/ipc/chromium/src',
            '/ipc/glue',
        ],
        'DEFINES': {
            'UNICODE': True,
            'DEBUG': True,
        },
        'LIBRARY_NAME': 'Foo',
    }

    def test_single_attr_set(self):
        # If we only have one set of attributes they're all considered common.
        single_attrs = deepcopy(self.test_attrs)
        common_attrs = find_common_attrs([single_attrs])
        self.assertFalse(any(v for v in single_attrs.values()))
        self.assertEqual(self.test_attrs, common_attrs)

    def test_list_factor(self):
        list_attrs = deepcopy(self.test_attrs)
        input_attrs = deepcopy(self.test_attrs)
        list_attrs['LOCAL_INCLUDES'] += ['/local/include']
        list_attrs['LOCAL_INCLDUES'] = list(reversed(list_attrs['LOCAL_INCLUDES']))
        common_attrs = find_common_attrs([input_attrs, list_attrs])
        self.assertEqual(list_attrs['LOCAL_INCLUDES'], ['/local/include'])
        self.assertEqual(self.test_attrs, common_attrs)
        self.assertFalse(any(v for v in input_attrs.values()))

    def test_value_factor(self):
        input_attrs = deepcopy(self.test_attrs)
        value_attrs = deepcopy(self.test_attrs)
        value_attrs['LOCAL_INCLUDES'] = ['/local/include']
        value_attrs['DEFINES']['DEBUG'] = False
        value_attrs['DEFINES']['UNICODE'] = False
        common_attrs = find_common_attrs([value_attrs, input_attrs])
        self.assertEqual(common_attrs['LIBRARY_NAME'], 'Foo')
        self.assertNotIn('LIBRARY_NAME', value_attrs)
        self.assertNotIn('LIBRARY_NAME', input_attrs)
        del common_attrs['LIBRARY_NAME']
        self.assertFalse(any(v for v in common_attrs.values()))
        self.assertEqual(input_attrs['LOCAL_INCLUDES'], self.test_attrs['LOCAL_INCLUDES'])
        self.assertEqual(input_attrs['DEFINES'], self.test_attrs['DEFINES'])
        self.assertEqual(value_attrs['LOCAL_INCLUDES'],  ['/local/include'])
        self.assertEqual(value_attrs['DEFINES'],  {'DEBUG': False, 'UNICODE': False})

    def test_dictionary_factor(self):
        input_attrs = deepcopy(self.test_attrs)
        dict_subset_attrs = deepcopy(self.test_attrs)
        overlapping_dict_attrs = deepcopy(self.test_attrs)

        del dict_subset_attrs['DEFINES']['DEBUG']
        overlapping_dict_attrs['DEFINES']['MOZ_DEBUG'] = True

        common_attrs = find_common_attrs([input_attrs, dict_subset_attrs, overlapping_dict_attrs])
        for attr in ('LIBRARY_NAME', 'LOCAL_INCLUDES'):
            self.assertEqual(common_attrs[attr], self.test_attrs[attr])
            self.assertFalse(input_attrs.get(attr))
            self.assertFalse(dict_subset_attrs.get(attr))
            self.assertFalse(overlapping_dict_attrs.get(attr))
        self.assertEqual(common_attrs['DEFINES'], {'UNICODE': True})
        self.assertEqual(dict_subset_attrs['DEFINES'], {})
        self.assertEqual(overlapping_dict_attrs['DEFINES'], {
            'DEBUG': True,
            'MOZ_DEBUG': True,
        })

    def test_common_attrs(self):
        input_attrs = deepcopy(self.test_attrs)
        dict_subset_attrs = deepcopy(self.test_attrs)

        del dict_subset_attrs['DEFINES']
        common_attrs = find_common_attrs([input_attrs, dict_subset_attrs])
        self.assertNotIn('DEFINES', common_attrs)
        expected_common = deepcopy(self.test_attrs)
        del expected_common['DEFINES']
        self.assertEqual(expected_common, common_attrs)
        for attr_set in (input_attrs, dict_subset_attrs):
            self.assertNotIn('LIBRARY_NAME', attr_set)
            self.assertEqual(attr_set['LOCAL_INCLUDES'], [])


if __name__ == '__main__':
    main()
