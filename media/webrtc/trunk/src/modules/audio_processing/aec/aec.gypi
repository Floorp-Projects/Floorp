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
      'target_name': 'aec',
      'type': '<(library)',
      'variables': {
        # Outputs some low-level debug files.
        'aec_debug_dump%': 0,
      },
      'dependencies': [
        'apm_util',
        '<(webrtc_root)/common_audio/common_audio.gyp:signal_processing',
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
        'include/echo_cancellation.h',
        'echo_cancellation.c',
        'echo_cancellation_internal.h',
        'aec_core.h',
        'aec_core.c',
        'aec_rdft.h',
        'aec_rdft.c',
        'aec_resampler.h',
        'aec_resampler.c',
      ],
      'conditions': [
        ['target_arch=="ia32" or target_arch=="x64"', {
          'dependencies': [ 'aec_sse2', ],
        }],
        ['aec_debug_dump==1', {
          'defines': [ 'WEBRTC_AEC_DEBUG_DUMP', ],
        }],
      ],
    },
  ],
  'conditions': [
    ['target_arch=="ia32" or target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'aec_sse2',
          'type': '<(library)',
          'sources': [
            'aec_core_sse2.c',
            'aec_rdft_sse2.c',
          ],
          'conditions': [
            ['os_posix==1 and OS!="mac"', {
              'cflags': [ '-msse2', ],
	      'cflags_mozilla': [ '-msse2', ],
            }],
            ['OS=="mac"', {
              'xcode_settings': {
                'OTHER_CFLAGS': [ '-msse2', ],
              },
            }],
          ],
        },
      ],
    }],
  ],
}
