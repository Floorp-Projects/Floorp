# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'remote_bitrate_estimator',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(rbe_components_path)/remote_bitrate_estimator_components.gyp:rbe_components',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'sources': [
        'include/bwe_defines.h',
        'include/remote_bitrate_estimator.h',
        'include/rtp_to_ntp.h',
        'bitrate_estimator.cc',
        'bitrate_estimator.h',
        'rtp_to_ntp.cc',
      ], # source
    },
  ], # targets
}
