# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'sources': [
    'aeskeywrap.c',
    'alg2268.c',
    'alghmac.c',
    'arcfive.c',
    'arcfour.c',
    'blake2b.c',
    'camellia.c',
    'chacha20poly1305.c',
    'crypto_primitives.c',
    'ctr.c',
    'cts.c',
    'des.c',
    'desblapi.c',
    'dh.c',
    'drbg.c',
    'dsa.c',
    'ec.c',
    'ecdecode.c',
    'ecl/ec_naf.c',
    'ecl/ecl.c',
    'ecl/ecl_gf.c',
    'ecl/ecl_mult.c',
    'ecl/ecp_25519.c',
    'ecl/ecp_256.c',
    'ecl/ecp_256_32.c',
    'ecl/ecp_384.c',
    'ecl/ecp_521.c',
    'ecl/ecp_aff.c',
    'ecl/ecp_jac.c',
    'ecl/ecp_jm.c',
    'ecl/ecp_mont.c',
    'fipsfreebl.c',
    'blinit.c',
    'freeblver.c',
    'gcm.c',
    'hmacct.c',
    'jpake.c',
    'ldvector.c',
    'md2.c',
    'md5.c',
    'mpi/mp_gf2m.c',
    'mpi/mpcpucache.c',
    'mpi/mpi.c',
    'mpi/mplogic.c',
    'mpi/mpmontg.c',
    'mpi/mpprime.c',
    'pqg.c',
    'rawhash.c',
    'rijndael.c',
    'rsa.c',
    'rsapkcs.c',
    'seed.c',
    'sha512.c',
    'sha_fast.c',
    'shvfy.c',
    'sysrand.c',
    'tlsprfalg.c',
    'verified/FStar.c',
  ],
  'conditions': [
    [ 'OS=="linux" or OS=="android"', {
      'conditions': [
        [ 'target_arch=="x64"', {
          'sources': [
            'arcfour-amd64-gas.s',
            'intel-aes.s',
            'intel-gcm.s',
            'mpi/mpi_amd64.c',
            'mpi/mpi_amd64_gas.s',
            'mpi/mp_comba.c',
          ],
          'conditions': [
            [ 'cc_is_clang==1', {
              'cflags': [
                '-no-integrated-as',
              ],
              'cflags_mozilla': [
                '-no-integrated-as',
              ],
              'asflags_mozilla': [
                '-no-integrated-as',
              ],
            }],
          ],
        }],
        [ 'target_arch=="ia32"', {
          'sources': [
            'mpi/mpi_x86.s',
          ],
        }],
        [ 'target_arch=="arm"', {
          'sources': [
            'mpi/mpi_arm.c',
          ],
        }],
      ],
    }],
    [ 'OS=="win"', {
      'libraries': [
        'advapi32.lib',
      ],
      'conditions': [
        [ 'cc_use_gnu_ld!=1 and target_arch=="x64"', {
          'sources': [
            'arcfour-amd64-masm.asm',
            'mpi/mpi_amd64.c',
            'mpi/mpi_amd64_masm.asm',
            'mpi/mp_comba_amd64_masm.asm',
            'intel-aes-x64-masm.asm',
            'intel-gcm-x64-masm.asm',
          ],
        }],
	      [ 'cc_use_gnu_ld!=1 and target_arch!="x64"', {
          # not x64
          'sources': [
            'mpi/mpi_x86_asm.c',
            'intel-aes-x86-masm.asm',
            'intel-gcm-x86-masm.asm',
          ],
        }],
        [ 'cc_is_clang!=1', {
          # MSVC
          'sources': [
            'intel-gcm-wrap.c',
          ],
        }],
      ],
    }],
    ['target_arch=="ia32" or target_arch=="x64" or target_arch=="arm64" or target_arch=="aarch64"', {
      'sources': [
        # All intel and 64-bit ARM architectures get the 64 bit version.
        'ecl/curve25519_64.c',
        'verified/Hacl_Curve25519.c',
      ],
    }, {
      'sources': [
        # All other architectures get the generic 32 bit implementation (slow!)
        'ecl/curve25519_32.c',
      ],
    }],
    [ 'disable_chachapoly==0', {
      # The ChaCha20 code is linked in through the static ssse3-crypto lib on
      # all platforms that support SSSE3. There are runtime checks in place to
      # choose the correct ChaCha implementation at runtime.
      'sources': [
        'verified/Hacl_Chacha20.c',
      ],
      'conditions': [
        [ 'OS!="win"', {
          'conditions': [
            [ 'target_arch=="x64"', {
              'sources': [
                'verified/Hacl_Poly1305_64.c',
              ],
            }, {
              # !Windows & !x64
              'conditions': [
                [ 'target_arch=="arm64" or target_arch=="aarch64"', {
                  'sources': [
                    'verified/Hacl_Poly1305_64.c',
                  ],
                }, {
                  # !Windows & !x64 & !arm64 & !aarch64
                  'sources': [
                    'verified/Hacl_Poly1305_32.c',
                  ],
                }],
              ],
            }],
          ],
        }, {
          # Windows
          'sources': [
            'verified/Hacl_Poly1305_32.c',
          ],
        }],
      ],
    }],
    [ 'fuzz==1', {
      'sources!': [ 'drbg.c' ],
      'sources': [ 'det_rng.c' ],
    }],
    [ 'fuzz_tls==1', {
      'defines': [
        'UNSAFE_FUZZER_MODE',
      ],
    }],
    [ 'ct_verif==1', {
      'defines': [
        'CT_VERIF',
      ],
    }],
    [ 'only_dev_random==1', {
      'defines': [
        'SEED_ONLY_DEV_URANDOM',
      ]
    }],
    [ 'OS=="mac"', {
      'conditions': [
        [ 'target_arch=="ia32"', {
          'sources': [
            'mpi/mpi_sse2.s',
          ],
          'defines': [
            'MP_USE_UINT_DIGIT',
            'MP_ASSEMBLY_MULTIPLY',
            'MP_ASSEMBLY_SQUARE',
            'MP_ASSEMBLY_DIV_2DX1D',
          ],
        }],
      ],
    }],
  ],
 'ldflags': [
   '-Wl,-Bsymbolic'
 ],
}
