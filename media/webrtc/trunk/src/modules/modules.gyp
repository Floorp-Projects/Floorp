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
    'audio_coding/codecs/iSAC/main/source/isac.gypi',
    'audio_coding/codecs/iSAC/fix/source/isacfix.gypi',
    'audio_coding/codecs/pcm16b/pcm16b.gypi',
    'audio_coding/main/source/audio_coding_module.gypi',
    'audio_coding/neteq/neteq.gypi',
    'audio_conference_mixer/source/audio_conference_mixer.gypi',
    'audio_device/main/source/audio_device.gypi',
    'audio_processing/audio_processing.gypi',
    'audio_processing/aec/aec.gypi',
    'audio_processing/aecm/aecm.gypi',
    'audio_processing/agc/agc.gypi',
    'audio_processing/ns/ns.gypi',
    'audio_processing/utility/util.gypi',
    'media_file/source/media_file.gypi',
    'udp_transport/source/udp_transport.gypi',
    'utility/source/utility.gypi',
    'video_coding/codecs/i420/main/source/i420.gypi',
    'video_coding/codecs/test_framework/test_framework.gypi',
    'video_coding/codecs/vp8/main/source/vp8.gypi',
    'video_coding/main/source/video_coding.gypi',
    'video_capture/main/source/video_capture.gypi',
    'video_processing/main/source/video_processing.gypi',
    'video_render/main/source/video_render.gypi',
    'rtp_rtcp/source/rtp_rtcp.gypi',
  ],

  # Test targets, excluded when building with Chromium.
  'conditions': [
    ['build_with_chromium==0', {
      'includes': [
        'audio_coding/codecs/iSAC/isac_test.gypi',
        'audio_coding/codecs/iSAC/isacfix_test.gypi',
        'audio_processing/apm_tests.gypi',
        'rtp_rtcp/source/rtp_rtcp_tests.gypi',
        'rtp_rtcp/test/test_bwe/test_bwe.gypi',
        'rtp_rtcp/test/testFec/test_fec.gypi',
        'rtp_rtcp/test/testAPI/test_api.gypi',
        'video_coding/main/source/video_coding_test.gypi',
        'video_coding/codecs/test/video_codecs_test_framework.gypi',
        'video_coding/codecs/tools/video_codecs_tools.gypi',
        'video_processing/main/test/vpm_tests.gypi',
      ], # includes
    }], # build_with_chromium
  ], # conditions
}
