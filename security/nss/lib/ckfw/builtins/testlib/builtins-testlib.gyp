# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'nssckbi-testlib',
      'type': 'shared_library',
      'sources': [
        '../anchor.c',
        '../bfind.c',
        '../binst.c',
        '../bobject.c',
        '../bsession.c',
        '../bslot.c',
        '../btoken.c',
        '../ckbiver.c',
        '../constants.c',
        '<(certdata-testlib_c)',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/ckfw/ckfw.gyp:nssckfw',
        '<(DEPTH)/lib/base/base.gyp:nssb'
      ],
      'actions': [
        {
          'msvs_cygwin_shell': 0,
          'action': [
            'python',
            '../certdata.py',
            'certdata-testlib.txt',
            '<@(_outputs)',
          ],
          'inputs': [
            '../certdata.py',
            '../certdata.perl',
            'certdata-testlib.txt'
          ],
          'outputs': [
            '<(certdata-testlib_c)'
          ],
          'action_name': 'generate_certdata-testlib_c'
        }
      ],
      'variables': {
        'mapfile': '../nssckbi.def',
        'certdata-testlib_c': '<(INTERMEDIATE_DIR)/certdata-testlib.c',
      }
    }
  ],
  'target_defaults': {
    'include_dirs': [
      '.',
      '..'
    ]
  },
  'variables': {
    'module': 'nss',
  }
}
