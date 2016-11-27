# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'crmf',
      'type': 'static_library',
      'sources': [
        'asn1cmn.c',
        'challcli.c',
        'cmmfasn1.c',
        'cmmfchal.c',
        'cmmfrec.c',
        'cmmfresp.c',
        'crmfcont.c',
        'crmfdec.c',
        'crmfenc.c',
        'crmfget.c',
        'crmfpop.c',
        'crmfreq.c',
        'crmftmpl.c',
        'encutil.c',
        'respcli.c',
        'respcmn.c',
        'servget.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports'
      ],
      'variables': {
        # This is purely for the use of the Mozilla build system.
        'no_expand_libs': 1,
      },
    }
  ],
  'variables': {
    'module': 'nss'
  }
}
