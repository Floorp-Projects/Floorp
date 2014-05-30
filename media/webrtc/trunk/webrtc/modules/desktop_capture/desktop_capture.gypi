# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'desktop_capture',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        "desktop_and_cursor_composer.cc",
        "desktop_and_cursor_composer.h",
        "desktop_capture_types.h",
        "desktop_capturer.h",
        "desktop_frame.cc",
        "desktop_frame.h",
        "desktop_frame_win.cc",
        "desktop_frame_win.h",
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
        "mac/desktop_configuration.h",
        "mac/desktop_configuration.mm",
        "mac/desktop_configuration_monitor.h",
        "mac/desktop_configuration_monitor.cc",
        "mac/osx_version.h",
        "mac/osx_version.cc",
        "mac/scoped_pixel_buffer_object.cc",
        "mac/scoped_pixel_buffer_object.h",
        "mouse_cursor.cc",
        "mouse_cursor.h",
        "mouse_cursor_monitor.h",
        "mouse_cursor_monitor_mac.mm",
        "mouse_cursor_monitor_win.cc",
        "mouse_cursor_monitor_x11.cc",
        "mouse_cursor_shape.h",
        "screen_capture_frame_queue.cc",
        "screen_capture_frame_queue.h",
        "screen_capturer.cc",
        "screen_capturer.h",
        "screen_capturer_helper.cc",
        "screen_capturer_helper.h",
        "screen_capturer_mac.mm",
        "screen_capturer_win.cc",
        "screen_capturer_x11.cc",
        "shared_desktop_frame.cc",
        "shared_desktop_frame.h",
        "shared_memory.cc",
        "shared_memory.h",
        "win/cursor.cc",
        "win/cursor.h",
        "win/desktop.cc",
        "win/desktop.h",
        "win/scoped_gdi_object.h",
        "win/scoped_thread_desktop.cc",
        "win/scoped_thread_desktop.h",
        "window_capturer.cc",
        "window_capturer.h",
        "window_capturer_mac.cc",
        "window_capturer_win.cc",
        "window_capturer_x11.cc",
        "x11/shared_x_display.h",
        "x11/shared_x_display.cc",
        "x11/x_error_trap.cc",
        "x11/x_error_trap.h",
        "x11/x_server_pixel_buffer.cc",
        "x11/x_server_pixel_buffer.h",
      ],
      'conditions': [
        ['OS!="ios" and (target_arch=="ia32" or target_arch=="x64")', {
          'dependencies': [
            'desktop_capture_differ_sse2',
          ],
        }],
        ['use_x11 == 1', {
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
            "mouse_cursor_monitor_null.cc",
            "screen_capturer_null.cc",
            "window_capturer_null.cc",
          ],
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
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
              'cflags': [
                '-msse2',
              ],
            }],
          ],
        },
      ],  # targets
    }],
  ],
}
