# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      # Note this library is missing an implementation for the video capture.
      # Targets must link with either 'video_capture' or
      # 'video_capture_module_internal_impl' depending on whether they want to
      # use the internal capturer.
      'target_name': 'video_capture_module',
      'type': 'static_library',
      'dependencies': [
        'webrtc_utility',
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'device_info_impl.cc',
        'device_info_impl.h',
        'video_capture.h',
        'video_capture_config.h',
        'video_capture_defines.h',
        'video_capture_delay.h',
        'video_capture_factory.h',
        'video_capture_factory.cc',
        'video_capture_impl.cc',
        'video_capture_impl.h',
      ],
    },
    {
      # Default video capture module implementation that only supports external
      # capture.
      'target_name': 'video_capture',
      'type': 'static_library',
      'dependencies': [
        'video_capture_module',
      ],
      'sources': [
        'external/device_info_external.cc',
        'external/video_capture_external.cc',
      ],
    },
  ], # targets
  'conditions': [
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'video_capture_module_internal_impl',
          'type': 'static_library',
          'conditions': [
            ['OS!="android"', {
              'dependencies': [
                'video_capture_module',
                '<(webrtc_root)/common.gyp:webrtc_common',
              ],
            }],
            ['OS=="linux"', {
              'sources': [
                'linux/device_info_linux.cc',
                'linux/device_info_linux.h',
                'linux/video_capture_linux.cc',
                'linux/video_capture_linux.h',
              ],
            }],  # linux
            ['OS=="mac"', {
              'sources': [
                'mac/qtkit/video_capture_qtkit.h',
                'mac/qtkit/video_capture_qtkit.mm',
                'mac/qtkit/video_capture_qtkit_info.h',
                'mac/qtkit/video_capture_qtkit_info.mm',
                'mac/qtkit/video_capture_qtkit_info_objc.h',
                'mac/qtkit/video_capture_qtkit_info_objc.mm',
                'mac/qtkit/video_capture_qtkit_objc.h',
                'mac/qtkit/video_capture_qtkit_objc.mm',
                'mac/qtkit/video_capture_qtkit_utility.h',
                'mac/video_capture_mac.mm',
              ],
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework Cocoa',
                    '-framework CoreVideo',
                    '-framework QTKit',
                  ],
                },
              },
            }],  # mac
            ['OS=="win"', {
              'dependencies': [
                '<(DEPTH)/third_party/winsdk_samples/winsdk_samples.gyp:directshow_baseclasses',
              ],
              'sources': [
                'windows/device_info_ds.cc',
                'windows/device_info_ds.h',
                'windows/device_info_mf.cc',
                'windows/device_info_mf.h',
                'windows/help_functions_ds.cc',
                'windows/help_functions_ds.h',
                'windows/sink_filter_ds.cc',
                'windows/sink_filter_ds.h',
                'windows/video_capture_ds.cc',
                'windows/video_capture_ds.h',
                'windows/video_capture_factory_windows.cc',
                'windows/video_capture_mf.cc',
                'windows/video_capture_mf.h',
              ],
              'link_settings': {
                'libraries': [
                  '-lStrmiids.lib',
                ],
              },
            }],  # win
            ['OS=="win" and clang==1', {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'AdditionalOptions': [
                    # Disable warnings failing when compiling with Clang on Windows.
                    # https://bugs.chromium.org/p/webrtc/issues/detail?id=5366
                    '-Wno-comment',
                    '-Wno-ignored-attributes',
                    '-Wno-microsoft-extra-qualification',
                    '-Wno-missing-braces',
                    '-Wno-overloaded-virtual',
                    '-Wno-reorder',
                    '-Wno-writable-strings',
                  ],
                },
              },
            }],
            ['OS=="ios"', {
              'sources': [
                'ios/device_info_ios.h',
                'ios/device_info_ios.mm',
                'ios/device_info_ios_objc.h',
                'ios/device_info_ios_objc.mm',
                'ios/rtc_video_capture_ios_objc.h',
                'ios/rtc_video_capture_ios_objc.mm',
                'ios/video_capture_ios.h',
                'ios/video_capture_ios.mm',
              ],
              'xcode_settings': {
                'CLANG_ENABLE_OBJC_ARC': 'YES',
              },
              'all_dependent_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework AVFoundation',
                    '-framework CoreMedia',
                    '-framework CoreVideo',
                    '-framework UIKit',
                  ],
                },
              },
            }],  # ios
          ], # conditions
        },
      ],
    }], # build_with_chromium==0
    ['include_tests==1 and OS!="android"', {
      'targets': [
        {
          'target_name': 'video_capture_tests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'video_capture_module',
            'video_capture_module_internal_impl',
            'webrtc_utility',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'test/video_capture_unittest.cc',
            'test/video_capture_main_mac.mm',
          ],
          'conditions': [
            ['OS=="mac" or OS=="linux"', {
              'cflags': [
                '-Wno-write-strings',
              ],
              'ldflags': [
                '-lpthread -lm',
              ],
            }],
            ['OS=="linux"', {
              'libraries': [
                '-lrt',
                '-lXext',
                '-lX11',
              ],
            }],
            ['OS=="mac"', {
              'dependencies': [
                # Link with a special main for mac so we can use the webcam.
                '<(webrtc_root)/test/test.gyp:test_support_main_threaded_mac',
              ],
              'xcode_settings': {
                # TODO(andrew): CoreAudio and AudioToolbox shouldn't be needed.
                'OTHER_LDFLAGS': [
                  '-framework Foundation -framework AppKit -framework Cocoa -framework OpenGL -framework CoreVideo -framework CoreAudio -framework AudioToolbox',
                ],
              },
            }], # OS=="mac"
            ['OS!="mac"', {
              'dependencies': [
                # Otherwise, use the regular main.
                '<(webrtc_root)/test/test.gyp:test_support_main',
              ],
            }], # OS!="mac"
          ] # conditions
        },
      ], # targets
    }],
  ],
}

