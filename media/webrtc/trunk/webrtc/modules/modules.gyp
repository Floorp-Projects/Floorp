# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../build/common.gypi',
    'audio_coding/codecs/cng/cng.gypi',
    'audio_coding/codecs/g711/g711.gypi',
    'audio_coding/codecs/g722/g722.gypi',
    'audio_coding/codecs/ilbc/ilbc.gypi',
    'audio_coding/codecs/isac/main/source/isac.gypi',
    'audio_coding/codecs/isac/fix/source/isacfix.gypi',
    'audio_coding/codecs/pcm16b/pcm16b.gypi',
    'audio_coding/main/source/audio_coding_module.gypi',
    'audio_coding/neteq/neteq.gypi',
    'audio_coding/neteq4/neteq.gypi',
    'audio_conference_mixer/source/audio_conference_mixer.gypi',
    'audio_device/audio_device.gypi',
    'audio_processing/audio_processing.gypi',
    'bitrate_controller/bitrate_controller.gypi',
    'desktop_capture/desktop_capture.gypi',
    'media_file/source/media_file.gypi',
    'pacing/pacing.gypi',
    'remote_bitrate_estimator/remote_bitrate_estimator.gypi',
    'rtp_rtcp/source/rtp_rtcp.gypi',
    'utility/source/utility.gypi',
    'video_coding/codecs/i420/main/source/i420.gypi',
    'video_coding/main/source/video_coding.gypi',
    'video_capture/video_capture.gypi',
    'video_processing/main/source/video_processing.gypi',
    'video_render/video_render.gypi',
  ],
  'conditions': [
    ['include_opus==1', {
      'includes': ['audio_coding/codecs/opus/opus.gypi',],
    }],
    ['include_tests==1', {
      'includes': [
        'audio_coding/codecs/isac/isac_test.gypi',
        'audio_coding/codecs/isac/isacfix_test.gypi',
        'audio_processing/audio_processing_tests.gypi',
        'rtp_rtcp/source/rtp_rtcp_tests.gypi',
        'rtp_rtcp/test/testFec/test_fec.gypi',
        'video_coding/main/source/video_coding_test.gypi',
        'video_coding/codecs/test/video_codecs_test_framework.gypi',
        'video_coding/codecs/tools/video_codecs_tools.gypi',
        'video_processing/main/test/vpm_tests.gypi',
      ], # includes
      'targets': [
        {
          'target_name': 'modules_unittests',
          'type': 'executable',
          'dependencies': [
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'module_common_types_unittest.cc',
          ],
        },
      ],
    }], # include_tests
  ], # conditions
}
