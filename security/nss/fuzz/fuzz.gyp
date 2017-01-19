# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../coreconf/config.gypi',
    '../cmd/platlibs.gypi'
  ],
  'targets': [
    {
      'target_name': 'libFuzzer',
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
      'direct_dependent_settings': {
        'include_dirs': [
          'libFuzzer',
        ],
      }
    },
    {
      'target_name': 'nssfuzz-cert',
      'type': 'executable',
      'sources': [
        'asn1_mutators.cc',
        'cert_target.cc',
        'initialize.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'libFuzzer',
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
        'libFuzzer',
      ],
    },
    {
      'target_name': 'nssfuzz-spki',
      'type': 'executable',
      'sources': [
        'asn1_mutators.cc',
        'spki_target.cc',
        'initialize.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'libFuzzer',
      ],
    },
    {
      'target_name': 'nssfuzz',
      'type': 'none',
      'dependencies': [
        'nssfuzz-cert',
        'nssfuzz-pkcs8',
        'nssfuzz-spki',
      ]
    }
  ],
  'target_defaults': {
    'variables': {
      'debug_optimization_level': '2',
    },
    'cflags/': [
      ['exclude', '-fsanitize-coverage'],
    ],
    'xcode_settings': {
      'OTHER_CFLAGS/': [
        ['exclude', '-fsanitize-coverage'],
      ],
    },
  },
  'variables': {
    'module': 'nss',
  }
}
