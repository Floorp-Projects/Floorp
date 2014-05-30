# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['../build/common.gypi'],
  'targets': [
    {
      'target_name': 'common_video',
      'type': 'static_library',
      'include_dirs': [
        '<(webrtc_root)/modules/interface/',
        'interface',
        'libyuv/include',
      ],
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'interface',
          'libyuv/include',
        ],
      },
      'conditions': [
        ['build_libyuv==1', {
          'dependencies': ['<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',],
        }, {
          # Need to add a directory normally exported by libyuv.gyp.
          'include_dirs': ['<(libyuv_dir)/include',],
        }],
      ],
      'sources': [
        'interface/i420_video_frame.h',
        'interface/native_handle.h',
        'interface/texture_video_frame.h',
        'i420_video_frame.cc',
        'libyuv/include/webrtc_libyuv.h',
        'libyuv/include/scaler.h',
        'libyuv/webrtc_libyuv.cc',
        'libyuv/scaler.cc',
        'plane.h',
        'plane.cc',
        'texture_video_frame.cc'
      ],
    },
  ],  # targets
}
