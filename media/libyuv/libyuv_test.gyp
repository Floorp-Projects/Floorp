# Copyright 2011 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'libyuv_disable_jpeg%': 0,
  },
  'targets': [
    {
      'target_name': 'libyuv_unittest',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'libyuv.gyp:libyuv',
        'testing/gtest.gyp:gtest',
        'third_party/gflags/gflags.gyp:gflags',
      ],
      'direct_dependent_settings': {
        'defines': [
          'GTEST_RELATIVE_PATH',
        ],
      },
      'export_dependent_settings': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        # headers
        'unit_test/unit_test.h',

        # sources
        'unit_test/basictypes_test.cc',
        'unit_test/compare_test.cc',
        'unit_test/color_test.cc',
        'unit_test/convert_test.cc',
        'unit_test/cpu_test.cc',
        'unit_test/math_test.cc',
        'unit_test/planar_test.cc',
        'unit_test/rotate_argb_test.cc',
        'unit_test/rotate_test.cc',
        'unit_test/scale_argb_test.cc',
        'unit_test/scale_test.cc',
        'unit_test/unit_test.cc',
        'unit_test/video_common_test.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'cflags': [
            '-fexceptions',
          ],
        }],
        [ 'OS == "ios" and target_subarch == 64', {
          'defines': [
            'LIBYUV_DISABLE_NEON'
          ],
        }],
        [ 'OS == "ios"', {
          'xcode_settings': {
            'DEBUGGING_SYMBOLS': 'YES',
            'DEBUG_INFORMATION_FORMAT' : 'dwarf-with-dsym',
            # Work around compile issue with isosim.mm, see
            # https://code.google.com/p/libyuv/issues/detail?id=548 for details.
            'WARNING_CFLAGS': [
              '-Wno-sometimes-uninitialized',
            ],
          },
          'cflags': [
            '-Wno-sometimes-uninitialized',
          ],
        }],
        [ 'OS != "ios" and libyuv_disable_jpeg != 1', {
          'defines': [
            'HAVE_JPEG',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        # TODO(YangZhang): These lines can be removed when high accuracy
        # YUV to RGB to Neon is ported.
        [ '(target_arch == "armv7" or target_arch == "armv7s" \
          or (target_arch == "arm" and arm_version >= 7) \
          or target_arch == "arm64") \
          and (arm_neon == 1 or arm_neon_optional == 1)', {
          'defines': [
            'LIBYUV_NEON'
          ],
        }],
      ], # conditions
      'defines': [
        # Enable the following 3 macros to turn off assembly for specified CPU.
        # 'LIBYUV_DISABLE_X86',
        # 'LIBYUV_DISABLE_NEON',
        # 'LIBYUV_DISABLE_MIPS',
        # Enable the following macro to build libyuv as a shared library (dll).
        # 'LIBYUV_USING_SHARED_LIBRARY',
      ],
    },
    {
      'target_name': 'compare',
      'type': 'executable',
      'dependencies': [
        'libyuv.gyp:libyuv',
      ],
      'sources': [
        # sources
        'util/compare.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'cflags': [
            '-fexceptions',
          ],
        }],
      ], # conditions
    },
    {
      'target_name': 'convert',
      'type': 'executable',
      'dependencies': [
        'libyuv.gyp:libyuv',
      ],
      'sources': [
        # sources
        'util/convert.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'cflags': [
            '-fexceptions',
          ],
        }],
      ], # conditions
    },
    # TODO(fbarchard): Enable SSE2 and OpenMP for better performance.
    {
      'target_name': 'psnr',
      'type': 'executable',
      'sources': [
        # sources
        'util/psnr_main.cc',
        'util/psnr.cc',
        'util/ssim.cc',
      ],
      'dependencies': [
        'libyuv.gyp:libyuv',
      ],
      'conditions': [
        [ 'OS == "ios" and target_subarch == 64', {
          'defines': [
            'LIBYUV_DISABLE_NEON'
          ],
        }],

        [ 'OS != "ios" and libyuv_disable_jpeg != 1', {
          'defines': [
            'HAVE_JPEG',
          ],
        }],
      ], # conditions
    },

    {
      'target_name': 'cpuid',
      'type': 'executable',
      'sources': [
        # sources
        'util/cpuid.c',
      ],
      'dependencies': [
        'libyuv.gyp:libyuv',
      ],
    },
  ], # targets
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          # TODO(kjellander): Figure out what to change in build/apk_test.gypi
          # to it can be used instead of the copied code below. Using it in its
          # current version was not possible, since the target starts with 'lib',
          # which somewhere confuses the variables.
          'target_name': 'libyuv_unittest_apk',
          'type': 'none',
          'variables': {
            # These are used to configure java_apk.gypi included below.
            'test_type': 'gtest',
            'apk_name': 'libyuv_unittest',
            'test_suite_name': 'libyuv_unittest',
            'intermediate_dir': '<(PRODUCT_DIR)/libyuv_unittest_apk',
            'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)libyuv_unittest<(SHARED_LIB_SUFFIX)',
            'final_apk_path': '<(intermediate_dir)/libyuv_unittest-debug.apk',
            'java_in_dir': '<(DEPTH)/testing/android/native_test/java',
            'test_runner_path': '<(DEPTH)/util/android/test_runner.py',
            'native_lib_target': 'libyuv_unittest',
            'gyp_managed_install': 0,
          },
          'includes': [
            'build/android/test_runner.gypi',
            'build/java_apk.gypi',
           ],
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base_java',
            # TODO(kjellander): Figure out why base_build_config_gen is needed
            # here. It really shouldn't since it's a dependency of base_java
            # above, but there's always 0 tests run if it's missing.
            '<(DEPTH)/base/base.gyp:base_build_config_gen',
            '<(DEPTH)/build/android/pylib/device/commands/commands.gyp:chromium_commands',
            '<(DEPTH)/build/android/pylib/remote/device/dummy/dummy.gyp:remote_device_dummy_apk',
            '<(DEPTH)/testing/android/appurify_support.gyp:appurify_support_java',
            '<(DEPTH)/testing/android/on_device_instrumentation.gyp:reporter_java',
            '<(DEPTH)/tools/android/android_tools.gyp:android_tools',
            'libyuv_unittest',
          ],
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
