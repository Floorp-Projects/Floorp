# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'libFuzzer',
      'type': 'static_library',
      'sources': [
        'FuzzerCrossOver.cpp',
        'FuzzerDriver.cpp',
        'FuzzerExtFunctionsDlsym.cpp',
        'FuzzerExtFunctionsWeak.cpp',
        'FuzzerIO.cpp',
        'FuzzerLoop.cpp',
        'FuzzerMutate.cpp',
        'FuzzerSHA1.cpp',
        'FuzzerTracePC.cpp',
        'FuzzerTraceState.cpp',
        'FuzzerUtil.cpp',
        'FuzzerUtilDarwin.cpp',
        'FuzzerUtilLinux.cpp',
      ],
      'cflags': [
        '-O2',
      ],
      'cflags/': [
        ['exclude', '-fsanitize='],
        ['exclude', '-fsanitize-'],
      ],
      'xcode_settings': {
        'GCC_OPTIMIZATION_LEVEL': '2', # -O2
        'OTHER_CFLAGS/': [
          ['exclude', '-fsanitize='],
          ['exclude', '-fsanitize-'],
        ],
      },
    }
  ],
}
