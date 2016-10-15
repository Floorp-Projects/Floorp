# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'smime',
      'type': 'static_library',
      'sources': [
        'cmsarray.c',
        'cmsasn1.c',
        'cmsattr.c',
        'cmscinfo.c',
        'cmscipher.c',
        'cmsdecode.c',
        'cmsdigdata.c',
        'cmsdigest.c',
        'cmsencdata.c',
        'cmsencode.c',
        'cmsenvdata.c',
        'cmsmessage.c',
        'cmspubkey.c',
        'cmsrecinfo.c',
        'cmsreclist.c',
        'cmssigdata.c',
        'cmssiginfo.c',
        'cmsudf.c',
        'cmsutil.c',
        'smimemessage.c',
        'smimeutil.c',
        'smimever.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
      ]
    },
    {
      # This is here so Firefox can link this without having to
      # repeat the list of dependencies.
      'target_name': 'smime3_deps',
      'type': 'none',
      'dependencies': [
        'smime',
        '<(DEPTH)/lib/pkcs12/pkcs12.gyp:pkcs12',
        '<(DEPTH)/lib/pkcs7/pkcs7.gyp:pkcs7'
      ],
    },
    {
      'target_name': 'smime3',
      'type': 'shared_library',
      'dependencies': [
        'smime3_deps',
        '<(DEPTH)/lib/nss/nss.gyp:nss3',
        '<(DEPTH)/lib/util/util.gyp:nssutil3'
      ],
      'variables': {
        'mapfile': 'smime.def'
      }
    }
  ],
  'conditions': [
    [ 'moz_fold_libs==1', {
      'targets': [
        {
          'target_name': 'smime3_static',
          'type': 'static_library',
          'dependencies': [
            'smime3_deps',
          ],
        }
      ],
    }],
  ],
  'variables': {
    'module': 'nss'
  }
}
