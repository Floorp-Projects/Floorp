# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This file contains common settings for building WebRTC components.

{
  # Nesting is required in order to use variables for setting other variables.
  'variables': {
    'variables': {
      'variables': {
        'variables': {
          # This will already be set to zero by supplement.gypi
          'build_with_chromium%': 1,
        },
        'build_with_chromium%': '<(build_with_chromium)',

        'conditions': [
          ['build_with_chromium==1', {
            'build_with_libjingle': 1,
            'webrtc_root%': '<(DEPTH)/third_party/webrtc',
            'apk_tests_path%': '<(DEPTH)/third_party/webrtc/build/apk_tests_noop.gyp',
            'modules_java_gyp_path%': '<(DEPTH)/third_party/webrtc/modules/modules_java_chromium.gyp',
          }, {
            'build_with_libjingle%': 0,
            'webrtc_root%': '<(DEPTH)/webrtc',
            'apk_tests_path%': '<(DEPTH)/webrtc/build/apk_tests.gyp',
            'modules_java_gyp_path%': '<(DEPTH)/webrtc/modules/modules_java.gyp',
          }],
        ],
      },
      'build_with_chromium%': '<(build_with_chromium)',
      'build_with_libjingle%': '<(build_with_libjingle)',
      'webrtc_root%': '<(webrtc_root)',
      'apk_tests_path%': '<(apk_tests_path)',
      'modules_java_gyp_path%': '<(modules_java_gyp_path)',
      'webrtc_vp8_dir%': '<(webrtc_root)/modules/video_coding/codecs/vp8',
      'webrtc_vp9_dir%': '<(webrtc_root)/modules/video_coding/codecs/vp9',
      'webrtc_h264_dir%': '<(webrtc_root)/modules/video_coding/codecs/h264',
      'include_g711%': 1,
      'include_g722%': 1,
      'include_ilbc%': 1,
      'include_opus%': 1,
      'include_isac%': 1,
      'include_pcm16b%': 1,
      'opus_dir%': '<(DEPTH)/third_party/opus',
    },
    'build_with_chromium%': '<(build_with_chromium)',
    'build_with_libjingle%': '<(build_with_libjingle)',
    'webrtc_root%': '<(webrtc_root)',
    'apk_tests_path%': '<(apk_tests_path)',
    'modules_java_gyp_path%': '<(modules_java_gyp_path)',
    'webrtc_vp8_dir%': '<(webrtc_vp8_dir)',
    'webrtc_vp9_dir%': '<(webrtc_vp9_dir)',
    'webrtc_h264_dir%': '<(webrtc_h264_dir)',

    'include_g711%': '<(include_g711)',
    'include_g722%': '<(include_g722)',
    'include_ilbc%': '<(include_ilbc)',
    'include_opus%': '<(include_opus)',
    'include_isac%': '<(include_isac)',
    'include_pcm16b%': '<(include_pcm16b)',

    'rtc_relative_path%': 1,
    'external_libraries%': '0',
    'json_root%': '<(DEPTH)/third_party/jsoncpp/source/include/',
    # openssl needs to be defined or gyp will complain. Is is only used when
    # when providing external libraries so just use current directory as a
    # placeholder.
    'ssl_root%': '.',

    # The Chromium common.gypi we use treats all gyp files without
    # chromium_code==1 as third party code. This disables many of the
    # preferred warning settings.
    #
    # We can set this here to have WebRTC code treated as Chromium code. Our
    # third party code will still have the reduced warning settings.
    'chromium_code': 1,

    # Set to 1 to enable code coverage on Linux using the gcov library.
    'coverage%': 0,

    # Remote bitrate estimator logging/plotting.
    'enable_bwe_test_logging%': 0,

    # Selects fixed-point code where possible.
    'prefer_fixed_point%': 0,

    # Enable data logging. Produces text files with data logged within engines
    # which can be easily parsed for offline processing.
    'enable_data_logging%': 0,

    # Enables the use of protocol buffers for debug recordings.
    'enable_protobuf%': 1,

    # Disable these to not build components which can be externally provided.
    'build_json%': 1,
    'build_libjpeg%': 1,
    'build_libyuv%': 1,
    'build_libvpx%': 1,
    'build_vp9%': 1,
    'build_ssl%': 1,
    'build_openmax_dl%': 1,
    'build_opus%': 1,

    # Disable by default
    'have_dbus_glib%': 0,

    # Enable to use the Mozilla internal settings.
    'build_with_mozilla%': 0,

    # Make it possible to provide custom locations for some libraries.
    'libvpx_dir%': '<(DEPTH)/third_party/libvpx',
    'libyuv_dir%': '<(DEPTH)/third_party/libyuv',
    'opus_dir%': '<(opus_dir)',

    # Use Java based audio layer as default for Android.
    # Change this setting to 1 to use Open SL audio instead.
    # TODO(henrika): add support for Open SL ES.
    'enable_android_opensl%': 0,

    # Link-Time Optimizations
    # Executes code generation at link-time instead of compile-time
    # https://gcc.gnu.org/wiki/LinkTimeOptimization
    'use_lto%': 0,

    # Defer ssl perference to that specified through sslconfig.h instead of
    # choosing openssl or nss directly.  In practice, this can be used to
    # enable schannel on windows.
    'use_legacy_ssl_defaults%': 0,

    'conditions': [
      ['build_with_chromium==1', {
        # Exclude pulse audio on Chromium since its prerequisites don't require
        # pulse audio.
        'include_pulse_audio%': 0,

        # Exclude internal ADM since Chromium uses its own IO handling.
        'include_internal_audio_device%': 0,

        'include_ndk_cpu_features%': 0,
      }, {  # Settings for the standalone (not-in-Chromium) build.
        # TODO(andrew): For now, disable the Chrome plugins, which causes a
        # flood of chromium-style warnings. Investigate enabling them:
        # http://code.google.com/p/webrtc/issues/detail?id=163
        'clang_use_chrome_plugins%': 0,

        'include_pulse_audio%': 1,
        'include_internal_audio_device%': 1,
        'include_ndk_cpu_features%': 0,
      }],
      ['build_with_libjingle==1', {
        'include_tests%': 0,
        'restrict_webrtc_logging%': 1,
      }, {
        'include_tests%': 1,
        'restrict_webrtc_logging%': 0,
      }],
      ['OS=="linux"', {
        'include_alsa_audio%': 1,
      }, {
        'include_alsa_audio%': 0,
      }],
      ['OS=="openbsd"', {
        'include_sndio_audio%': 1,
      }, {
        'include_sndio_audio%': 0,
      }],
      ['OS=="solaris" or (OS!="openbsd" and os_bsd==1)', {
        'include_pulse_audio%': 1,
      }, {
        'include_pulse_audio%': 0,
      }],
      ['OS=="linux" or OS=="solaris" or os_bsd==1', {
        'include_v4l2_video_capture%': 1,
      }, {
        'include_v4l2_video_capture%': 0,
      }],
      ['OS=="ios"', {
        'build_libjpeg%': 0,
        'enable_protobuf%': 0,
      }],
      ['target_arch=="arm" or target_arch=="arm64"', {
        'prefer_fixed_point%': 1,
      }],
      ['OS!="ios" and (target_arch!="arm" or arm_version>=7) and target_arch!="mips64el" and build_with_mozilla==0', {
        'rtc_use_openmax_dl%': 1,
      }, {
        'rtc_use_openmax_dl%': 0,
      }],
    ], # conditions
  },
  'target_defaults': {
    'conditions': [
      ['moz_widget_toolkit_gonk==1', {
        'defines' : [
          'WEBRTC_GONK',
        ],
      }],
      ['restrict_webrtc_logging==1', {
        'defines': ['WEBRTC_RESTRICT_LOGGING',],
      }],
      ['build_with_mozilla==1', {
        'defines': [
          # Changes settings for Mozilla build.
          'WEBRTC_MOZILLA_BUILD',
         ],
      }],
      ['have_dbus_glib==1', {
        'defines': [
          'HAVE_DBUS_GLIB',
         ],
         'cflags': [
           '<!@(pkg-config --cflags dbus-glib-1)',
         ],
      }],
      ['rtc_relative_path==1', {
        'defines': ['EXPAT_RELATIVE_PATH',],
      }],
      ['os_posix==1', {
        'configurations': {
          'Debug_Base': {
            'defines': [
              # Chromium's build/common.gypi defines _DEBUG for all posix
              # _except_ for ios & mac.  The size of data types such as
              # pthread_mutex_t varies between release and debug builds
              # and is controlled via this flag.  Since we now share code
              # between base/base.gyp and build/common.gypi (this file),
              # both gyp(i) files, must consistently set this flag uniformly
              # or else we'll run in to hard-to-figure-out problems where
              # one compilation unit uses code from another but expects
              # differently laid out types.
              # For WebRTC, we want it there as well, because ASSERT and
              # friends trigger off of it.
              '_DEBUG',
            ],
          },
        },
      }],
      ['build_with_chromium==1', {
        'defines': [
          # Changes settings for Chromium build.
          'WEBRTC_CHROMIUM_BUILD',
          'LOGGING_INSIDE_WEBRTC',
        ],
        'include_dirs': [
          # Include the top-level directory when building in Chrome, so we can
          # use full paths (e.g. headers inside testing/ or third_party/).
          '<(DEPTH)',
          # The overrides must be included before the WebRTC root as that's the
          # mechanism for selecting the override headers in Chromium.
          '../overrides',
          # The WebRTC root is needed to allow includes in the WebRTC code base
          # to be prefixed with webrtc/.
          '../..',
        ],
      }, {
         # Include the top-level dir so the WebRTC code can use full paths.
        'include_dirs': [
          '../..',
        ],
        'conditions': [
          ['os_posix==1', {
            'conditions': [
              # -Wextra is currently disabled in Chromium's common.gypi. Enable
              # for targets that can handle it. For Android/arm64 right now
              # there will be an 'enumeral and non-enumeral type in conditional
              # expression' warning in android_tools/ndk_experimental's version
              # of stlport.
              # See: https://code.google.com/p/chromium/issues/detail?id=379699
              ['target_arch!="arm64" or OS!="android"', {
                'cflags': [
                  '-Wextra',
                  # We need to repeat some flags from Chromium's common.gypi
                  # here that get overridden by -Wextra.
                  '-Wno-unused-parameter',
                  '-Wno-missing-field-initializers',
                  '-Wno-strict-overflow',
                ],
              }],
            ],
            'cflags_cc': [
              '-Wnon-virtual-dtor',
              # This is enabled for clang; enable for gcc as well.
              '-Woverloaded-virtual',
            ],
          }],
          ['clang==1', {
            'cflags': [
              '-Wimplicit-fallthrough',
              '-Wthread-safety',
            ],
          }],
        ],
      }],
      ['target_arch=="arm64"', {
        'defines': [
          'WEBRTC_ARCH_ARM',
          # TODO(zhongwei) Defining an unique WEBRTC_NEON and
          # distinguishing ARMv7 NEON and ARM64 NEON by
          # WEBRTC_ARCH_ARM_V7 and WEBRTC_ARCH_ARM64 should be better.

          # This macro is used to distinguish ARMv7 NEON and ARM64 NEON
          'WEBRTC_ARCH_ARM64_NEON',
        ],
      }],
      ['target_arch=="arm"', {
        'defines': [
          'WEBRTC_ARCH_ARM',
        ],
        'conditions': [
          ['arm_version>=7', {
            'defines': ['WEBRTC_ARCH_ARM_V7',
                        'WEBRTC_BUILD_NEON_LIBS'],
            'conditions': [
              ['arm_neon==1', {
                'defines': ['WEBRTC_ARCH_ARM_NEON',],
              }],
              ['arm_neon==0 and (OS=="android" or moz_widget_toolkit_gonk==1)', {
                'defines': ['WEBRTC_DETECT_ARM_NEON',],
              }],
            ],
          }],
        ],
      }],
      ['os_bsd==1', {
        'defines': [
          'WEBRTC_BSD',
          'WEBRTC_THREAD_RR',
        ],
      }],
      ['OS=="dragonfly" or OS=="netbsd"', {
        'defines': [
          # doesn't support pthread_condattr_setclock
          'WEBRTC_CLOCK_TYPE_REALTIME',
        ],
      }],
      ['OS=="openbsd"', {
        'defines' : [
          'WEBRTC_AUDIO_SNDIO',
        ],
      }],
      # Mozilla: if we support Mozilla on MIPS, we'll need to mod the cflags entries here
      ['target_arch=="mipsel" and mips_arch_variant!="r6" and android_webview_build==0', {
        'defines': [
          'MIPS32_LE',
        ],
        'conditions': [
          ['mips_float_abi=="hard"', {
            'defines': [
              'MIPS_FPU_LE',
            ],
          }],
          ['mips_arch_variant=="r2"', {
            'defines': [
              'MIPS32_R2_LE',
            ],
          }],
          ['mips_dsp_rev==1', {
            'defines': [
              'MIPS_DSP_R1_LE',
            ],
          }],
          ['mips_dsp_rev==2', {
            'defines': [
              'MIPS_DSP_R1_LE',
              'MIPS_DSP_R2_LE',
            ],
          }],
        ],
      }],
      ['coverage==1 and OS=="linux"', {
        'cflags': [ '-ftest-coverage',
                    '-fprofile-arcs' ],
        'link_settings': { 'libraries': [ '-lgcov' ] },
      }],
      ['os_posix==1', {
        # For access to standard POSIXish features, use WEBRTC_POSIX instead of
        # a more specific macro.
        'defines': [
          'WEBRTC_POSIX',
        ],
      }],
      ['OS=="ios"', {
        'defines': [
          'WEBRTC_MAC',
          'WEBRTC_IOS',
        ],
      }],
      ['OS=="linux"', {
#        'conditions': [
#          ['have_clock_monotonic==1', {
#            'defines': [
#              'WEBRTC_CLOCK_TYPE_REALTIME',
#            ],
#          }],
#        ],
        'defines': [
          'WEBRTC_LINUX',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'WEBRTC_MAC',
        ],
      }],
      ['OS=="win"', {
        'defines': [
          'WEBRTC_WIN',
        ],
        # TODO(andrew): enable all warnings when possible.
        # TODO(phoglund): get rid of 4373 supression when
        # http://code.google.com/p/webrtc/issues/detail?id=261 is solved.
        'msvs_disabled_warnings': [
          4373,  # legacy warning for ignoring const / volatile in signatures.
          4389,  # Signed/unsigned mismatch.
        ],
        # Re-enable some warnings that Chromium disables.
        'msvs_disabled_warnings!': [4189,],
      }],
      # used on GONK as well
      ['enable_android_opensl==1 and (OS=="android" or moz_widget_toolkit_gonk==1)', {
        'defines': [
          'WEBRTC_ANDROID_OPENSLES',
        ],
      }],
      ['moz_webrtc_omx==1', {
        'defines' : [
          'MOZ_WEBRTC_OMX'
        ],
      }],
      ['OS=="android"', {
        'defines': [
          'WEBRTC_LINUX',
          'WEBRTC_ANDROID',
         ],
         'conditions': [
           ['clang!=1', {
             # The Android NDK doesn't provide optimized versions of these
             # functions. Ensure they are disabled for all compilers.
             'cflags': [
               '-fno-builtin-cos',
               '-fno-builtin-sin',
               '-fno-builtin-cosf',
               '-fno-builtin-sinf',
             ],
           }],
         ],
      }],
    ], # conditions
    'direct_dependent_settings': {
      'conditions': [
        ['build_with_mozilla==1', {
          'defines': [
            # Changes settings for Mozilla build.
            'WEBRTC_MOZILLA_BUILD',
           ],
        }],
        ['build_with_chromium==1', {
          'defines': [
            # Changes settings for Chromium build.
            'WEBRTC_CHROMIUM_BUILD',
          ],
          'include_dirs': [
            # overrides must be included first as that is the mechanism for
            # selecting the override headers in Chromium.
            '../overrides',
            '../..',
          ],
        }, {
          'include_dirs': [
            '../..',
          ],
        }],
        ['OS=="mac"', {
          'defines': [
            'WEBRTC_MAC',
          ],
        }],
        ['OS=="ios"', {
          'defines': [
            'WEBRTC_MAC',
            'WEBRTC_IOS',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            'WEBRTC_WIN',
          ],
        }],
        ['OS=="linux"', {
          'defines': [
            'WEBRTC_LINUX',
          ],
        }],
        ['OS=="android"', {
          'defines': [
            'WEBRTC_LINUX',
            'WEBRTC_ANDROID',
           ],
           'conditions': [
             ['enable_android_opensl==1', {
               'defines': [
                 'WEBRTC_ANDROID_OPENSLES',
               ],
             }]
           ],
        }],
        ['os_posix==1', {
          # For access to standard POSIXish features, use WEBRTC_POSIX instead
          # of a more specific macro.
          'defines': [
            'WEBRTC_POSIX',
          ],
        }],
      ],
    },
  }, # target_defaults
}

