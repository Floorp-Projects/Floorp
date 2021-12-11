/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/fake_decode_from_file.h"

#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace test {

int FakeDecodeFromFile::DecodeInternal(const uint8_t* encoded,
                                       size_t encoded_len,
                                       int sample_rate_hz,
                                       int16_t* decoded,
                                       SpeechType* speech_type) {
  if (encoded_len == 0) {
    // Decoder is asked to produce codec-internal comfort noise.
    RTC_DCHECK(!encoded);  // NetEq always sends nullptr in this case.
    RTC_DCHECK(cng_mode_);
    RTC_DCHECK_GT(last_decoded_length_, 0);
    std::fill_n(decoded, last_decoded_length_, 0);
    *speech_type = kComfortNoise;
    return rtc::dchecked_cast<int>(last_decoded_length_);
  }

  RTC_CHECK_GE(encoded_len, 12);
  uint32_t timestamp_to_decode =
      ByteReader<uint32_t>::ReadLittleEndian(encoded);
  uint32_t samples_to_decode =
      ByteReader<uint32_t>::ReadLittleEndian(&encoded[4]);
  if (samples_to_decode == 0) {
    // Number of samples in packet is unknown.
    if (last_decoded_length_ > 0) {
      // Use length of last decoded packet, but since this is the total for all
      // channels, we have to divide by 2 in the stereo case.
      samples_to_decode = rtc::dchecked_cast<int>(rtc::CheckedDivExact(
          last_decoded_length_, static_cast<size_t>(stereo_ ? 2uL : 1uL)));
    } else {
      // This is the first packet to decode, and we do not know the length of
      // it. Set it to 10 ms.
      samples_to_decode = rtc::CheckedDivExact(sample_rate_hz, 100);
    }
  }

  if (next_timestamp_from_input_ &&
      timestamp_to_decode != *next_timestamp_from_input_) {
    // A gap in the timestamp sequence is detected. Skip the same number of
    // samples from the file.
    uint32_t jump = timestamp_to_decode - *next_timestamp_from_input_;
    RTC_CHECK(input_->Seek(jump));
  }

  next_timestamp_from_input_ = timestamp_to_decode + samples_to_decode;

  uint32_t original_payload_size_bytes =
      ByteReader<uint32_t>::ReadLittleEndian(&encoded[8]);
  if (original_payload_size_bytes == 1) {
    // This is a comfort noise payload.
    RTC_DCHECK_GT(last_decoded_length_, 0);
    std::fill_n(decoded, last_decoded_length_, 0);
    *speech_type = kComfortNoise;
    cng_mode_ = true;
    return rtc::dchecked_cast<int>(last_decoded_length_);
  }

  cng_mode_ = false;
  RTC_CHECK(input_->Read(static_cast<size_t>(samples_to_decode), decoded));

  if (stereo_) {
    InputAudioFile::DuplicateInterleaved(decoded, samples_to_decode, 2,
                                         decoded);
    samples_to_decode *= 2;
  }

  *speech_type = kSpeech;
  last_decoded_length_ = samples_to_decode;
  return rtc::dchecked_cast<int>(last_decoded_length_);
}

void FakeDecodeFromFile::PrepareEncoded(uint32_t timestamp,
                                        size_t samples,
                                        size_t original_payload_size_bytes,
                                        rtc::ArrayView<uint8_t> encoded) {
  RTC_CHECK_GE(encoded.size(), 12);
  ByteWriter<uint32_t>::WriteLittleEndian(&encoded[0], timestamp);
  ByteWriter<uint32_t>::WriteLittleEndian(&encoded[4],
                                          rtc::checked_cast<uint32_t>(samples));
  ByteWriter<uint32_t>::WriteLittleEndian(
      &encoded[8], rtc::checked_cast<uint32_t>(original_payload_size_bytes));
}

}  // namespace test
}  // namespace webrtc
