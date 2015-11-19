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
      'target_name': 'PCM16B',
      'type': 'static_library',
      'dependencies': [
        'audio_encoder_interface',
        'G711',
      ],
      'include_dirs': [
        'include',
        '<(webrtc_root)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
          '<(webrtc_root)',
        ],
      },
      'sources': [
        'include/audio_encoder_pcm16b.h',
        'include/pcm16b.h',
        'audio_encoder_pcm16b.cc',
        'pcm16b.c',
      ],
    },
  ], # targets
}
