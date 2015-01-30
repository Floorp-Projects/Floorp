# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      # Note this library is missing an implementation for the video render.
      # For that targets must link with 'video_render_module_impl' or
      # 'video_render_module_internal_impl' if they want to compile and use
      # the internal render as the default renderer.
      'target_name': 'video_render_module',
      'type': 'static_library',
      'dependencies': [
        'webrtc_utility',
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'external/video_render_external_impl.cc',
        'external/video_render_external_impl.h',
        'i_video_render.h',
        'include/video_render.h',
        'include/video_render_defines.h',
        'incoming_video_stream.cc',
        'incoming_video_stream.h',
        'video_render_frames.cc',
        'video_render_frames.h',
        'video_render_impl.h',
      ],
    },
    {
      # Default video_render_module implementation that only supports external
      # renders.
      'target_name': 'video_render_module_impl',
      'type': 'static_library',
      'dependencies': [
        'video_render_module',
      ],
      'sources': [
        'video_render_impl.cc',
      ],
    },
    {
      # video_render_module implementation that supports the internal
      # video_render implementation.
      'target_name': 'video_render_module_internal_impl',
      'type': 'static_library',
      'dependencies': [
        'video_render_module',
      ],
      'sources': [
        'video_render_internal_impl.cc',
      ],
       # TODO(andrew): with the proper suffix, these files will be excluded
       # automatically.
      'conditions': [
        ['OS=="android"', {
          'sources': [
            'android/video_render_android_impl.h',
            'android/video_render_android_native_opengl2.h',
            'android/video_render_android_surface_view.h',
            'android/video_render_opengles20.h',
            'android/video_render_android_impl.cc',
            'android/video_render_android_native_opengl2.cc',
            'android/video_render_android_surface_view.cc',
            'android/video_render_opengles20.cc',
          ],
          'link_settings': {
            'libraries': [
              '-lGLESv2',
            ],
          },
        }],
        ['OS=="ios"', {
          'sources': [
            # iOS
            'ios/open_gles20.h',
            'ios/open_gles20.mm',
            'ios/video_render_ios_channel.h',
            'ios/video_render_ios_channel.mm',
            'ios/video_render_ios_gles20.h',
            'ios/video_render_ios_gles20.mm',
            'ios/video_render_ios_impl.h',
            'ios/video_render_ios_impl.mm',
            'ios/video_render_ios_view.h',
            'ios/video_render_ios_view.mm',
          ],
          'xcode_settings': {
            'CLANG_ENABLE_OBJC_ARC': 'YES',
          },
          'all_dependent_settings': {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-framework OpenGLES',
                '-framework QuartzCore',
                '-framework UIKit',
              ],
            },
          },
        }],
        ['OS=="linux"', {
          'sources': [
            'linux/video_render_linux_impl.h',
            'linux/video_x11_channel.h',
            'linux/video_x11_render.h',
            'linux/video_render_linux_impl.cc',
            'linux/video_x11_channel.cc',
            'linux/video_x11_render.cc',
          ],
          'link_settings': {
            'libraries': [
              '-lXext',
            ],
          },
        }],
        ['OS=="mac"', {
          'sources': [
            'mac/cocoa_full_screen_window.h',
            'mac/cocoa_render_view.h',
            'mac/video_render_agl.h',
            'mac/video_render_mac_carbon_impl.h',
            'mac/video_render_mac_cocoa_impl.h',
            'mac/video_render_nsopengl.h',
            'mac/video_render_nsopengl.mm',
            'mac/video_render_mac_cocoa_impl.mm',
            'mac/video_render_agl.cc',
            'mac/video_render_mac_carbon_impl.cc',
            'mac/cocoa_render_view.mm',
            'mac/cocoa_full_screen_window.mm',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'windows/i_video_render_win.h',
            'windows/video_render_direct3d9.h',
            'windows/video_render_windows_impl.h',
            'windows/video_render_direct3d9.cc',
            'windows/video_render_windows_impl.cc',
          ],
          'include_dirs': [
            '<(directx_sdk_path)/Include',
          ],
        }],
      ] # conditions
    },
  ], # targets

  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'video_render_tests',
          'type': 'executable',
          'dependencies': [
            'video_render_module_internal_impl',
            'webrtc_utility',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/common_video/common_video.gyp:common_video',
          ],
          'sources': [
            'test/testAPI/testAPI.cc',
            'test/testAPI/testAPI.h',
            'test/testAPI/testAPI_android.cc',
            'test/testAPI/testAPI_mac.mm',
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
              'link_settings': {
                'libraries': [
                  '-lX11',
                ],
              },
            }],
            ['OS=="mac"', {
              'xcode_settings': {
                'OTHER_LDFLAGS': [
                  '-framework Foundation -framework AppKit -framework Cocoa -framework OpenGL',
                ],
              },
            }],
          ] # conditions
        }, # video_render_module_test
      ], # targets
      'conditions': [
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'video_render_tests_run',
              'type': 'none',
              'dependencies': [
                'video_render_tests',
              ],
              'includes': [
                '../../build/isolate.gypi',
              ],
              'sources': [
                'video_render_tests.isolate',
              ],
            },
          ],
        }],
      ],
    }], # include_tests==0
  ], # conditions
}

