# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'target_defaults': {
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
