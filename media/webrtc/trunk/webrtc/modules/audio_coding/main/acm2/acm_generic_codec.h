/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_GENERIC_CODEC_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_GENERIC_CODEC_H_

#include <map>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/audio_coding/codecs/audio_decoder.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_common_defs.h"
#include "webrtc/modules/audio_coding/neteq/interface/neteq.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

#define MAX_FRAME_SIZE_10MSEC 6

// forward declaration
struct WebRtcVadInst;
struct WebRtcCngEncInst;

namespace webrtc {

struct WebRtcACMCodecParams;
struct CodecInst;
class CriticalSectionWrapper;

namespace acm2 {

// forward declaration
class AcmReceiver;

// Proxy for AudioDecoder
class AudioDecoderProxy final : public AudioDecoder {
 public:
  AudioDecoderProxy();
  void SetDecoder(AudioDecoder* decoder);
  bool IsSet() const;
  int Decode(const uint8_t* encoded,
             size_t encoded_len,
             int sample_rate_hz,
             size_t max_decoded_bytes,
             int16_t* decoded,
             SpeechType* speech_type) override;
  int DecodeRedundant(const uint8_t* encoded,
                      size_t encoded_len,
                      int sample_rate_hz,
                      size_t max_decoded_bytes,
                      int16_t* decoded,
                      SpeechType* speech_type) override;
  bool HasDecodePlc() const override;
  int DecodePlc(int num_frames, int16_t* decoded) override;
  int Init() override;
  int IncomingPacket(const uint8_t* payload,
                     size_t payload_len,
                     uint16_t rtp_sequence_number,
                     uint32_t rtp_timestamp,
                     uint32_t arrival_timestamp) override;
  int ErrorCode() override;
  int PacketDuration(const uint8_t* encoded, size_t encoded_len) const override;
  int PacketDurationRedundant(const uint8_t* encoded,
                              size_t encoded_len) const override;
  bool PacketHasFec(const uint8_t* encoded, size_t encoded_len) const override;
  CNG_dec_inst* CngDecoderInstance() override;
  size_t Channels() const override;

 private:
  rtc::scoped_ptr<CriticalSectionWrapper> decoder_lock_;
  AudioDecoder* decoder_ GUARDED_BY(decoder_lock_);
};

class ACMGenericCodec {
 public:
  ACMGenericCodec(const CodecInst& codec_inst,
                  int cng_pt_nb,
                  int cng_pt_wb,
                  int cng_pt_swb,
                  int cng_pt_fb,
                  bool enable_red,
                  int red_pt_nb);
  ~ACMGenericCodec();

  ///////////////////////////////////////////////////////////////////////////
  // ACMGenericCodec* CreateInstance();
  // The function will be used for FEC. It is not implemented yet.
  //
  ACMGenericCodec* CreateInstance();

  ///////////////////////////////////////////////////////////////////////////
  // bool EncoderInitialized();
  //
  // Return value:
  //   True if the encoder is successfully initialized,
  //   false otherwise.
  //
  bool EncoderInitialized();

  ///////////////////////////////////////////////////////////////////////////
  // int16_t EncoderParams()
  // It is called to get encoder parameters. It will call
  // EncoderParamsSafe() in turn.
  //
  // Output:
  //   -enc_params         : a buffer where the encoder parameters is
  //                         written to. If the encoder is not
  //                         initialized this buffer is filled with
  //                         invalid values
  // Return value:
  //   -1 if the encoder is not initialized,
  //    0 otherwise.
  //
  int16_t EncoderParams(WebRtcACMCodecParams* enc_params) const;

  ///////////////////////////////////////////////////////////////////////////
  // int16_t InitEncoder(...)
  // This function is called to initialize the encoder with the given
  // parameters.
  //
  // Input:
  //   -codec_params        : parameters of encoder.
  //   -force_initialization: if false the initialization is invoked only if
  //                          the encoder is not initialized. If true the
  //                          encoder is forced to (re)initialize.
  //
  // Return value:
  //   0 if could initialize successfully,
  //  -1 if failed to initialize.
  //
  //
  int16_t InitEncoder(WebRtcACMCodecParams* codec_params,
                      bool force_initialization);

  ///////////////////////////////////////////////////////////////////////////
  // uint32_t NoMissedSamples()
  // This function returns the number of samples which are overwritten in
  // the audio buffer. The audio samples are overwritten if the input audio
  // buffer is full, but Add10MsData() is called. (We might remove this
  // function if it is not used)
  //
  // Return Value:
  //   Number of samples which are overwritten.
  //
  uint32_t NoMissedSamples() const;

  ///////////////////////////////////////////////////////////////////////////
  // void ResetNoMissedSamples()
  // This function resets the number of overwritten samples to zero.
  // (We might remove this function if we remove NoMissedSamples())
  //
  void ResetNoMissedSamples();

  ///////////////////////////////////////////////////////////////////////////
  // int16_t SetBitRate()
  // The function is called to set the encoding rate.
  //
  // Input:
  //   -bitrate_bps        : encoding rate in bits per second
  //
  // Return value:
  //   -1 if failed to set the rate, due to invalid input or given
  //      codec is not rate-adjustable.
  //    0 if the rate is adjusted successfully
  //
  int16_t SetBitRate(const int32_t bitrate_bps);

  ///////////////////////////////////////////////////////////////////////////
  // uint32_t EarliestTimestamp()
  // Returns the timestamp of the first 10 ms in audio buffer. This is used
  // to identify if a synchronization of two encoders is required.
  //
  // Return value:
  //   timestamp of the first 10 ms audio in the audio buffer.
  //
  uint32_t EarliestTimestamp() const;

  ///////////////////////////////////////////////////////////////////////////
  // int16_t SetVAD()
  // This is called to set VAD & DTX. If the codec has internal DTX, it will
  // be used. If DTX is enabled and the codec does not have internal DTX,
  // WebRtc-VAD will be used to decide if the frame is active. If DTX is
  // disabled but VAD is enabled, the audio is passed through VAD to label it
  // as active or passive, but the frame is  encoded normally. However the
  // bit-stream is labeled properly so that ACM::Process() can use this
  // information. In case of failure, the previous states of the VAD & DTX
  // are kept.
  //
  // Inputs/Output:
  //   -enable_dtx         : if true DTX will be enabled otherwise the DTX is
  //                         disabled. If codec has internal DTX that will be
  //                         used, otherwise WebRtc-CNG is used. In the latter
  //                         case VAD is automatically activated.
  //   -enable_vad         : if true WebRtc-VAD is enabled, otherwise VAD is
  //                         disabled, except for the case that DTX is enabled
  //                         but codec doesn't have internal DTX. In this case
  //                         VAD is enabled regardless of the value of
  //                         |enable_vad|.
  //   -mode               : this specifies the aggressiveness of VAD.
  //
  // Return value
  //   -1 if failed to set DTX & VAD as specified,
  //    0 if succeeded.
  //
  int16_t SetVAD(bool* enable_dtx, bool* enable_vad, ACMVADMode* mode);

  // Registers comfort noise at |sample_rate_hz| to use |payload_type|.
  void SetCngPt(int sample_rate_hz, int payload_type);

  // Registers RED at |sample_rate_hz| to use |payload_type|.
  void SetRedPt(int sample_rate_hz, int payload_type);

  ///////////////////////////////////////////////////////////////////////////
  // UpdateEncoderSampFreq()
  // Call this function to update the encoder sampling frequency. This
  // is for codecs where one payload-name supports several encoder sampling
  // frequencies. Otherwise, to change the sampling frequency we need to
  // register new codec. ACM will consider that as registration of a new
  // codec, not a change in parameter. For iSAC, switching from WB to SWB
  // is treated as a change in parameter. Therefore, we need this function.
  //
  // Input:
  //   -samp_freq_hz        : encoder sampling frequency.
  //
  // Return value:
  //   -1 if failed, or if this is meaningless for the given codec.
  //    0 if succeeded.
  //
  int16_t UpdateEncoderSampFreq(uint16_t samp_freq_hz);

  ///////////////////////////////////////////////////////////////////////////
  // EncoderSampFreq()
  // Get the sampling frequency that the encoder (WebRtc wrapper) expects.
  //
  // Output:
  //   -samp_freq_hz       : sampling frequency, in Hertz, which the encoder
  //                         should be fed with.
  //
  // Return value:
  //   -1 if failed to output sampling rate.
  //    0 if the sample rate is returned successfully.
  //
  int16_t EncoderSampFreq(uint16_t* samp_freq_hz);

  ///////////////////////////////////////////////////////////////////////////
  // SetISACMaxPayloadSize()
  // Set the maximum payload size of iSAC packets. No iSAC payload,
  // regardless of its frame-size, may exceed the given limit. For
  // an iSAC payload of size B bits and frame-size T sec we have;
  // (B < max_payload_len_bytes * 8) and (B/T < max_rate_bit_per_sec), c.f.
  // SetISACMaxRate().
  //
  // Input:
  //   -max_payload_len_bytes : maximum payload size in bytes.
  //
  // Return value:
  //   -1 if failed to set the maximum  payload-size.
  //    0 if the given length is set successfully.
  //
  int32_t SetISACMaxPayloadSize(const uint16_t max_payload_len_bytes);

  ///////////////////////////////////////////////////////////////////////////
  // SetISACMaxRate()
  // Set the maximum instantaneous rate of iSAC. For a payload of B bits
  // with a frame-size of T sec the instantaneous rate is B/T bits per
  // second. Therefore, (B/T < max_rate_bit_per_sec) and
  // (B < max_payload_len_bytes * 8) are always satisfied for iSAC payloads,
  // c.f SetISACMaxPayloadSize().
  //
  // Input:
  //   -max_rate_bps       : maximum instantaneous bit-rate given in bits/sec.
  //
  // Return value:
  //   -1 if failed to set the maximum rate.
  //    0 if the maximum rate is set successfully.
  //
  int32_t SetISACMaxRate(const uint32_t max_rate_bps);

  ///////////////////////////////////////////////////////////////////////////
  // int SetOpusApplication(OpusApplicationMode application,
  //                        bool disable_dtx_if_needed)
  // Sets the intended application for the Opus encoder. Opus uses this to
  // optimize the encoding for applications like VOIP and music. Currently, two
  // modes are supported: kVoip and kAudio. kAudio is only allowed when Opus
  // DTX is switched off. If DTX is on, and |application| == kAudio, a failure
  // will be triggered unless |disable_dtx_if_needed| == true, for which, the
  // DTX will be forced off.
  //
  // Input:
  //   - application            : intended application.
  //   - disable_dtx_if_needed  : whether to force Opus DTX to stop when needed.
  //
  // Return value:
  //   -1 if failed or on codecs other than Opus.
  //    0 if succeeded.
  //
  int SetOpusApplication(OpusApplicationMode application,
                         bool disable_dtx_if_needed);

  ///////////////////////////////////////////////////////////////////////////
  // int SetOpusMaxPlaybackRate()
  // Sets maximum playback rate the receiver will render, if the codec is Opus.
  // This is to tell Opus that it is enough to code the input audio up to a
  // bandwidth. Opus can take this information to optimize the bit rate and
  // increase the computation efficiency.
  //
  // Input:
  //   -frequency_hz      : maximum playback rate in Hz.
  //
  // Return value:
  //   -1 if failed or on codecs other than Opus.
  //    0 if succeeded.
  //
  int SetOpusMaxPlaybackRate(int /* frequency_hz */);

  ///////////////////////////////////////////////////////////////////////////
  // EnableOpusDtx(bool force_voip)
  // Enable the DTX, if the codec is Opus. Currently, DTX can only be enabled
  // when the application mode is kVoip. If |force_voip| == true, the
  // application mode will be forced to kVoip. Otherwise, a failure will be
  // triggered if current application mode is kAudio.
  // Input:
  //   - force_voip  : whether to force application mode to kVoip.
  // Return value:
  //   -1 if failed or on codecs other than Opus.
  //    0 if succeeded.
  //
  int EnableOpusDtx(bool force_voip);

  ///////////////////////////////////////////////////////////////////////////
  // DisbleOpusDtx()
  // Disable the DTX, if the codec is Opus.
  // Return value:
  //   -1 if failed or on codecs other than Opus.
  //    0 if succeeded.
  //
  int DisableOpusDtx();

  ///////////////////////////////////////////////////////////////////////////
  // HasFrameToEncode()
  // Returns true if there is enough audio buffered for encoding, such that
  // calling Encode() will return a payload.
  //
  bool HasFrameToEncode() const;

  // Returns a pointer to the AudioDecoder part of a joint encoder-decoder
  // object, if it exists. Otherwise, nullptr is returned.
  AudioDecoder* Decoder();

  ///////////////////////////////////////////////////////////////////////////
  // bool HasInternalFEC()
  // Used to check if the codec has internal FEC.
  //
  // Return value:
  //   true if the codec has an internal FEC, e.g. Opus.
  //   false otherwise.
  //
  bool HasInternalFEC() const {
    return has_internal_fec_;
  }

  ///////////////////////////////////////////////////////////////////////////
  // int SetFEC();
  // Sets the codec internal FEC. No effects on codecs that do not provide
  // internal FEC.
  //
  // Input:
  //   -enable_fec         : if true FEC will be enabled otherwise the FEC is
  //                         disabled.
  //
  // Return value:
  //   -1 if failed,
  //    0 if succeeded.
  //
  int SetFEC(bool enable_fec);

  ///////////////////////////////////////////////////////////////////////////
  // int SetPacketLossRate()
  // Sets expected packet loss rate for encoding. Some encoders provide packet
  // loss gnostic encoding to make stream less sensitive to packet losses,
  // through e.g., FEC. No effects on codecs that do not provide such encoding.
  //
  // Input:
  //   -loss_rate          : expected packet loss rate (0 -- 100 inclusive).
  //
  // Return value:
  //   -1 if failed,
  //    0 if succeeded or packet loss rate is ignored.
  //
  int SetPacketLossRate(int /* loss_rate */);

  ///////////////////////////////////////////////////////////////////////////
  // int SetCopyRed()
  // Enable or disable copy RED. It fails if there is no RED payload that
  // matches the codec, e.g., sample rate differs.
  //
  // Return value:
  //   -1 if failed,
  //    0 if succeeded.
  int SetCopyRed(bool enable);

  AudioEncoder* GetAudioEncoder();

  const AudioEncoder* GetAudioEncoder() const;

 private:
  bool has_internal_fec_;

  bool copy_red_enabled_;

  void ResetAudioEncoder();

  OpusApplicationMode GetOpusApplication(int num_channels,
                                         bool enable_dtx) const;

  rtc::scoped_ptr<AudioEncoder> audio_encoder_;
  rtc::scoped_ptr<AudioEncoder> cng_encoder_;
  rtc::scoped_ptr<AudioEncoder> red_encoder_;
  AudioEncoder* encoder_;
  AudioDecoderProxy decoder_proxy_;
  WebRtcACMCodecParams acm_codec_params_;
  int bitrate_bps_;
  bool fec_enabled_;
  int loss_rate_;
  int max_playback_rate_hz_;
  int max_payload_size_bytes_;
  int max_rate_bps_;
  bool opus_dtx_enabled_;
  bool is_opus_;
  bool is_isac_;
  // Map from payload type to CNG sample rate (Hz).
  std::map<int, int> cng_pt_;
  // Map from payload type to RED sample rate (Hz).
  std::map<int, int> red_pt_;
  OpusApplicationMode opus_application_;
  bool opus_application_set_;
};

}  // namespace acm2

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_GENERIC_CODEC_H_
