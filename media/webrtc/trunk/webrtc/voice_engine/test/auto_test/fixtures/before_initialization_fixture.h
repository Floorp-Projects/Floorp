/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_H_
#define SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_H_

#include <assert.h>

#include "common_types.h"
#include "engine_configurations.h"
#include "test/testsupport/gtest_disable.h"
#include "voice_engine/include/voe_audio_processing.h"
#include "voice_engine/include/voe_base.h"
#include "voice_engine/include/voe_call_report.h"
#include "voice_engine/include/voe_codec.h"
#include "voice_engine/include/voe_dtmf.h"
#include "voice_engine/include/voe_encryption.h"
#include "voice_engine/include/voe_errors.h"
#include "voice_engine/include/voe_external_media.h"
#include "voice_engine/include/voe_file.h"
#include "voice_engine/include/voe_hardware.h"
#include "voice_engine/include/voe_neteq_stats.h"
#include "voice_engine/include/voe_network.h"
#include "voice_engine/include/voe_rtp_rtcp.h"
#include "voice_engine/include/voe_video_sync.h"
#include "voice_engine/include/voe_volume_control.h"
#include "voice_engine/test/auto_test/voe_test_defines.h"

// TODO(qhogpat): Remove these undefs once the clashing macros are gone.
#undef TEST
#undef ASSERT_TRUE
#undef ASSERT_FALSE
#include "gtest/gtest.h"
#include "gmock/gmock.h"

// This convenient fixture sets up all voice engine interfaces automatically for
// use by testing subclasses. It allocates each interface and releases it once
// which means that if a tests allocates additional interfaces from the voice
// engine and forgets to release it, this test will fail in the destructor.
// It will not call any init methods.
//
// Implementation note:
// The interface fetching is done in the constructor and not SetUp() since
// this relieves our subclasses from calling SetUp in the superclass if they
// choose to override SetUp() themselves. This is fine as googletest will
// construct new test objects for each method.
class BeforeInitializationFixture : public testing::Test {
 public:
  BeforeInitializationFixture();
  virtual ~BeforeInitializationFixture();

 protected:
  // Use this sleep function to sleep in tests.
  void Sleep(long milliseconds);

  webrtc::VoiceEngine*        voice_engine_;
  webrtc::VoEBase*            voe_base_;
  webrtc::VoECodec*           voe_codec_;
  webrtc::VoEVolumeControl*   voe_volume_control_;
  webrtc::VoEDtmf*            voe_dtmf_;
  webrtc::VoERTP_RTCP*        voe_rtp_rtcp_;
  webrtc::VoEAudioProcessing* voe_apm_;
  webrtc::VoENetwork*         voe_network_;
  webrtc::VoEFile*            voe_file_;
  webrtc::VoEVideoSync*       voe_vsync_;
  webrtc::VoEEncryption*      voe_encrypt_;
  webrtc::VoEHardware*        voe_hardware_;
  webrtc::VoEExternalMedia*   voe_xmedia_;
  webrtc::VoECallReport*      voe_call_report_;
  webrtc::VoENetEqStats*      voe_neteq_stats_;
};

#endif  // SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_H_
