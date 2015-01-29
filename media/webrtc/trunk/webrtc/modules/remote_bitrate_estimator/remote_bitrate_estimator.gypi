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
      'sources': [
        'include/bwe_defines.h',
        'include/remote_bitrate_estimator.h',
        'rate_statistics.cc',
        'rate_statistics.h',
      ], # source
    },
    {
      'target_name': 'bwe_tools_util',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        'rtp_rtcp',
      ],
      'sources': [
        'tools/bwe_rtp.cc',
        'tools/bwe_rtp.h',
      ],
    },
    {
      'target_name': 'bwe_rtp_to_text',
      'type': 'executable',
      'includes': [
        '../rtp_rtcp/source/rtp_rtcp.gypi',
      ],
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers_default',
        'bwe_tools_util',
        'rtp_rtcp',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'sources': [
        'tools/rtp_to_text.cc',
        '<(webrtc_root)/test/rtp_file_reader.cc',
        '<(webrtc_root)/test/rtp_file_reader.h',
      ], # source
    },
    {
      'target_name': 'bwe_rtp_play',
      'type': 'executable',
      'includes': [
        '../rtp_rtcp/source/rtp_rtcp.gypi',
      ],
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers_default',
        'bwe_tools_util',
        'rtp_rtcp',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'sources': [
        'tools/bwe_rtp_play.cc',
        '<(webrtc_root)/test/rtp_file_reader.cc',
        '<(webrtc_root)/test/rtp_file_reader.h',
      ], # source
    },
  ], # targets
}
