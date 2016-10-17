# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'jar',
      'type': 'static_library',
      'sources': [
        'jar-ds.c',
        'jar.c',
        'jarfile.c',
        'jarint.c',
        'jarsign.c',
        'jarver.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    }
  ],
  'target_defaults': {
    'defines': [
      'MOZILLA_CLIENT=1',
    ],
    'conditions': [
      [ 'OS=="win"', {
        'configurations': {
          'x86_Base': {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'PreprocessorDefinitions': [
                  'NSS_X86_OR_X64',
                  'NSS_X86',
                ],
              },
            },
          },
          'x64_Base': {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'PreprocessorDefinitions': [
                  'NSS_USE_64',
                  'NSS_X86_OR_X64',
                  'NSS_X64',
                ],
              },
            },
          },
        },
      }, {
        'conditions': [
          [ 'target_arch=="x64"', {
            'defines': [
              'NSS_USE_64',
              'NSS_X86_OR_X64',
              'NSS_X64',
            ],
          }],
          [ 'target_arch=="ia32"', {
            'defines': [
              'NSS_X86_OR_X64',
              'NSS_X86',
            ],
          }],
        ],
      }],
    ],
  },
  'variables': {
    'module': 'nss'
  }
}
