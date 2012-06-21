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
      'target_name': 'video_capture_module',
      'type': '<(library)',
      'dependencies': [
        'webrtc_utility',
        '<(webrtc_root)/common_video/common_video.gyp:webrtc_libyuv',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        '../../../interface',
        '<(webrtc_root)/common_video/libyuv/include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../../interface',
          '<(webrtc_root)/common_video/libyuv/include',
        ],
      },
      'sources': [
        # interfaces
        '../interface/video_capture.h',
        '../interface/video_capture_defines.h',
        '../interface/video_capture_factory.h',
        # headers
        'video_capture_config.h',
        'video_capture_delay.h',
        'video_capture_impl.h',
        'device_info_impl.h',

        # DEFINE PLATFORM INDEPENDENT SOURCE FILES
        'video_capture_factory.cc',
        'video_capture_impl.cc',
        'device_info_impl.cc',
      ],
      'conditions': [
        ['include_internal_video_capture==0', {
          'sources': [
            'External/device_info_external.cc',
            'External/video_capture_external.cc',
          ],
        },{  # include_internal_video_capture == 1
          'conditions': [
            # DEFINE PLATFORM SPECIFIC SOURCE FILES
            ['OS=="linux"', {
              'include_dirs': [
                'Linux',
              ],
              'sources': [
                'Linux/device_info_linux.h',
                'Linux/video_capture_linux.h',
                'Linux/device_info_linux.cc',
                'Linux/video_capture_linux.cc',
              ],
            }],  # linux
            ['OS=="mac"', {
              'sources': [
                'Mac/QTKit/video_capture_recursive_lock.h',
                'Mac/QTKit/video_capture_qtkit.h',
                'Mac/QTKit/video_capture_qtkit_info.h',
                'Mac/QTKit/video_capture_qtkit_info_objc.h',
                'Mac/QTKit/video_capture_qtkit_objc.h',
                'Mac/QTKit/video_capture_qtkit_utility.h',
                'Mac/video_capture_mac.mm',
                'Mac/QTKit/video_capture_qtkit.mm',
                'Mac/QTKit/video_capture_qtkit_objc.mm',
                'Mac/QTKit/video_capture_recursive_lock.mm',
                'Mac/QTKit/video_capture_qtkit_info.mm',
                'Mac/QTKit/video_capture_qtkit_info_objc.mm',
              ],
              'include_dirs': [
                'Mac',
              ],
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework QTKit',
                  ],
                },
              },
            }],  # mac
            ['OS=="win"', {
              'dependencies': [
                '<(webrtc_root)/modules/video_capture/main/source/Windows/direct_show_base_classes.gyp:direct_show_base_classes',
              ],
              'include_dirs': [
                'Windows',
              ],
              'sources': [
                'Windows/help_functions_windows.h',
                'Windows/sink_filter_windows.h',
                'Windows/video_capture_windows.h',
                'Windows/device_info_windows.h',
                'Windows/capture_delay_values_windows.h',
                'Windows/help_functions_windows.cc',
                'Windows/sink_filter_windows.cc',
                'Windows/video_capture_windows.cc',
                'Windows/device_info_windows.cc',
                'Windows/video_capture_factory_windows.cc',
              ],
              'msvs_settings': {
                'VCLibrarianTool': {
                  'AdditionalDependencies': 'Strmiids.lib',
                },
              },
            }],  # win
            ['OS=="android"', {
              'include_dirs': [
                'android',
              ],
              'sources': [
                'android/device_info_android.cc',
                'android/device_info_android.h',
                'android/video_capture_android.cc',
                'android/video_capture_android.h',
              ],
            }],  # android
          ], # conditions
        }],  # include_internal_video_capture
      ], # conditions
    },
  ],
   # Exclude the test targets when building with chromium.
  'conditions': [
    ['build_with_chromium==0', {
      'targets': [        
        {        
          'target_name': 'video_capture_module_test',
          'type': 'executable',
          'dependencies': [
           'video_capture_module',
           'webrtc_utility',
           '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
           '<(webrtc_root)/../testing/gtest.gyp:gtest',
           '<(webrtc_root)/../test/test.gyp:test_support_main',
          ],
          'include_dirs': [
            '../interface',
          ],
          'sources': [
            '../test/video_capture_unittest.cc',
          ],
          'conditions': [            
           # DEFINE PLATFORM SPECIFIC INCLUDE AND CFLAGS
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
                # TODO(andrew): CoreAudio and AudioToolbox shouldn't be needed.
                'OTHER_LDFLAGS': [
                  '-framework Foundation -framework AppKit -framework Cocoa -framework OpenGL -framework CoreVideo -framework CoreAudio -framework AudioToolbox',
                ],
              },
            }],
          ] # conditions
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
