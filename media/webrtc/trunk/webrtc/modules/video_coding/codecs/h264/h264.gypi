# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'webrtc_h264',
      'type': 'static_library',
      'conditions': [
        ['OS=="ios"', {
          'dependencies': [
            'webrtc_h264_video_toolbox',
          ],
          'sources': [
            'h264_objc.mm',
          ],
        }],
      ],
      'sources': [
        'h264.cc',
        'include/h264.h',
      ],
    }, # webrtc_h264
  ],
  'conditions': [
    ['OS=="ios"', {
      'targets': [
        {
          'target_name': 'webrtc_h264_video_toolbox',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',
          ],
          'link_settings': {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-framework CoreMedia',
                '-framework CoreVideo',
                '-framework VideoToolbox',
              ],
            },
          },
          'sources': [
            'h264_video_toolbox_decoder.cc',
            'h264_video_toolbox_decoder.h',
            'h264_video_toolbox_encoder.cc',
            'h264_video_toolbox_encoder.h',
            'h264_video_toolbox_nalu.cc',
            'h264_video_toolbox_nalu.h',
          ],
        }, # webrtc_h264_video_toolbox
      ], # targets
    }], # OS=="ios"
  ], # conditions
}
