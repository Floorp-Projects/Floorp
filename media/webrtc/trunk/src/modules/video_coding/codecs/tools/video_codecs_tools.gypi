# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  # Exclude the test target when building with chromium.
  'conditions': [   
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'video_quality_measurement',
          'type': 'executable',
          'dependencies': [
            'video_codecs_test_framework',
            'webrtc_video_coding',
            'webrtc_vp8',
            '<(webrtc_root)/../test/metrics.gyp:metrics',
            '<(webrtc_root)/../third_party/google-gflags/google-gflags.gyp:google-gflags',
           ],
           'sources': [
             'video_quality_measurement.cc',
           ],
        },
      ], # targets
    }], # build_with_chromium
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2: