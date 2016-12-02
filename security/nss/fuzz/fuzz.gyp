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
        'libFuzzer/FuzzerIO.cpp',
        'libFuzzer/FuzzerLoop.cpp',
        'libFuzzer/FuzzerMutate.cpp',
        'libFuzzer/FuzzerSHA1.cpp',
        'libFuzzer/FuzzerTracePC.cpp',
        'libFuzzer/FuzzerTraceState.cpp',
        'libFuzzer/FuzzerUtil.cpp',
        'libFuzzer/FuzzerUtilDarwin.cpp',
        'libFuzzer/FuzzerUtilLinux.cpp',
      ],
      'cflags': [
        '-O2',
      ],
      'cflags!': [
        '-O1',
      ],
      'cflags/': [
        ['exclude', '-fsanitize'],
      ],
      'xcode_settings': {
        'GCC_OPTIMIZATION_LEVEL': '2', # -O2
        'OTHER_CFLAGS/': [
          ['exclude', '-fsanitize'],
        ],
      },
    },
    {
      'target_name': 'nssfuzz',
      'type': 'executable',
      'sources': [
        'asn1_mutators.cc',
        'nssfuzz.cc',
        'pkcs8_target.cc',
        'quickder_targets.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'libFuzzer',
      ],
      'cflags': [
        '-O2',
      ],
      'cflags!': [
        '-O1',
      ],
      'cflags/': [
        ['exclude', '-fsanitize-coverage'],
      ],
      'xcode_settings': {
        'GCC_OPTIMIZATION_LEVEL': '2', # -O2
        'OTHER_CFLAGS/': [
          ['exclude', '-fsanitize-coverage'],
        ],
      },
    }
  ],
  'target_defaults': {
    'include_dirs': [
      'libFuzzer',
    ],
  },
  'variables': {
    'module': 'nss',
  }
}
