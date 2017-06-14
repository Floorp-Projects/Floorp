/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_STANDARD_TEST_H
#define WEBRTC_VOICE_ENGINE_VOE_STANDARD_TEST_H

#include <stdio.h>
#include <string>

#include "gflags/gflags.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/include/voe_file.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/test/auto_test/voe_test_common.h"
#include "webrtc/voice_engine/test/auto_test/voe_test_interface.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_external_media.h"
#include "webrtc/voice_engine/include/voe_hardware.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"

namespace webrtc {
class VoENetEqStats;
}

#if defined(WEBRTC_ANDROID)
extern char mobileLogMsg[640];
#endif

DECLARE_bool(include_timing_dependent_tests);

namespace voetest {

class SubAPIManager {
 public:
  SubAPIManager()
    : _base(true),
      _codec(false),
      _externalMedia(false),
      _file(false),
      _hardware(false),
      _netEqStats(false),
      _network(false),
      _rtp_rtcp(false),
      _videoSync(false),
      _volumeControl(false),
      _apm(false) {
      _codec = true;
      _externalMedia = true;
      _file = true;
      _hardware = true;
      _netEqStats = true;
      _network = true;
      _rtp_rtcp = true;
      _videoSync = true;
      _volumeControl = true;
      _apm = true;
  }

  void DisplayStatus() const;

 private:
  bool _base, _codec;
  bool _externalMedia, _file, _hardware;
  bool _netEqStats, _network, _rtp_rtcp, _videoSync, _volumeControl, _apm;
};

class VoETestManager {
 public:
  VoETestManager();
  ~VoETestManager();

  // Must be called after construction.
  bool Init();

  void GetInterfaces();
  int ReleaseInterfaces();

  const char* AudioFilename() const {
    const std::string& result =
        webrtc::test::ResourcePath("voice_engine/audio_long16", "pcm");
    return result.c_str();
  }

  VoiceEngine* VoiceEnginePtr() const {
    return voice_engine_;
  }
  VoEBase* BasePtr() const {
    return voe_base_;
  }
  VoECodec* CodecPtr() const {
    return voe_codec_;
  }
  VoEVolumeControl* VolumeControlPtr() const {
    return voe_volume_control_;
  }
  VoERTP_RTCP* RTP_RTCPPtr() const {
    return voe_rtp_rtcp_;
  }
  VoEAudioProcessing* APMPtr() const {
    return voe_apm_;
  }

  VoENetwork* NetworkPtr() const {
    return voe_network_;
  }

  VoEFile* FilePtr() const {
    return voe_file_;
  }

  VoEHardware* HardwarePtr() const {
    return voe_hardware_;
  }

  VoEVideoSync* VideoSyncPtr() const {
    return voe_vsync_;
  }

  VoEExternalMedia* ExternalMediaPtr() const {
    return voe_xmedia_;
  }

  VoENetEqStats* NetEqStatsPtr() const {
    return voe_neteq_stats_;
  }

 private:
  bool                   initialized_;

  VoiceEngine*           voice_engine_;
  VoEBase*               voe_base_;
  VoECodec*              voe_codec_;
  VoEExternalMedia*      voe_xmedia_;
  VoEFile*               voe_file_;
  VoEHardware*           voe_hardware_;
  VoENetwork*            voe_network_;
  VoENetEqStats*         voe_neteq_stats_;
  VoERTP_RTCP*           voe_rtp_rtcp_;
  VoEVideoSync*          voe_vsync_;
  VoEVolumeControl*      voe_volume_control_;
  VoEAudioProcessing*    voe_apm_;
};

}  // namespace voetest

#endif // WEBRTC_VOICE_ENGINE_VOE_STANDARD_TEST_H
