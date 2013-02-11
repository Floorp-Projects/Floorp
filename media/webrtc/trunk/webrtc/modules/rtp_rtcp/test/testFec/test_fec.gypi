# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [{   
     'target_name': 'test_fec',
      'type': 'executable',
      'dependencies': [
        'rtp_rtcp',
        '<(webrtc_root)/test/test.gyp:test_support_main',
      ],      
      'include_dirs': [
        '../../source',
        '../../../../system_wrappers/interface',
      ],
      'sources': [
        'test_fec.cc',
      ],         
    },
    {   
      'target_name': 'test_packet_masks_metrics',
      'type': 'executable',
      'dependencies': [
        'rtp_rtcp',
        '<(webrtc_root)/test/test.gyp:test_support_main',
        'rtp_rtcp',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],      
      'include_dirs': [
        '../../source',
        '<(webrtc_root)/system_wrappers/interface',
      ],
      'sources': [
        'test_packet_masks_metrics.cc',
        'average_residual_loss_xor_codes.h',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
