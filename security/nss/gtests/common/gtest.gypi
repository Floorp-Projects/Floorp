# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'target_defaults': {
    'include_dirs': [
      '<(DEPTH)/gtests/google_test/gtest/include',
      '<(DEPTH)/gtests/common',
      '<(DEPTH)/cpputil',
    ],
    'cflags': [
      '-Wsign-compare',
    ],
    'xcode_settings': {
      'OTHER_CFLAGS': [
        '-Wsign-compare',
      ],
    },
    'conditions': [
      ['OS=="win"', {
        'libraries': [
          '-lws2_32',
        ],
      }],
      ['OS=="android"', {
        'libraries': [
          '-lstdc++',
        ],
      }],
      [ 'fuzz==1', {
        'defines': [
          'UNSAFE_FUZZER_MODE',
        ],
      }],
    ],
    'msvs_settings': {
      'VCCLCompilerTool': {
        'ExceptionHandling': 1,
        'PreprocessorDefinitions': [
          'NOMINMAX',
        ],
      },
    },
  },
}
