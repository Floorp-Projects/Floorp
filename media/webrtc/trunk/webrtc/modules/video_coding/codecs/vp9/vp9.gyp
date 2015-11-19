# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
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
      'target_name': 'webrtc_vp9',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/modules/video_coding/utility/video_coding_utility.gyp:video_coding_utility',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'conditions': [
        ['build_libvpx==1', {
          'dependencies': [
            '<(libvpx_dir)/libvpx.gyp:libvpx',
          ],
        }],
        ['build_vp9==1', {
          'sources': [
            'include/vp9.h',
            'vp9_impl.cc',
            'vp9_impl.h',
          ],
        }, {
          'sources': [
            'vp9_dummy_impl.cc',
          ],
        }],
      ],
    },
  ],
}
