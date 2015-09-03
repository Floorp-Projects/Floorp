# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'multi_monitor_screenshare%' : 1,
  },
  'targets': [
    {
      'target_name': 'desktop_capture',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
      ],
      'sources': [
        "desktop_and_cursor_composer.cc",
        "desktop_and_cursor_composer.h",
        "desktop_capture_types.h",
        "desktop_capturer.h",
        "desktop_frame.cc",
        "desktop_frame.h",
        "desktop_geometry.cc",
        "desktop_geometry.h",
        "desktop_capture_options.h",
        "desktop_capture_options.cc",
        "desktop_capturer.h",
        "desktop_region.cc",
        "desktop_region.h",
        "differ.cc",
        "differ.h",
        "differ_block.cc",
        "differ_block.h",
#        "mac/full_screen_chrome_window_detector.cc",
#        "mac/full_screen_chrome_window_detector.h",
#        "mac/window_list_utils.cc",
#        "mac/window_list_utils.h",
        "mouse_cursor.cc",
        "mouse_cursor.h",
        "mouse_cursor_monitor.h",
        "screen_capture_frame_queue.cc",
        "screen_capture_frame_queue.h",
        "screen_capturer.cc",
        "screen_capturer.h",
        "screen_capturer_helper.cc",
        "screen_capturer_helper.h",
        "shared_desktop_frame.cc",
        "shared_desktop_frame.h",
        "shared_memory.cc",
        "shared_memory.h",
#        "win/screen_capturer_win_gdi.cc",
#        "win/screen_capturer_win_gdi.h",
#        "win/screen_capturer_win_magnifier.cc",
#        "win/screen_capturer_win_magnifier.h",
#        "win/screen_capture_utils.cc",
#        "win/screen_capture_utils.h",
#        "win/window_capture_utils.cc",
#        "win/window_capture_utils.h",
        "window_capturer.cc",
        "window_capturer.h",
        "desktop_device_info.h",
        "desktop_device_info.cc",
        "app_capturer.h",
        "app_capturer.cc",
      ],
      'conditions': [
        ['OS!="android"', {
          'sources': [
            '../../video_engine/desktop_capture_impl.cc',
            '../../video_engine/desktop_capture_impl.h',
          ],
        }],
        ['multi_monitor_screenshare != 0', {
          'defines': [
            'MULTI_MONITOR_SCREENSHARE'
          ],
        }],
        ['OS!="ios" and (target_arch=="ia32" or target_arch=="x64")', {
          'dependencies': [
            'desktop_capture_differ_sse2',
          ],
        }],
        ['use_x11 == 1', {
          'defines':[
            'USE_X11',
          ],
          'sources': [
            "mouse_cursor_monitor_x11.cc",
            "screen_capturer_x11.cc",
            "window_capturer_x11.cc",
            "x11/shared_x_util.h",
            "x11/shared_x_util.cc",
            "x11/shared_x_display.h",
            "x11/shared_x_display.cc",
            "x11/x_error_trap.cc",
            "x11/x_error_trap.h",
            "x11/x_server_pixel_buffer.cc",
            "x11/x_server_pixel_buffer.h",
            "x11/desktop_device_info_x11.h",
            "x11/desktop_device_info_x11.cc",
            "app_capturer_x11.cc",
          ],
          'link_settings': {
            'libraries': [
              '-lX11',
              '-lXcomposite',
              '-lXdamage',
              '-lXext',
              '-lXfixes',
              '-lXrender',
            ],
          },
        }],
        ['OS!="win" and OS!="mac" and use_x11==0', {
          'sources': [
            "app_capturer_null.cc",
            "desktop_device_info_null.cc",
            "mouse_cursor_monitor_null.cc",
            "screen_capturer_null.cc",
            "window_capturer_null.cc",
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            "mac/desktop_configuration.h",
            "mac/desktop_configuration.mm",
            "mac/desktop_configuration_monitor.h",
            "mac/desktop_configuration_monitor.cc",
            "mac/full_screen_chrome_window_detector.cc",
            "mac/full_screen_chrome_window_detector.h",
            "mac/window_list_utils.cc",
            "mac/window_list_utils.h",
            "mac/scoped_pixel_buffer_object.cc",
            "mac/scoped_pixel_buffer_object.h",
            "mac/desktop_device_info_mac.h",
            "mac/desktop_device_info_mac.mm",
            "mouse_cursor_monitor_mac.mm",
            "screen_capturer_mac.mm",
            "window_capturer_mac.mm",
            "app_capturer_mac.mm",
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
        }],
        ['OS=="win"', {
           'sources': [
             "desktop_frame_win.cc",
             "desktop_frame_win.h",
             "mouse_cursor_monitor_win.cc",
             "screen_capturer_win.cc",
             "win/cursor.cc",
             "win/cursor.h",
             "win/desktop.cc",
             "win/desktop.h",
             "win/scoped_gdi_object.h",
             "win/scoped_thread_desktop.cc",
             "win/scoped_thread_desktop.h",
             "win/win_shared.h",
             "win/win_shared.cc",
             "win/desktop_device_info_win.h",
             "win/desktop_device_info_win.cc",
             "win/screen_capturer_win_gdi.cc",
             "win/screen_capturer_win_gdi.h",
             "win/screen_capturer_win_magnifier.cc",
             "win/screen_capturer_win_magnifier.h",
             "win/screen_capture_utils.cc",
             "win/screen_capture_utils.h",
             "win/window_capture_utils.cc",
             "win/window_capture_utils.h",
             "window_capturer_win.cc",
             "app_capturer_win.cc",
           ],
          }],
      ],
    },
  ],  # targets
  'conditions': [
    ['OS!="ios" and (target_arch=="ia32" or target_arch=="x64")', {
      'targets': [
        {
          # Have to be compiled as a separate target because it needs to be
          # compiled with SSE2 enabled.
          'target_name': 'desktop_capture_differ_sse2',
          'type': 'static_library',
          'sources': [
            "differ_block_sse2.cc",
            "differ_block_sse2.h",
          ],
          'conditions': [
            [ 'os_posix == 1 and OS != "mac"', {
              'cflags': [ '-msse2', ],
              'cflags_mozilla': [ '-msse2', ],
            }],
          ],
        },
      ],  # targets
    }],
  ],
}
