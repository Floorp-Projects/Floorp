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
          # This will be set to zero in the supplement.gypi triggered by a
          # gclient hook in the standalone build.
          'build_with_chromium%': 1,
          'build_with_libjingle%': 0,
        },
        'build_with_chromium%': '<(build_with_chromium)',
        'build_with_libjingle%': '<(build_with_libjingle)',

        'conditions': [
          ['build_with_chromium==1 or build_with_libjingle==1', {
            'webrtc_root%': '<(DEPTH)/third_party/webrtc',
          }, {
            'webrtc_root%': '<(DEPTH)/webrtc',
          }],
        ],
      },
      'build_with_chromium%': '<(build_with_chromium)',
      'build_with_libjingle%': '<(build_with_libjingle)',
      'webrtc_root%': '<(webrtc_root)',

      'webrtc_vp8_dir%': '<(webrtc_root)/modules/video_coding/codecs/vp8',
      'include_opus%': 1,
    },
    'build_with_chromium%': '<(build_with_chromium)',
    'build_with_libjingle%': '<(build_with_libjingle)',
    'webrtc_root%': '<(webrtc_root)',
    'webrtc_vp8_dir%': '<(webrtc_vp8_dir)',
    'include_opus%': '<(include_opus)',

    # The Chromium common.gypi we use treats all gyp files without
    # chromium_code==1 as third party code. This disables many of the
    # preferred warning settings.
    #
    # We can set this here to have WebRTC code treated as Chromium code. Our
    # third party code will still have the reduced warning settings.
    'chromium_code': 1,

    # Adds video support to dependencies shared by voice and video engine.
    # This should normally be enabled; the intended use is to disable only
    # when building voice engine exclusively.
    'enable_video%': 1,

    # Selects fixed-point code where possible.
    'prefer_fixed_point%': 0,

    # Enable data logging. Produces text files with data logged within engines
    # which can be easily parsed for offline processing.
    'enable_data_logging%': 0,

    # Disable these to not build components which can be externally provided.
    'build_libjpeg%': 1,
    'build_libyuv%': 1,
    'build_libvpx%': 1,

    # Enable to use the Mozilla internal settings.
    'build_with_mozilla%': 0,

    'libyuv_dir%': '<(DEPTH)/third_party/libyuv',

    # Define MIPS architecture variant, MIPS DSP variant and MIPS FPU
    # This may be subject to change in accordance to Chromium's MIPS flags
    'mips_arch_variant%': 'mips32r1',
    'mips_dsp_rev%': 0,
    'mips_fpu%' : 1,

    'conditions': [
      ['build_with_chromium==1', {
        # Exclude pulse audio on Chromium since its prerequisites don't require
        # pulse audio.
        'include_pulse_audio%': 0,

        # Exclude internal ADM since Chromium uses its own IO handling.
        'include_internal_audio_device%': 0,

        # Exclude internal VCM in Chromium build.
        'include_internal_video_capture%': 0,

        # Exclude internal video render module in Chromium build.
        'include_internal_video_render%': 0,

        'include_tests%': 0,

        # Disable the use of protocol buffers in production code.
        'enable_protobuf%': 0,

        'enable_tracing%': 0,

        'enable_android_opensl%': 0,
      }, {  # Settings for the standalone (not-in-Chromium) build.
        'include_pulse_audio%': 1,
        'include_internal_audio_device%': 1,
        'include_internal_video_capture%': 1,
        'include_internal_video_render%': 1,
        'enable_protobuf%': 1,
        'enable_tracing%': 1,
        'include_tests%': 1,

        # TODO(andrew): For now, disable the Chrome plugins, which causes a
        # flood of chromium-style warnings. Investigate enabling them:
        # http://code.google.com/p/webrtc/issues/detail?id=163
        'clang_use_chrome_plugins%': 0,

        # Switch between Android audio device OpenSL ES implementation
        # and Java Implementation
        'enable_android_opensl%': 0,
      }],
      ['OS=="ios"', {
        'enable_video%': 0,
        'enable_protobuf%': 0,
        'build_libjpeg%': 0,
        'build_libyuv%': 0,
        'build_libvpx%': 0,
        'include_tests%': 0,
      }],
      ['build_with_libjingle==1', {
        'include_tests%': 0,
      }],
      ['target_arch=="arm"', {
        'prefer_fixed_point%': 1,
      }],
    ], # conditions
  },
  'target_defaults': {
    'include_dirs': [
      # TODO(andrew): Remove '..' when we've added webrtc/ to include paths.
      '..',
      # Allow includes to be prefixed with webrtc/ in case it is not an
      # immediate subdirectory of <(DEPTH).
      '../..',
      # To include the top-level directory when building in Chrome, so we can
      # use full paths (e.g. headers inside testing/ or third_party/).
      '<(DEPTH)',
    ],
    'defines': [
      # TODO(leozwang): Run this as a gclient hook rather than at build-time:
      # http://code.google.com/p/webrtc/issues/detail?id=687
      'WEBRTC_SVNREVISION="Unavailable(issue687)"',
      #'WEBRTC_SVNREVISION="<!(python <(webrtc_root)/build/version.py)"',
    ],
    'conditions': [
      ['enable_tracing==1', {
        'defines': ['WEBRTC_LOGGING',],
      }],
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
      }, {
        'conditions': [
          ['os_posix==1', {
            'cflags': [
              '-Wextra',
              # We need to repeat some flags from Chromium's common.gypi here
              # that get overridden by -Wextra.
              '-Wno-unused-parameter',
              '-Wno-missing-field-initializers',
            ],
            'cflags_cc': [
              # This is enabled for clang; enable for gcc as well.
              '-Woverloaded-virtual',
            ],
          }],
        ],
      }],
      ['target_arch=="arm"', {
        'defines': [
          'WEBRTC_ARCH_ARM',
        ],
        'conditions': [
          ['armv7==1', {
            'defines': ['WEBRTC_ARCH_ARM_V7',],
            'conditions': [
              ['arm_neon==1', {
                'defines': ['WEBRTC_ARCH_ARM_NEON',],
              }, {
                'defines': ['WEBRTC_DETECT_ARM_NEON',],
              }],
            ],
          }],
        ],
      }],
      ['target_arch=="mipsel"', {
        'defines': [
          'MIPS32_LE',
        ],
        'conditions': [
          ['mips_fpu==1', {
            'defines': [
              'MIPS_FPU_LE',
            ],
            'cflags': [
              '-mhard-float',
            ],
          }, {
            'cflags': [
              '-msoft-float',
            ],
          }],
          ['mips_arch_variant=="mips32r2"', {
            'defines': [
              'MIPS32_R2_LE',
            ],
            'cflags': [
              '-mips32r2',
            ],
            'cflags_cc': [
              '-mips32r2',
            ],
          }],
          ['mips_dsp_rev==1', {
            'defines': [
              'MIPS_DSP_R1_LE',
            ],
            'cflags': [
              '-mdsp',
            ],
            'cflags_cc': [
              '-mdsp',
            ],
          }],
          ['mips_dsp_rev==2', {
            'defines': [
              'MIPS_DSP_R1_LE',
              'MIPS_DSP_R2_LE',
            ],
            'cflags': [
              '-mdspr2',
            ],
            'cflags_cc': [
              '-mdspr2',
            ],
          }],
        ],
      }],
      ['OS=="ios"', {
        'defines': [
          'WEBRTC_MAC',
          'WEBRTC_IOS',
          'WEBRTC_THREAD_RR',
          'WEBRTC_CLOCK_TYPE_REALTIME',
        ],
      }],
      ['OS=="linux"', {
        'defines': [
          'WEBRTC_LINUX',
          'WEBRTC_THREAD_RR',
          # TODO(andrew): can we select this automatically?
          # Define this if the Linux system does not support CLOCK_MONOTONIC.
          #'WEBRTC_CLOCK_TYPE_REALTIME',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'WEBRTC_MAC',
          'WEBRTC_THREAD_RR',
          'WEBRTC_CLOCK_TYPE_REALTIME',
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
      ['OS=="android"', {
        'defines': [
          'WEBRTC_LINUX',
          'WEBRTC_ANDROID',
          # TODO(leozwang): Investigate CLOCK_REALTIME and CLOCK_MONOTONIC
          # support on Android. Keep WEBRTC_CLOCK_TYPE_REALTIME for now,
          # remove it after I verify that CLOCK_MONOTONIC is fully functional
          # with condition and event functions in system_wrappers.
          'WEBRTC_CLOCK_TYPE_REALTIME',
          'WEBRTC_THREAD_RR',
         ],
         'conditions': [
           ['enable_android_opensl==1', {
             'defines': [
               'WEBRTC_ANDROID_OPENSLES',
             ],
           }],
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
  }, # target_defaults
}

