# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    'coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'nss_libs',
      'type': 'none',
      'dependencies': [
        'lib/ckfw/builtins/builtins.gyp:nssckbi',
        'lib/freebl/freebl.gyp:freebl3',
        'lib/softoken/softoken.gyp:softokn3',
      ],
      'conditions': [
        [ 'moz_fold_libs==0', {
          'dependencies': [
            'lib/nss/nss.gyp:nss3',
            'lib/smime/smime.gyp:smime3',
            'lib/sqlite/sqlite.gyp:sqlite3',
            'lib/ssl/ssl.gyp:ssl3',
            'lib/util/util.gyp:nssutil3',
          ],
        }],
        [ 'OS=="linux"', {
          'dependencies': [
            'lib/freebl/freebl.gyp:freeblpriv3',
            'lib/sysinit/sysinit.gyp:nsssysinit',
          ],
        }],
        [ 'disable_dbm==0', {
          'dependencies': [
            'lib/softoken/legacydb/legacydb.gyp:nssdbm3',
          ],
        }],
      ],
    },
    {
      'target_name': 'nss_static_libs',
      'type': 'none',
      'dependencies': [
        'cmd/lib/lib.gyp:sectool',
        'lib/base/base.gyp:nssb',
        'lib/certdb/certdb.gyp:certdb',
        'lib/certhigh/certhigh.gyp:certhi',
        'lib/ckfw/ckfw.gyp:nssckfw',
        'lib/crmf/crmf.gyp:crmf',
        'lib/cryptohi/cryptohi.gyp:cryptohi',
        'lib/dev/dev.gyp:nssdev',
        'lib/freebl/freebl.gyp:freebl',
        'lib/jar/jar.gyp:jar',
        'lib/nss/nss.gyp:nss_static',
        'lib/pk11wrap/pk11wrap.gyp:pk11wrap',
        'lib/pkcs12/pkcs12.gyp:pkcs12',
        'lib/pkcs7/pkcs7.gyp:pkcs7',
        'lib/pki/pki.gyp:nsspki',
        'lib/smime/smime.gyp:smime',
        'lib/softoken/softoken.gyp:softokn',
        'lib/ssl/ssl.gyp:ssl',
        'lib/util/util.gyp:nssutil'
      ],
      'conditions': [
        [ 'OS=="linux"', {
          'dependencies': [
            'lib/sysinit/sysinit.gyp:nsssysinit_static',
          ],
        }],
        [ 'disable_dbm==0', {
          'dependencies': [
            'lib/dbm/src/src.gyp:dbm',
            'lib/softoken/legacydb/legacydb.gyp:nssdbm',
          ],
        }],
        [ 'disable_libpkix==0', {
          'dependencies': [
            'lib/libpkix/pkix/certsel/certsel.gyp:pkixcertsel',
            'lib/libpkix/pkix/checker/checker.gyp:pkixchecker',
            'lib/libpkix/pkix/crlsel/crlsel.gyp:pkixcrlsel',
            'lib/libpkix/pkix/params/params.gyp:pkixparams',
            'lib/libpkix/pkix/results/results.gyp:pkixresults',
            'lib/libpkix/pkix/store/store.gyp:pkixstore',
            'lib/libpkix/pkix/top/top.gyp:pkixtop',
            'lib/libpkix/pkix/util/util.gyp:pkixutil',
            'lib/libpkix/pkix_pl_nss/module/module.gyp:pkixmodule',
            'lib/libpkix/pkix_pl_nss/pki/pki.gyp:pkixpki',
            'lib/libpkix/pkix_pl_nss/system/system.gyp:pkixsystem',
          ],
        }],
        [ 'use_system_sqlite==0', {
          'dependencies': [
            'lib/sqlite/sqlite.gyp:sqlite',
          ],
        }],
        [ 'moz_fold_libs==1', {
          'dependencies': [
            'lib/nss/nss.gyp:nss3_static',
            'lib/smime/smime.gyp:smime3_static',
          ],
        }],
      ],
    },
    {
      'target_name': 'nss_cmds',
      'type': 'none',
      'dependencies': [
        'cmd/certutil/certutil.gyp:certutil',
        'cmd/modutil/modutil.gyp:modutil',
        'cmd/pk12util/pk12util.gyp:pk12util',
        'cmd/shlibsign/shlibsign.gyp:shlibsign',
      ],
      'conditions': [
        [ 'mozilla_client==0', {
          'dependencies': [
            'cmd/crlutil/crlutil.gyp:crlutil',
            'cmd/pwdecrypt/pwdecrypt.gyp:pwdecrypt',
            'cmd/signtool/signtool.gyp:signtool',
            'cmd/signver/signver.gyp:signver',
            'cmd/smimetools/smimetools.gyp:cmsutil',
            'cmd/ssltap/ssltap.gyp:ssltap',
            'cmd/symkeyutil/symkeyutil.gyp:symkeyutil',
          ],
        }],
      ],
    },
    {
      'target_name': 'nss_sign_shared_libs',
      'type': 'none',
      'dependencies': [
        'cmd/shlibsign/shlibsign.gyp:shlibsign',
      ],
      'actions': [
        {
          'action_name': 'shlibsign',
          'inputs': [
            '<(nss_dist_obj_dir)/lib/<(dll_prefix)freebl3.<(dll_suffix)',
            '<(nss_dist_obj_dir)/lib/<(dll_prefix)freeblpriv3.<(dll_suffix)',
            '<(nss_dist_obj_dir)/lib/<(dll_prefix)nssdbm3.<(dll_suffix)',
            '<(nss_dist_obj_dir)/lib/<(dll_prefix)softokn3.<(dll_suffix)',
          ],
          'outputs': [
            '<(nss_dist_obj_dir)/lib/<(dll_prefix)freebl3.chk',
            '<(nss_dist_obj_dir)/lib/<(dll_prefix)freeblpriv3.chk',
            '<(nss_dist_obj_dir)/lib/<(dll_prefix)nssdbm3.chk',
            '<(nss_dist_obj_dir)/lib/<(dll_prefix)softokn3.chk'
          ],
          'conditions': [
            ['OS!="linux"', {
              'inputs/': [['exclude', 'freeblpriv']],
              'outputs/': [['exclude', 'freeblpriv']]
            }],
          ],
          'action': ['<(python)', '<(DEPTH)/coreconf/shlibsign.py', '<@(_inputs)']
        }
      ],
    },
  ],
  'conditions': [
    [ 'disable_tests==0', {
      'targets': [
        {
          'target_name': 'nss_tests',
          'type': 'none',
          'dependencies': [
            'cmd/addbuiltin/addbuiltin.gyp:addbuiltin',
            'cmd/atob/atob.gyp:atob',
            'cmd/bltest/bltest.gyp:bltest',
            'cmd/btoa/btoa.gyp:btoa',
            'cmd/certcgi/certcgi.gyp:certcgi',
            'cmd/chktest/chktest.gyp:chktest',
            'cmd/crmftest/crmftest.gyp:crmftest',
            'cmd/dbtest/dbtest.gyp:dbtest',
            'cmd/derdump/derdump.gyp:derdump',
            'cmd/digest/digest.gyp:digest',
            'cmd/ecperf/ecperf.gyp:ecperf',
            'cmd/fbectest/fbectest.gyp:fbectest',
            'cmd/fipstest/fipstest.gyp:fipstest',
            'cmd/httpserv/httpserv.gyp:httpserv',
            'cmd/listsuites/listsuites.gyp:listsuites',
            'cmd/makepqg/makepqg.gyp:makepqg',
            'cmd/multinit/multinit.gyp:multinit',
            'cmd/ocspclnt/ocspclnt.gyp:ocspclnt',
            'cmd/ocspresp/ocspresp.gyp:ocspresp',
            'cmd/oidcalc/oidcalc.gyp:oidcalc',
            'cmd/p7content/p7content.gyp:p7content',
            'cmd/p7env/p7env.gyp:p7env',
            'cmd/p7sign/p7sign.gyp:p7sign',
            'cmd/p7verify/p7verify.gyp:p7verify',
            'cmd/pk11ectest/pk11ectest.gyp:pk11ectest',
            'cmd/pk11gcmtest/pk11gcmtest.gyp:pk11gcmtest',
            'cmd/pk11mode/pk11mode.gyp:pk11mode',
            'cmd/pk1sign/pk1sign.gyp:pk1sign',
            'cmd/pp/pp.gyp:pp',
            'cmd/rsaperf/rsaperf.gyp:rsaperf',
            'cmd/sdrtest/sdrtest.gyp:sdrtest',
            'cmd/selfserv/selfserv.gyp:selfserv',
            'cmd/shlibsign/mangle/mangle.gyp:mangle',
            'cmd/strsclnt/strsclnt.gyp:strsclnt',
            'cmd/tests/tests.gyp:baddbdir',
            'cmd/tests/tests.gyp:conflict',
            'cmd/tests/tests.gyp:dertimetest',
            'cmd/tests/tests.gyp:encodeinttest',
            'cmd/tests/tests.gyp:nonspr10',
            'cmd/tests/tests.gyp:remtest',
            'cmd/tests/tests.gyp:secmodtest',
            'cmd/tstclnt/tstclnt.gyp:tstclnt',
            'cmd/vfychain/vfychain.gyp:vfychain',
            'cmd/vfyserv/vfyserv.gyp:vfyserv',
            'gtests/google_test/google_test.gyp:gtest1',
            'gtests/common/common.gyp:gtests',
            'gtests/der_gtest/der_gtest.gyp:der_gtest',
            'gtests/pk11_gtest/pk11_gtest.gyp:pk11_gtest',
            'gtests/ssl_gtest/ssl_gtest.gyp:ssl_gtest',
            'gtests/util_gtest/util_gtest.gyp:util_gtest',
            'gtests/nss_bogo_shim/nss_bogo_shim.gyp:nss_bogo_shim'
          ],
          'conditions': [
            [ 'OS=="linux"', {
              'dependencies': [
                'cmd/lowhashtest/lowhashtest.gyp:lowhashtest',
              ],
            }],
            [ 'disable_libpkix==0', {
              'dependencies': [
                'cmd/pkix-errcodes/pkix-errcodes.gyp:pkix-errcodes',
              ],
            }],
          ],
        },
      ],
    }],
    [ 'fuzz==1', {
      'targets': [
        {
          'target_name': 'fuzz',
          'type': 'none',
          'actions': [
            {
              'action_name': 'warn_fuzz',
              'action': ['cat', 'fuzz/warning.txt'],
              'inputs': ['fuzz/warning.txt'],
              'ninja_use_console': 1,
              'outputs': ['dummy'],
            }
          ],
        },
      ],
    }],
  ],
}
