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
      'target_name': 'video_render_module',
      'type': 'static_library',
      'dependencies': [
        'webrtc_utility',
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        'include',
        '../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
          '../interface',
        ],
      },
      'sources': [
        'android/video_render_android_impl.cc',
        'android/video_render_android_impl.h',
        'android/video_render_android_native_opengl2.cc',
        'android/video_render_android_native_opengl2.h',
        'android/video_render_android_surface_view.cc',
        'android/video_render_android_surface_view.h',
        'android/video_render_opengles20.cc',
        'android/video_render_opengles20.h',
        'external/video_render_external_impl.cc',
        'external/video_render_external_impl.h',
        'i_video_render.h',
        'include/video_render.h',
        'include/video_render_defines.h',
        'incoming_video_stream.cc',
        'incoming_video_stream.h',
        'linux/video_render_linux_impl.cc',
        'linux/video_render_linux_impl.h',
        'linux/video_x11_channel.cc',
        'linux/video_x11_channel.h',
        'linux/video_x11_render.cc',
        'linux/video_x11_render.h',
        'mac/cocoa_full_screen_window.mm',
        'mac/cocoa_full_screen_window.h',
        'mac/cocoa_render_view.mm',
        'mac/cocoa_render_view.h',
        'mac/video_render_agl.cc',
        'mac/video_render_agl.h',
        'mac/video_render_mac_carbon_impl.cc',
        'mac/video_render_mac_carbon_impl.h',
        'mac/video_render_mac_cocoa_impl.h',
        'mac/video_render_mac_cocoa_impl.mm',
        'mac/video_render_nsopengl.h',
        'mac/video_render_nsopengl.mm',
        'video_render_frames.cc',
        'video_render_frames.h',
        'video_render_impl.cc',
        'video_render_impl.h',
        'windows/i_video_render_win.h',
        'windows/video_render_direct3d9.cc',
        'windows/video_render_direct3d9.h',
        'windows/video_render_windows_impl.cc',
        'windows/video_render_windows_impl.h',
      ],
      # TODO(andrew): with the proper suffix, these files will be excluded
      # automatically.
      'conditions': [
        ['include_internal_video_render==1', {
          'defines': ['WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER',],
        }],
        ['OS!="android" or include_internal_video_render==0', {
          'sources!': [
            'android/video_render_android_impl.h',
            'android/video_render_android_native_opengl2.h',
            'android/video_render_android_surface_view.h',
            'android/video_render_opengles20.h',
            'android/video_render_android_impl.cc',
            'android/video_render_android_native_opengl2.cc',
            'android/video_render_android_surface_view.cc',
            'android/video_render_opengles20.cc',
          ],
        }],
        ['OS!="linux" or include_internal_video_render==0', {
          'sources!': [
            'linux/video_render_linux_impl.h',
            'linux/video_x11_channel.h',
            'linux/video_x11_render.h',
            'linux/video_render_linux_impl.cc',
            'linux/video_x11_channel.cc',
            'linux/video_x11_render.cc',
          ],
        }],
        ['OS!="mac" or include_internal_video_render==0', {
          'sources!': [
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
        ['OS=="mac"', {
          'direct_dependent_settings': {
            'include_dirs': ['mac',],
          },
        }],
        ['OS!="win" or include_internal_video_render==0', {
          'sources!': [
            'windows/i_video_render_win.h',
            'windows/video_render_direct3d9.h',
            'windows/video_render_windows_impl.h',
            'windows/video_render_direct3d9.cc',
            'windows/video_render_windows_impl.cc',
          ],
        }],
      ] # conditions
    }, # video_render_module
  ], # targets

  'conditions': [
    ['include_internal_video_render==1', {
      'defines': ['WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER',],
    }],
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'video_render_module_test',
          'type': 'executable',
          'dependencies': [
            'video_render_module',
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
              'libraries': [
                '-lrt',
                '-lXext',
                '-lX11',
              ],
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
    }], # include_tests==0
  ], # conditions
}

