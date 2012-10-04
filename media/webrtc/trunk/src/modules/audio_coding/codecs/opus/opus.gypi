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
      'target_name': 'opus',
      'type': '<(library)',
      'include_dirs': [
        '../interface',
      ],
      'include_dirs': [
        '../../neteq',
        '../../../../common_audio/signal_processing/include',
        '../../../../common_audio/signal_processing',
        'interface',
      ],
      'conditions': [
        ['build_with_mozilla==1', {
          # Mozilla provides its own build of the opus library.
          'include_dirs': [
            '$(DIST)/include/opus',
           ]
        }, {
          'dependencies': [
            '<(webrtc_root)/../third_party/opus/opus.gyp:opus'
          ],
           'include_dirs': [
             '<(webrtc_root)/../third_party/opus/source/include',
            ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../neteq',
          '../../../../common_audio/signal_processing/include',
          '../../../../common_audio/signal_processing',
          'interface',
        ],
        'conditions': [
          ['build_with_mozilla==1', {
            'include_dirs': [
              '$(DIST)/include/opus',
             ]
          }, {
             'include_dirs': [
               '<(webrtc_root)/../third_party/opus/source/include',
              ],
          }],
        ],
      },
      'sources': [
        'interface/opus_interface.h',
        'opus_interface.c',
      ],
    },
  ],
}
