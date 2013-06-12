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
      'direct_dependent_settings': {
        # Headers may use include path relative to webrtc root and depend on
        # WEBRTC_WIN define, so we need to make sure dependent targets have
        # these settings.
        #
        # TODO(sergeyu): Move these settings to common.gypi
        'include_dirs': [
          '../../..',
        ],
        'conditions': [
          ['OS=="win"', {
            'defines': [
              'WEBRTC_WIN',
            ],
          }],
        ],
      },
      'sources': [
        "desktop_capturer.h",
        "desktop_frame.cc",
        "desktop_frame.h",
        "desktop_frame_win.cc",
        "desktop_frame_win.h",
        "desktop_geometry.cc",
        "desktop_geometry.h",
        "desktop_region.cc",
        "desktop_region.h",
        "shared_memory.cc",
        "shared_memory.h",
      ],
      'conditions': [
        ['OS!="win"', {
          'sources/': [
            ['exclude', '_win(_unittest)?\\.(cc|h)$'],
          ],
        }],
      ],
    },
  ], # targets
}
