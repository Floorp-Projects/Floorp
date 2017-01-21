# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../coreconf/config.gypi',
  ],
  'variables': {
    'use_fuzzing_engine': '<!(test -f /usr/lib/libFuzzingEngine.a && echo 1 || echo 0)',
  },
  'target_defaults': {
    'variables': {
      'debug_optimization_level': '2',
    },
    'target_conditions': [
      [ '_type=="executable"', {
        'libraries!': [
          '<@(nspr_libs)',
        ],
        'libraries': [
          '<(nss_dist_obj_dir)/lib/libplds4.a',
          '<(nss_dist_obj_dir)/lib/libnspr4.a',
          '<(nss_dist_obj_dir)/lib/libplc4.a',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'fuzz_base',
      'dependencies': [
        '<(DEPTH)/lib/certdb/certdb.gyp:certdb',
        '<(DEPTH)/lib/certhigh/certhigh.gyp:certhi',
        '<(DEPTH)/lib/cryptohi/cryptohi.gyp:cryptohi',
        '<(DEPTH)/lib/base/base.gyp:nssb',
        '<(DEPTH)/lib/dev/dev.gyp:nssdev',
        '<(DEPTH)/lib/pki/pki.gyp:nsspki',
        '<(DEPTH)/lib/util/util.gyp:nssutil',
        '<(DEPTH)/lib/nss/nss.gyp:nss_static',
        '<(DEPTH)/lib/pk11wrap/pk11wrap.gyp:pk11wrap',
        '<(DEPTH)/lib/pkcs7/pkcs7.gyp:pkcs7',
      ],
      'conditions': [
        ['use_fuzzing_engine==0', {
          'type': 'static_library',
          'sources': [
            'libFuzzer/FuzzerCrossOver.cpp',
            'libFuzzer/FuzzerDriver.cpp',
            'libFuzzer/FuzzerExtFunctionsDlsym.cpp',
            'libFuzzer/FuzzerExtFunctionsWeak.cpp',
            'libFuzzer/FuzzerExtFunctionsWeakAlias.cpp',
            'libFuzzer/FuzzerIO.cpp',
            'libFuzzer/FuzzerIOPosix.cpp',
            'libFuzzer/FuzzerIOWindows.cpp',
            'libFuzzer/FuzzerLoop.cpp',
            'libFuzzer/FuzzerMain.cpp',
            'libFuzzer/FuzzerMerge.cpp',
            'libFuzzer/FuzzerMutate.cpp',
            'libFuzzer/FuzzerSHA1.cpp',
            'libFuzzer/FuzzerTracePC.cpp',
            'libFuzzer/FuzzerTraceState.cpp',
            'libFuzzer/FuzzerUtil.cpp',
            'libFuzzer/FuzzerUtilDarwin.cpp',
            'libFuzzer/FuzzerUtilLinux.cpp',
            'libFuzzer/FuzzerUtilPosix.cpp',
            'libFuzzer/FuzzerUtilWindows.cpp',
          ],
          'cflags/': [
            ['exclude', '-fsanitize-coverage'],
          ],
          'xcode_settings': {
            'OTHER_CFLAGS/': [
              ['exclude', '-fsanitize-coverage'],
            ],
          },
          'direct_dependent_settings': {
            'include_dirs': [
              'libFuzzer',
            ],
          },
        }, {
          'type': 'none',
          'direct_dependent_settings': {
            'libraries': ['-lFuzzingEngine'],
          }
        }]
      ],
    },
    {
      'target_name': 'nssfuzz-pkcs8',
      'type': 'executable',
      'sources': [
        'asn1_mutators.cc',
        'initialize.cc',
        'pkcs8_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'fuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-quickder',
      'type': 'executable',
      'sources': [
        'asn1_mutators.cc',
        'initialize.cc',
        'quickder_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'fuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-hash',
      'type': 'executable',
      'sources': [
        'hash_target.cc',
        'initialize.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'fuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz',
      'type': 'none',
      'dependencies': [
        'nssfuzz-hash',
        'nssfuzz-pkcs8',
        'nssfuzz-quickder',
      ],
    }
  ],
}
