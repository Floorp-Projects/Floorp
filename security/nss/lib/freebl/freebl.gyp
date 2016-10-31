# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'intel-gcm-wrap_c_lib',
      'type': 'static_library',
      'sources': [
        'intel-gcm-wrap.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports'
      ],
      'cflags': [
        '-mssse3'
      ],
      'cflags_mozilla': [
        '-mssse3'
      ]
    },
    {
      'target_name': 'freebl',
      'type': 'static_library',
      'sources': [
        'loader.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    },
    {
      'target_name': '<(freebl_name)',
      'type': 'shared_library',
      'sources': [
        'aeskeywrap.c',
        'alg2268.c',
        'alghmac.c',
        'arcfive.c',
        'arcfour.c',
        'camellia.c',
        'chacha20poly1305.c',
        'ctr.c',
        'cts.c',
        'des.c',
        'desblapi.c',
        'det_rng.c',
        'dh.c',
        'drbg.c',
        'dsa.c',
        'ec.c',
        'ecdecode.c',
        'ecl/ec_naf.c',
        'ecl/ecl.c',
        'ecl/ecl_curve.c',
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
        'tlsprfalg.c'
      ],
      'conditions': [
        [ 'OS=="linux"', {
          'sources': [
            'nsslowhash.c',
            'stubs.c',
          ],
          'conditions': [
            [ 'test_build==1', {
              'dependencies': [
                '<(DEPTH)/lib/util/util.gyp:nssutil3',
              ],
            }],
            [ 'target_arch=="x64"', {
              'sources': [
                'arcfour-amd64-gas.s',
                'intel-aes.s',
                'intel-gcm.s',
                'mpi/mpi_amd64.c',
                'mpi/mpi_amd64_gas.s',
                'mpi/mp_comba.c',
              ],
              'dependencies': [
                'intel-gcm-wrap_c_lib',
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
        }, {
          # not Linux
          'conditions': [
            [ 'moz_fold_libs==0', {
              'dependencies': [
                '../util/util.gyp:nssutil3',
              ],
            }, {
              'libraries': [
                '<(moz_folded_library_name)',
              ],
            }],
          ],
        }],
        [ 'OS=="win"', {
          'sources': [
            #TODO: building with mingw should not need this.
            'ecl/uint128.c',
            #TODO: clang-cl needs -msse3 here
            'intel-gcm-wrap.c',
          ],
          'libraries': [
            'advapi32.lib',
          ],
          'conditions': [
            [ 'target_arch=="x64"', {
              'sources': [
                'arcfour-amd64-masm.asm',
                'mpi/mpi_amd64.c',
                'mpi/mpi_amd64_masm.asm',
                'mpi/mp_comba_amd64_masm.asm',
                'intel-aes-x64-masm.asm',
                'intel-gcm-x64-masm.asm',
              ],
            }, {
              # not x64
              'sources': [
                'mpi/mpi_x86_asm.c',
                'intel-aes-x86-masm.asm',
                'intel-gcm-x86-masm.asm',
              ],
            }],
          ],
        }],
        ['target_arch=="ia32" or target_arch=="x64"', {
          'sources': [
            # All intel architectures get the 64 bit version
            'ecl/curve25519_64.c',
          ],
        }, {
          'sources': [
            # All non intel architectures get the generic 32 bit implementation (slow!)
            'ecl/curve25519_32.c',
          ],
        }],
        #TODO uint128.c
        [ 'disable_chachapoly==0', {
          'conditions': [
            [ 'OS!="win" and target_arch=="x64"', {
              'sources': [
                'chacha20_vec.c',
                'poly1305-donna-x64-sse2-incremental-source.c',
              ],
            }, {
              # not x64
              'sources': [
                'chacha20.c',
                'poly1305.c',
              ],
            }],
          ],
        }],
        [ 'fuzz==1', {
          'defines': [
            'UNSAFE_FUZZER_MODE',
          ],
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
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
      ],
      'variables': {
       'conditions': [
         [ 'OS=="linux"', {
           'mapfile': 'freebl_hash_vector.def',
         }, {
           'mapfile': 'freebl.def',
         }],
       ]
      },
      'ldflags': [
        '-Wl,-Bsymbolic'
      ]
    },
  ],
  'conditions': [
    [ 'OS=="linux"', {
      # stub build
      'targets': [
        {
          'target_name': 'freebl3',
          'type': 'shared_library',
          'defines': [
            'FREEBL_NO_DEPEND',
          ],
          'sources': [
            'lowhash_vector.c'
          ],
          'dependencies': [
            '<(DEPTH)/exports.gyp:nss_exports'
          ],
          'variables': {
            'mapfile': 'freebl_hash.def'
          }
        },
      ],
    }],
  ],
  'target_defaults': {
    'include_dirs': [
      'mpi',
      'ecl'
    ],
    'defines': [
      'SHLIB_SUFFIX=\"<(dll_suffix)\"',
      'SHLIB_PREFIX=\"<(dll_prefix)\"',
      'SHLIB_VERSION=\"3\"',
      'SOFTOKEN_SHLIB_VERSION=\"3\"',
      'RIJNDAEL_INCLUDE_TABLES',
      'MP_API_COMPATIBLE'
    ],
    'conditions': [
      [ 'OS=="win"', {
        'configurations': {
          'x86_Base': {
            'msvs_settings': {
              'VCCLCompilerTool': {
                #TODO: -Ox optimize flags
                'PreprocessorDefinitions': [
                  'NSS_X86_OR_X64',
                  'NSS_X86',
                  'MP_ASSEMBLY_MULTIPLY',
                  'MP_ASSEMBLY_SQUARE',
                  'MP_ASSEMBLY_DIV_2DX1D',
                  'MP_USE_UINT_DIGIT',
                  'MP_NO_MP_WORD',
                  'USE_HW_AES',
                  'INTEL_GCM',
                ],
              },
            },
          },
          'x64_Base': {
            'msvs_settings': {
              'VCCLCompilerTool': {
                #TODO: -Ox optimize flags
                'PreprocessorDefinitions': [
                  'NSS_USE_64',
                  'NSS_X86_OR_X64',
                  'NSS_X64',
                  'MP_IS_LITTLE_ENDIAN',
                  'NSS_BEVAND_ARCFOUR',
                  'MPI_AMD64',
                  'MP_ASSEMBLY_MULTIPLY',
                  'NSS_USE_COMBA',
                  'USE_HW_AES',
                  'INTEL_GCM',
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
              # The Makefile does version-tests on GCC, but we're not doing that here.
              'HAVE_INT128_SUPPORT',
            ],
          }, {
            'sources': [
              'ecl/uint128.c',
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
      [ 'OS=="linux"', {
        'defines': [
          'FREEBL_LOWHASH',
        ],
        'conditions': [
          [ 'test_build==0', {
            'defines': [
              'FREEBL_NO_DEPEND',
            ],
          }],
          [ 'target_arch=="x64"', {
            'defines': [
              'MP_IS_LITTLE_ENDIAN',
              'NSS_BEVAND_ARCFOUR',
              'MPI_AMD64',
              'MP_ASSEMBLY_MULTIPLY',
              'NSS_USE_COMBA',
              'USE_HW_AES',
              'INTEL_GCM',
            ],
          }],
          [ 'target_arch=="ia32"', {
            'defines': [
              'MP_IS_LITTLE_ENDIAN',
              'MP_ASSEMBLY_MULTIPLY',
              'MP_ASSEMBLY_SQUARE',
              'MP_ASSEMBLY_DIV_2DX1D',
              'MP_USE_UINT_DIGIT',
            ],
          }],
          [ 'target_arch=="arm"', {
            'defines': [
              'MP_ASSEMBLY_MULTIPLY',
              'MP_ASSEMBLY_SQUARE',
              'MP_USE_UINT_DIGIT',
              'SHA_NO_LONG_LONG',
            ],
          }],
        ],
      }],
      [ 'OS=="mac"', {
      }],
      [ 'OS=="win"', {
      }],
    ],
  },
  'variables': {
    'module': 'nss',
  }
}
