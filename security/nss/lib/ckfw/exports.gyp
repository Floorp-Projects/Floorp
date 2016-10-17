# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_ckfw_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'nssck.api',
            'nssckepv.h',
            'nssckft.h',
            'nssckfw.h',
            'nssckfwc.h',
            'nssckfwt.h',
            'nssckg.h',
            'nssckmdt.h',
            'nssckt.h'
          ],
          'destination': '<(PRODUCT_DIR)/dist/<(module)/public'
        },
        {
          'files': [
            'ck.h',
            'ckfw.h',
            'ckfwm.h',
            'ckfwtm.h',
            'ckmd.h',
            'ckt.h'
          ],
          'destination': '<(PRODUCT_DIR)/dist/<(module)/private'
        }
      ]
    }
  ],
  'variables': {
    'module': 'nss'
  }
}