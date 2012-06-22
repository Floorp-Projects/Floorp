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
      'target_name': 'ns',
      'type': '<(library)',
      'dependencies': [
        '<(webrtc_root)/common_audio/common_audio.gyp:signal_processing',
        'apm_util'
      ],
      'include_dirs': [
        'include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'sources': [
        'include/noise_suppression.h',
        'noise_suppression.c',
        'windows_private.h',
        'defines.h',
        'ns_core.c',
        'ns_core.h',
      ],
    },
    {
      'target_name': 'ns_fix',
      'type': '<(library)',
      'dependencies': [
        '<(webrtc_root)/common_audio/common_audio.gyp:signal_processing',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        'include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'sources': [
        'include/noise_suppression_x.h',
        'noise_suppression_x.c',
        'nsx_defines.h',
        'nsx_core.c',
        'nsx_core.h',
      ],
    },
  ],
}
