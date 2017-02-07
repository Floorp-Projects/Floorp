# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../coreconf/config.gypi',
  ],
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
        '<(DEPTH)/lib/pkcs7/pkcs7.gyp:pkcs7',
        # This is a static build of pk11wrap, softoken, and freebl.
        '<(DEPTH)/lib/pk11wrap/pk11wrap.gyp:pk11wrap_static',
      ],
      'conditions': [
        ['fuzz_oss==0', {
          'type': 'static_library',
          'sources': [
            '<!@(ls <(DEPTH)/fuzz/libFuzzer/*.cpp)',
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
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'fuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-mpi',
      'type': 'executable',
      'sources': [
        'mpi_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'fuzz_base',
      ],
      'conditions': [
        [ 'fuzz_oss==1', {
          'libraries': [
            '/usr/lib/x86_64-linux-gnu/libcrypto.a',
          ],
        }, {
          'libraries': [
            '-lcrypto',
          ],
        }],
      ],
      'include_dirs': [
        '<(DEPTH)/lib/freebl/mpi',
      ],
    },
    {
      'target_name': 'nssfuzz-certDN',
      'type': 'executable',
      'sources': [
        'certDN_target.cc',
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
        'nssfuzz-certDN',
        'nssfuzz-hash',
        'nssfuzz-pkcs8',
        'nssfuzz-quickder',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            'nssfuzz-mpi',
          ],
        }],
      ],
    }
  ],
}
