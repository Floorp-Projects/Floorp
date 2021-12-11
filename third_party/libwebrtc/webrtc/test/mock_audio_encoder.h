/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_MOCK_AUDIO_ENCODER_H_
#define TEST_MOCK_AUDIO_ENCODER_H_

#include <string>

#include "api/array_view.h"
#include "api/audio_codecs/audio_encoder.h"
#include "test/gmock.h"

namespace webrtc {

class MockAudioEncoder : public AudioEncoder {
 public:
  // TODO(nisse): Valid overrides commented out, because the gmock
  // methods don't use any override declarations, and we want to avoid
  // warnings from -Winconsistent-missing-override. See
  // http://crbug.com/428099.
  MockAudioEncoder();
  ~MockAudioEncoder();
  MOCK_METHOD0(Die, void());
  MOCK_METHOD1(Mark, void(std::string desc));
  MOCK_CONST_METHOD0(SampleRateHz, int());
  MOCK_CONST_METHOD0(NumChannels, size_t());
  MOCK_CONST_METHOD0(RtpTimestampRateHz, int());
  MOCK_CONST_METHOD0(Num10MsFramesInNextPacket, size_t());
  MOCK_CONST_METHOD0(Max10MsFramesInAPacket, size_t());
  MOCK_CONST_METHOD0(GetTargetBitrate, int());
  MOCK_METHOD0(Reset, void());
  MOCK_METHOD1(SetFec, bool(bool enable));
  MOCK_METHOD1(SetDtx, bool(bool enable));
  MOCK_METHOD1(SetApplication, bool(Application application));
  MOCK_METHOD1(SetMaxPlaybackRate, void(int frequency_hz));
  MOCK_METHOD1(SetMaxBitrate, void(int max_bps));
  MOCK_METHOD1(SetMaxPayloadSize, void(int max_payload_size_bytes));
  MOCK_METHOD2(OnReceivedUplinkBandwidth,
               void(int target_audio_bitrate_bps,
                    rtc::Optional<int64_t> probing_interval_ms));
  MOCK_METHOD1(OnReceivedUplinkPacketLossFraction,
               void(float uplink_packet_loss_fraction));

  MOCK_METHOD2(EnableAudioNetworkAdaptor,
               bool(const std::string& config_string, RtcEventLog* event_log));

  // Note, we explicitly chose not to create a mock for the Encode method.
  MOCK_METHOD3(EncodeImpl,
               EncodedInfo(uint32_t timestamp,
                           rtc::ArrayView<const int16_t> audio,
                           rtc::Buffer* encoded));

  class FakeEncoding {
   public:
    // Creates a functor that will return |info| and adjust the rtc::Buffer
    // given as input to it, so it is info.encoded_bytes larger.
    explicit FakeEncoding(const AudioEncoder::EncodedInfo& info);

    // Shorthand version of the constructor above, for when only setting
    // encoded_bytes in the EncodedInfo object matters.
    explicit FakeEncoding(size_t encoded_bytes);

    AudioEncoder::EncodedInfo operator()(uint32_t timestamp,
                                         rtc::ArrayView<const int16_t> audio,
                                         rtc::Buffer* encoded);

   private:
    AudioEncoder::EncodedInfo info_;
  };

  class CopyEncoding {
   public:
    ~CopyEncoding();

    // Creates a functor that will return |info| and append the data in the
    // payload to the buffer given as input to it. Up to info.encoded_bytes are
    // appended - make sure the payload is big enough!  Since it uses an
    // ArrayView, it _does not_ copy the payload. Make sure it doesn't fall out
    // of scope!
    CopyEncoding(AudioEncoder::EncodedInfo info,
                 rtc::ArrayView<const uint8_t> payload);

    // Shorthand version of the constructor above, for when you wish to append
    // the whole payload and do not care about any EncodedInfo attribute other
    // than encoded_bytes.
    explicit CopyEncoding(rtc::ArrayView<const uint8_t> payload);

    AudioEncoder::EncodedInfo operator()(uint32_t timestamp,
                                         rtc::ArrayView<const int16_t> audio,
                                         rtc::Buffer* encoded);

   private:
    AudioEncoder::EncodedInfo info_;
    rtc::ArrayView<const uint8_t> payload_;
  };
};

}  // namespace webrtc

#endif  // TEST_MOCK_AUDIO_ENCODER_H_
