# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'variables': {
    'module%': '',
    'variables': {
      'variables': {
        'variables': {
          'python%': 'python',
        },
        # chromium uses pymod_do_main, but gyp doesn't set a sensible
        # Python sys.path (gyp_chromium does).
        'python%': '<(python)',
        'host_arch%': '<!(<(python) <(DEPTH)/coreconf/detect_host_arch.py)',
      },
      'python%': '<(python)',
      'host_arch%': '<(host_arch)',
      'conditions': [
        ['OS=="android"', {
          'target_arch%': 'arm',
        }, {
          # Default architecture we're building for is the architecture we're
          # building on.
          'target_arch%': '<(host_arch)',
        }],
        ['OS=="win"', {
          'use_system_zlib%': 0,
          'nspr_libs%': ['nspr4.lib', 'plc4.lib', 'plds4.lib'],
          #XXX: gyp breaks if these are empty!
          'nspr_lib_dir%': ' ',
          'nspr_include_dir%': ' ',
          'zlib_libs%': [],
          #TODO
          'moz_debug_flags%': '',
          'dll_prefix': '',
          'dll_suffix': 'dll',
        }, {
          # On non-windows, default to a system NSPR.
          'nspr_libs%': ['-lplds4', '-lplc4', '-lnspr4'],
          'nspr_lib_dir%': '<!(<(python) <(DEPTH)/coreconf/nspr_lib_dir.py)',
          'nspr_include_dir%': '<!(<(python) <(DEPTH)/coreconf/nspr_include_dir.py)',
          'use_system_zlib%': 1,
        }],
        ['OS=="linux" or OS=="android"', {
          'zlib_libs%': ['<!@(<(python) <(DEPTH)/coreconf/pkg_config.py --libs zlib)'],
          'moz_debug_flags%': '-gdwarf-2',
          'optimize_flags%': '-O2',
          'dll_prefix': 'lib',
          'dll_suffix': 'so',
        }],
        ['OS=="mac"', {
          'zlib_libs%': ['-lz'],
          'use_system_sqlite%': 1,
          'moz_debug_flags%': '-gdwarf-2 -gfull',
          'optimize_flags%': '-O2',
          'dll_prefix': 'lib',
          'dll_suffix': 'dylib',
        }, {
          'use_system_sqlite%': 0,
        }],
        ['"<(GENERATOR)"=="ninja"', {
          'cc_is_clang%': '<!(<(python) <(DEPTH)/coreconf/check_cc_clang.py)',
        }, {
          'cc_is_clang%': '0',
        }],
      ],
    },
    # Copy conditionally-set variables out one scope.
    'python%': '<(python)',
    'host_arch%': '<(host_arch)',
    'target_arch%': '<(target_arch)',
    'use_system_zlib%': '<(use_system_zlib)',
    'zlib_libs%': ['<@(zlib_libs)'],
    'moz_debug_flags%': '<(moz_debug_flags)',
    'nspr_libs%': ['<@(nspr_libs)'],
    'nspr_lib_dir%': '<(nspr_lib_dir)',
    'nspr_include_dir%': '<(nspr_include_dir)',
    'use_system_sqlite%': '<(use_system_sqlite)',
    'sqlite_libs%': ['-lsqlite3'],
    'dll_prefix': '<(dll_prefix)',
    'dll_suffix': '<(dll_suffix)',
    'cc_is_clang%': '<(cc_is_clang)',
    'disable_tests%': 0,
    'disable_chachapoly%': 0,
    'disable_dbm%': 0,
    'disable_libpkix%': 0,
    'ssl_enable_zlib%': 1,
    'use_asan%': 0,
    'mozilla_client%': 0,
    'moz_fold_libs%': 0,
    'moz_folded_library_name%': '',
  },
  'target_defaults': {
    # Settings specific to targets should go here.
    # This is mostly for linking to libraries.
    'variables': {
      'mapfile%': '',
    },
    'include_dirs': [
      '<(nspr_include_dir)',
      '<(PRODUCT_DIR)/dist/<(module)/private',
    ],
    'conditions': [
      [ 'OS=="linux"', {
        'libraries': [
          '-lpthread',
          '-ldl',
          '-lc',
        ],
      }],
    ],
    'target_conditions': [
      # mapfile handling
      [ 'mapfile!=""', {
        # Work around a gyp bug. Fixed upstream but not in Ubuntu packages:
        # https://chromium.googlesource.com/external/gyp/+/b85ad3e578da830377dbc1843aa4fbc5af17a192%5E%21/
        'sources': [
          '<(DEPTH)/coreconf/empty.c',
        ],
        'xcode_settings': {
          'OTHER_LDFLAGS': [
            '-exported_symbols_list',
            '<(INTERMEDIATE_DIR)/out.>(mapfile)',
          ],
        },
        'conditions': [
          [ 'OS=="linux" or OS=="android"', {
            'ldflags': [
              '-Wl,--version-script,<(INTERMEDIATE_DIR)/out.>(mapfile)',
            ],
          }],
          [ 'OS=="win"', {
            # On Windows, .def files are used directly as sources.
            'sources': [
              '>(mapfile)',
            ],
          }, {
            # On other platforms, .def files need processing.
            'sources': [
              '<(INTERMEDIATE_DIR)/out.>(mapfile)',
            ],
            'actions': [{
              'action_name': 'generate_mapfile',
              'inputs': [
                '>(mapfile)',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/out.>(mapfile)',
              ],
              'action': ['<@(process_map_file)'],
            }],
          }]
        ],
      }],
      [ '_type=="shared_library" or _type=="executable"', {
        'libraries': [
          '<@(nspr_libs)',
        ],
        'library_dirs': [
          '<(nspr_lib_dir)',
        ],
      }],
      # Shared library specific settings.
      [ '_type=="shared_library"', {
        'conditions': [
          [ 'OS=="linux" or OS=="android"', {
            'ldflags': [
              '-Wl,--gc-sections',
              '-Wl,-z,defs',
            ],
          }],
        ],
        'xcode_settings': {
          'DYLIB_INSTALL_NAME_BASE': '@executable_path',
          'DYLIB_COMPATIBILITY_VERSION': '1',
          'DYLIB_CURRENT_VERSION': '1',
          'OTHER_LDFLAGS': [
            '-headerpad_max_install_names',
          ],
        },
        'msvs_settings': {
          'VCLinkerTool': {
            'SubSystem': '2',
          },
        },
      }],
    ],
    'default_configuration': 'Debug',
    'configurations': {
      # Common settings for Debug+Release should go here.
      'Common_Base': {
        'abstract': 1,
        'defines': [
          'NSS_NO_INIT_SUPPORT',
          'USE_UTIL_DIRECTLY',
          'NO_NSPR_10_SUPPORT',
          'SSL_DISABLE_DEPRECATED_CIPHER_SUITE_NAMES',
        ],
        'msvs_configuration_attributes': {
          'OutputDirectory': '$(SolutionDir)$(ConfigurationName)',
          'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'AdditionalIncludeDirectories': ['<(nspr_include_dir)'],
          },
        },
        'xcode_settings': {
          'CLANG_CXX_LANGUAGE_STANDARD': 'c++0x',
          'OTHER_CFLAGS': [
            '-fPIC',
            '-fno-common',
            '-pipe',
          ],
          # TODO:
          # 'GCC_TREAT_WARNINGS_AS_ERRORS'
          # 'WARNING_CFLAGS'
        },
        'conditions': [
          ['OS=="linux" or OS=="android"', {
            'defines': [
              'LINUX2_1',
              'LINUX',
              'linux',
              'HAVE_STRERROR',
              'XP_UNIX',
              '_REENTRANT',
            ],
            'cflags': [
              '-fPIC',
              '-pipe',
              '-ffunction-sections',
              '-fdata-sections',
              '<(moz_debug_flags)',
            ],
            'cflags_cc': [
              '-std=c++0x',
            ],
            'conditions': [

              [ 'target_arch=="ia32"', {
                'cflags': ['-m32'],
                'ldflags': ['-m32'],
              }],
              [ 'target_arch=="x64"', {
                'cflags': ['-m64'],
                'ldflags': ['-m64'],
              }],
            ],
          }],
          [ 'OS=="android" and mozilla_client==0', {
            'defines': [
              'NO_SYSINFO',
              'NO_FORK_CHECK',
              'ANDROID',
            ],
          }],
          [ 'OS=="mac"', {
            'defines': [
              'DARWIN',
              'HAVE_STRERROR',
              'HAVE_BSD_FLOCK',
              'XP_UNIX',
            ],
            'conditions': [
              [ 'target_arch=="ia32"', {
                'xcode_settings': {
                  'ARCHS': ['i386'],
                },
              }],
              [ 'target_arch=="x64"', {
                'xcode_settings': {
                  'ARCHS': ['x86_64'],
                },
              }],
            ],
          }],
          [ 'OS=="win"', {
            'defines': [
              '_WINDOWS',
              'WIN95',
              '_CRT_SECURE_NO_WARNINGS',
              '_CRT_NONSTDC_NO_WARNINGS',
            ],
            'cflags': [
              '-W3',
            ],
          }],
          [ 'disable_dbm==1', {
            'defines': [
              'NSS_DISABLE_DBM',
            ],
          }],
          [ 'disable_libpkix==1', {
            'defines': [
              'NSS_DISABLE_LIBPKIX',
            ],
          }],
        ],
      },
      # Common settings for x86 should go here.
      'x86_Base': {
        'abstract': 1,
        'msvs_settings': {
          'VCLinkerTool': {
            'MinimumRequiredVersion': '5.01',  # XP.
            'TargetMachine': '1',
          },
          'VCCLCompilerTool': {
            'PreprocessorDefinitions': [
              'WIN32',
            ],
          },
        },
        'msvs_configuration_platform': 'Win32',
      },
      # Common settings for x86-64 should go here.
      'x64_Base': {
        'abstract': 1,
        'msvs_configuration_platform': 'x64',
        'msvs_settings': {
          'VCLinkerTool': {
            'TargetMachine': '17', # x86-64
          },
          'VCCLCompilerTool': {
            'PreprocessorDefinitions': [
              'WIN64',
              '_AMD64_',
            ],
          },
        },
      },
      # Common settings for debug should go here.
      'Debug_Base': {
        'abstract': 1,
        #TODO: DEBUG_$USER
        'defines': ['DEBUG'],
        'xcode_settings': {
          'COPY_PHASE_STRIP': 'NO',
          'GCC_OPTIMIZATION_LEVEL': '0',
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',
            'BasicRuntimeChecks': '3',
            'RuntimeLibrary': '2', # /MD
          },
          'VCLinkerTool': {
            'LinkIncremental': '1',
          },
          'VCResourceCompilerTool': {
            'PreprocessorDefinitions': ['DEBUG'],
          },
        },
      },
      # Common settings for release should go here.n
      'Release_Base': {
        'abstract': 1,
        'defines': [
          'NDEBUG',
        ],
        'xcode_settings': {
          'DEAD_CODE_STRIPPING': 'YES',  # -Wl,-dead_strip
          'GCC_OPTIMIZATION_LEVEL': '2', # -O2
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '2', # /Os
            'RuntimeLibrary': '2', # /MD
          },
          'VCLinkerTool': {
            'LinkIncremental': '1',
          },
        },
      },
      #
      # Concrete configurations
      #
      # These configurations shouldn't have anything in them, it should
      # all be derived from the _Base configurations above.
      'Debug': {
        'inherit_from': ['Common_Base', 'x86_Base', 'Debug_Base'],
      },
      'Release': {
        'inherit_from': ['Common_Base', 'x86_Base', 'Release_Base'],
      },
      # The gyp ninja backend requires these.
      'Debug_x64': {
        'inherit_from': ['Common_Base', 'x64_Base', 'Debug_Base'],
      },
      'Release_x64': {
        'inherit_from': ['Common_Base', 'x64_Base', 'Release_Base'],
      },
    },
  },
  'conditions': [
    [ 'OS=="linux" or OS=="android"', {
      'variables': {
        'process_map_file': ['/bin/sh', '-c', '/bin/grep -v ";-" >(mapfile) | sed -e "s,;+,," -e "s; DATA ;;" -e "s,;;,," -e "s,;.*,;," > >@(_outputs)'],
      },
    }],
    [ 'OS=="mac"', {
      'variables': {
        'process_map_file': ['/bin/sh', '-c', '/usr/bin/grep -v ";+" >(mapfile) | grep -v ";-" | sed -e "s; DATA ;;" -e "s,;;,," -e "s,;.*,," -e "s,^,_," > >@(_outputs)'],
      },
    }],
  ],
}
