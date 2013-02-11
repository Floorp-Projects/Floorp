/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_INTERFACE_AUDIO_CODING_MODULE_H
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_INTERFACE_AUDIO_CODING_MODULE_H

#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/interface/module.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// forward declarations
struct CodecInst;
struct WebRtcRTPHeader;
class AudioFrame;
class RTPFragmentationHeader;

#define WEBRTC_10MS_PCM_AUDIO 960 // 16 bits super wideband 48 kHz

// Callback class used for sending data ready to be packetized
class AudioPacketizationCallback {
 public:
  virtual ~AudioPacketizationCallback() {}

  virtual WebRtc_Word32 SendData(
      FrameType frame_type,
      WebRtc_UWord8 payload_type,
      WebRtc_UWord32 timestamp,
      const WebRtc_UWord8* payload_data,
      WebRtc_UWord16 payload_len_bytes,
      const RTPFragmentationHeader* fragmentation) = 0;
};

// Callback class used for inband Dtmf detection
class AudioCodingFeedback {
 public:
  virtual ~AudioCodingFeedback() {}

  virtual WebRtc_Word32 IncomingDtmf(const WebRtc_UWord8 digit_dtmf,
                                     const bool end) = 0;
};

// Callback class used for reporting VAD decision
class ACMVADCallback {
 public:
  virtual ~ACMVADCallback() {}

  virtual WebRtc_Word32 InFrameType(WebRtc_Word16 frameType) = 0;
};

// Callback class used for reporting receiver statistics
class ACMVQMonCallback {
 public:
  virtual ~ACMVQMonCallback() {}

  virtual WebRtc_Word32 NetEqStatistics(
      const WebRtc_Word32 id, // current ACM id
      const WebRtc_UWord16 MIUsValid, // valid voice duration in ms
      const WebRtc_UWord16 MIUsReplaced, // concealed voice duration in ms
      const WebRtc_UWord8 eventFlags, // concealed voice flags
      const WebRtc_UWord16 delayMS) = 0; // average delay in ms
};

class AudioCodingModule: public Module {
 protected:
  AudioCodingModule() {}
  virtual ~AudioCodingModule() {}

 public:
  ///////////////////////////////////////////////////////////////////////////
  // Creation and destruction of a ACM
  //
  static AudioCodingModule* Create(const WebRtc_Word32 id);

  static void Destroy(AudioCodingModule* module);

  ///////////////////////////////////////////////////////////////////////////
  //   Utility functions
  //

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_UWord8 NumberOfCodecs()
  // Returns number of supported codecs.
  //
  // Return value:
  //   number of supported codecs.
  ///
  static WebRtc_UWord8 NumberOfCodecs();

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 Codec()
  // Get supported codec with list number.
  //
  // Input:
  //   -list_id             : list number.
  //
  // Output:
  //   -codec              : a structure where the parameters of the codec,
  //                         given by list number is written to.
  //
  // Return value:
  //   -1 if the list number (list_id) is invalid.
  //    0 if succeeded.
  //
  static WebRtc_Word32 Codec(const WebRtc_UWord8 list_id, CodecInst& codec);

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 Codec()
  // Get supported codec with the given codec name, sampling frequency, and
  // a given number of channels.
  //
  // Input:
  //   -payload_name       : name of the codec.
  //   -sampling_freq_hz   : sampling frequency of the codec. Note! for RED
  //                         a sampling frequency of -1 is a valid input.
  //   -channels           : number of channels ( 1 - mono, 2 - stereo).
  //
  // Output:
  //   -codec              : a structure where the function returns the
  //                         default parameters of the codec.
  //
  // Return value:
  //   -1 if no codec matches the given parameters.
  //    0 if succeeded.
  //
  static WebRtc_Word32 Codec(const char* payload_name, CodecInst& codec,
                             int sampling_freq_hz, int channels);

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 Codec()
  //
  // Returns the list number of the given codec name, sampling frequency, and
  // a given number of channels.
  //
  // Input:
  //   -payload_name        : name of the codec.
  //   -sampling_freq_hz    : sampling frequency of the codec. Note! for RED
  //                          a sampling frequency of -1 is a valid input.
  //   -channels            : number of channels ( 1 - mono, 2 - stereo).
  //
  // Return value:
  //   if the codec is found, the index of the codec in the list,
  //   -1 if the codec is not found.
  //
  static WebRtc_Word32 Codec(const char* payload_name, int sampling_freq_hz,
                             int channels);

  ///////////////////////////////////////////////////////////////////////////
  // bool IsCodecValid()
  // Checks the validity of the parameters of the given codec.
  //
  // Input:
  //   -codec              : the structure which keeps the parameters of the
  //                         codec.
  //
  // Return value:
  //   true if the parameters are valid,
  //   false if any parameter is not valid.
  //
  static bool IsCodecValid(const CodecInst& codec);

  ///////////////////////////////////////////////////////////////////////////
  //   Sender
  //

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 InitializeSender()
  // Any encoder-related state of ACM will be initialized to the
  // same state when ACM is created. This will not interrupt or
  // effect decoding functionality of ACM. ACM will lose all the
  // encoding-related settings by calling this function.
  // For instance, a send codec has to be registered again.
  //
  // Return value:
  //   -1 if failed to initialize,
  //    0 if succeeded.
  //
  virtual WebRtc_Word32 InitializeSender() = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 ResetEncoder()
  // This API resets the states of encoder. All the encoder settings, such as
  // send-codec or VAD/DTX, will be preserved.
  //
  // Return value:
  //   -1 if failed to initialize,
  //    0 if succeeded.
  //
  virtual WebRtc_Word32 ResetEncoder() = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 RegisterSendCodec()
  // Registers a codec, specified by |send_codec|, as sending codec.
  // This API can be called multiple of times to register Codec. The last codec
  // registered overwrites the previous ones.
  // The API can also be used to change payload type for CNG and RED, which are
  // registered by default to default payload types.
  // Note that registering CNG and RED won't overwrite speech codecs.
  // This API can be called to set/change the send payload-type, frame-size
  // or encoding rate (if applicable for the codec).
  //
  // Note: If a stereo codec is registered as send codec, VAD/DTX will
  // automatically be turned off, since it is not supported for stereo sending.
  //
  // Note: If a secondary encoder is already registered, and the new send-codec
  // has a sampling rate that does not match the secondary encoder, the
  // secondary encoder will be unregistered.
  //
  // Input:
  //   -send_codec         : Parameters of the codec to be registered, c.f.
  //                         common_types.h for the definition of
  //                         CodecInst.
  //
  // Return value:
  //   -1 if failed to initialize,
  //    0 if succeeded.
  //
  virtual WebRtc_Word32 RegisterSendCodec(const CodecInst& send_codec) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int RegisterSecondarySendCodec()
  // Register a secondary encoder to enable dual-streaming. If a secondary
  // codec is already registered, it will be removed before the new one is
  // registered.
  //
  // Note: The secondary encoder will be unregistered if a primary codec
  // is set with a sampling rate which does not match that of the existing
  // secondary codec.
  //
  // Input:
  //   -send_codec         : Parameters of the codec to be registered, c.f.
  //                         common_types.h for the definition of
  //                         CodecInst.
  //
  // Return value:
  //   -1 if failed to register,
  //    0 if succeeded.
  //
  virtual int RegisterSecondarySendCodec(const CodecInst& send_codec) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // void UnregisterSecondarySendCodec()
  // Unregister the secondary encoder to disable dual-streaming.
  //
  virtual void UnregisterSecondarySendCodec() = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 SendCodec()
  // Get parameters for the codec currently registered as send codec.
  //
  // Output:
  //   -current_send_codec          : parameters of the send codec.
  //
  // Return value:
  //   -1 if failed to get send codec,
  //    0 if succeeded.
  //
  virtual WebRtc_Word32 SendCodec(CodecInst& current_send_codec) const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int SecondarySendCodec()
  // Get the codec parameters for the current secondary send codec.
  //
  // Output:
  //   -secondary_codec          : parameters of the secondary send codec.
  //
  // Return value:
  //   -1 if failed to get send codec,
  //    0 if succeeded.
  //
  virtual int SecondarySendCodec(CodecInst* secondary_codec) const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 SendFrequency()
  // Get the sampling frequency of the current encoder in Hertz.
  //
  // Return value:
  //   positive; sampling frequency [Hz] of the current encoder.
  //   -1 if an error has happened.
  //
  virtual WebRtc_Word32 SendFrequency() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 Bitrate()
  // Get encoding bit-rate in bits per second.
  //
  // Return value:
  //   positive; encoding rate in bits/sec,
  //   -1 if an error is happened.
  //
  virtual WebRtc_Word32 SendBitrate() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 SetReceivedEstimatedBandwidth()
  // Set available bandwidth [bits/sec] of the up-link channel.
  // This information is used for traffic shaping, and is currently only
  // supported if iSAC is the send codec.
  //
  // Input:
  //   -bw                 : bandwidth in bits/sec estimated for
  //                         up-link.
  // Return value
  //   -1 if error occurred in setting the bandwidth,
  //    0 bandwidth is set successfully.
  //
  virtual WebRtc_Word32 SetReceivedEstimatedBandwidth(
      const WebRtc_Word32 bw) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 RegisterTransportCallback()
  // Register a transport callback which will be called to deliver
  // the encoded buffers whenever Process() is called and a
  // bit-stream is ready.
  //
  // Input:
  //   -transport          : pointer to the callback class
  //                         transport->SendData() is called whenever
  //                         Process() is called and bit-stream is ready
  //                         to deliver.
  //
  // Return value:
  //   -1 if the transport callback could not be registered
  //    0 if registration is successful.
  //
  virtual WebRtc_Word32 RegisterTransportCallback(
      AudioPacketizationCallback* transport) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 Add10MsData()
  // Add 10MS of raw (PCM) audio data to the encoder. If the sampling
  // frequency of the audio does not match the sampling frequency of the
  // current encoder ACM will resample the audio.
  //
  // Input:
  //   -audio_frame        : the input audio frame, containing raw audio
  //                         sampling frequency etc.,
  //                         c.f. module_common_types.h for definition of
  //                         AudioFrame.
  //
  // Return value:
  //      0   successfully added the frame.
  //     -1   some error occurred and data is not added.
  //   < -1   to add the frame to the buffer n samples had to be
  //          overwritten, -n is the return value in this case.
  //
  virtual WebRtc_Word32 Add10MsData(const AudioFrame& audio_frame) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // (FEC) Forward Error Correction
  //

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 SetFECStatus(const bool enable)
  // configure FEC status i.e. on/off.
  //
  // RFC 2198 describes a solution which has a single payload type which
  // signifies a packet with redundancy. That packet then becomes a container,
  // encapsulating multiple payloads into a single RTP packet.
  // Such a scheme is flexible, since any amount of redundancy may be
  // encapsulated within a single packet.  There is, however, a small overhead
  // since each encapsulated payload must be preceded by a header indicating
  // the type of data enclosed.
  //
  // This means that FEC is actually a RED scheme.
  //
  // Input:
  //   -enable_fec         : if true FEC is enabled, otherwise FEC is
  //                         disabled.
  //
  // Return value:
  //   -1 if failed to set FEC status,
  //    0 if succeeded.
  //
  virtual WebRtc_Word32 SetFECStatus(const bool enable_fec) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // bool FECStatus()
  // Get FEC status
  //
  // Return value
  //   true if FEC is enabled,
  //   false if FEC is disabled.
  //
  virtual bool FECStatus() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  //   (VAD) Voice Activity Detection
  //

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 SetVAD()
  // If DTX is enabled & the codec does not have internal DTX/VAD
  // WebRtc VAD will be automatically enabled and |enable_vad| is ignored.
  //
  // If DTX is disabled but VAD is enabled no DTX packets are send,
  // regardless of whether the codec has internal DTX/VAD or not. In this
  // case, WebRtc VAD is running to label frames as active/in-active.
  //
  // NOTE! VAD/DTX is not supported when sending stereo.
  //
  // Inputs:
  //   -enable_dtx         : if true DTX is enabled,
  //                         otherwise DTX is disabled.
  //   -enable_vad         : if true VAD is enabled,
  //                         otherwise VAD is disabled.
  //   -vad_mode           : determines the aggressiveness of VAD. A more
  //                         aggressive mode results in more frames labeled
  //                         as in-active, c.f. definition of
  //                         ACMVADMode in audio_coding_module_typedefs.h
  //                         for valid values.
  //
  // Return value:
  //   -1 if failed to set up VAD/DTX,
  //    0 if succeeded.
  //
  virtual WebRtc_Word32 SetVAD(const bool enable_dtx = true,
                               const bool enable_vad = false,
                               const ACMVADMode vad_mode = VADNormal) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 VAD()
  // Get VAD status.
  //
  // Outputs:
  //   -dtx_enabled        : is set to true if DTX is enabled, otherwise
  //                         is set to false.
  //   -vad_enabled        : is set to true if VAD is enabled, otherwise
  //                         is set to false.
  //   -vad_mode            : is set to the current aggressiveness of VAD.
  //
  // Return value:
  //   -1 if fails to retrieve the setting of DTX/VAD,
  //    0 if succeeded.
  //
  virtual WebRtc_Word32 VAD(bool& dtx_enabled, bool& vad_enabled,
                            ACMVADMode& vad_mode) const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 ReplaceInternalDTXWithWebRtc()
  // Used to replace codec internal DTX scheme with WebRtc. This is only
  // supported for G729, where this call replaces AnnexB with WebRtc DTX.
  //
  // Input:
  //   -use_webrtc_dtx     : if false (default) the codec built-in DTX/VAD
  //                         scheme is used, otherwise the internal DTX is
  //                         replaced with WebRtc DTX/VAD.
  //
  // Return value:
  //   -1 if failed to replace codec internal DTX with WebRtc,
  //    0 if succeeded.
  //
  virtual WebRtc_Word32 ReplaceInternalDTXWithWebRtc(
      const bool use_webrtc_dtx = false) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 IsInternalDTXReplacedWithWebRtc()
  // Get status if the codec internal DTX (when such exists) is replaced with
  // WebRtc DTX. This is only supported for G729.
  //
  // Output:
  //   -uses_webrtc_dtx    : is set to true if the codec internal DTX is
  //                         replaced with WebRtc DTX/VAD, otherwise it is set
  //                         to false.
  //
  // Return value:
  //   -1 if failed to determine if codec internal DTX is replaced with WebRtc,
  //    0 if succeeded.
  //
  virtual WebRtc_Word32 IsInternalDTXReplacedWithWebRtc(
      bool& uses_webrtc_dtx) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 RegisterVADCallback()
  // Call this method to register a callback function which is called
  // any time that ACM encounters an empty frame. That is a frame which is
  // recognized inactive. Depending on the codec WebRtc VAD or internal codec
  // VAD is employed to identify a frame as active/inactive.
  //
  // Input:
  //   -vad_callback        : pointer to a callback function.
  //
  // Return value:
  //   -1 if failed to register the callback function.
  //    0 if the callback function is registered successfully.
  //
  virtual WebRtc_Word32 RegisterVADCallback(ACMVADCallback* vad_callback) = 0;

  ///////////////////////////////////////////////////////////////////////////
  //   Receiver
  //

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 InitializeReceiver()
  // Any decoder-related state of ACM will be initialized to the
  // same state when ACM is created. This will not interrupt or
  // effect encoding functionality of ACM. ACM would lose all the
  // decoding-related settings by calling this function.
  // For instance, all registered codecs are deleted and have to be
  // registered again.
  //
  // Return value:
  //   -1 if failed to initialize,
  //    0 if succeeded.
  //
  virtual WebRtc_Word32 InitializeReceiver() = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 ResetDecoder()
  // This API resets the states of decoders. ACM will not lose any
  // decoder-related settings, such as registered codecs.
  //
  // Return value:
  //   -1 if failed to initialize,
  //    0 if succeeded.
  //
  virtual WebRtc_Word32 ResetDecoder() = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 ReceiveFrequency()
  // Get sampling frequency of the last received payload.
  //
  // Return value:
  //   non-negative the sampling frequency in Hertz.
  //   -1 if an error has occurred.
  //
  virtual WebRtc_Word32 ReceiveFrequency() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 PlayoutFrequency()
  // Get sampling frequency of audio played out.
  //
  // Return value:
  //   the sampling frequency in Hertz.
  //
  virtual WebRtc_Word32 PlayoutFrequency() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 RegisterReceiveCodec()
  // Register possible decoders, can be called multiple times for
  // codecs, CNG-NB, CNG-WB, CNG-SWB, AVT and RED.
  //
  // Input:
  //   -receive_codec      : parameters of the codec to be registered, c.f.
  //                         common_types.h for the definition of
  //                         CodecInst.
  //
  // Return value:
  //   -1 if failed to register the codec
  //    0 if the codec registered successfully.
  //
  virtual WebRtc_Word32 RegisterReceiveCodec(
      const CodecInst& receive_codec) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 UnregisterReceiveCodec()
  // Unregister the codec currently registered with a specific payload type
  // from the list of possible receive codecs.
  //
  // Input:
  //   -payload_type        : The number representing the payload type to
  //                         unregister.
  //
  // Output:
  //   -1 if fails to unregister.
  //    0 if the given codec is successfully unregistered.
  //
  virtual WebRtc_Word32 UnregisterReceiveCodec(
      const WebRtc_Word16 payload_type) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 ReceiveCodec()
  // Get the codec associated with last received payload.
  //
  // Output:
  //   -curr_receive_codec : parameters of the codec associated with the last
  //                         received payload, c.f. common_types.h for
  //                         the definition of CodecInst.
  //
  // Return value:
  //   -1 if failed to retrieve the codec,
  //    0 if the codec is successfully retrieved.
  //
  virtual WebRtc_Word32 ReceiveCodec(CodecInst& curr_receive_codec) const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 IncomingPacket()
  // Call this function to insert a parsed RTP packet into ACM.
  //
  // Inputs:
  //   -incoming_payload   : received payload.
  //   -payload_len_bytes  : the length of payload in bytes.
  //   -rtp_info           : the relevant information retrieved from RTP
  //                         header.
  //
  // Return value:
  //   -1 if failed to push in the payload
  //    0 if payload is successfully pushed in.
  //
  virtual WebRtc_Word32 IncomingPacket(const WebRtc_UWord8* incoming_payload,
                                       const WebRtc_Word32 payload_len_bytes,
                                       const WebRtcRTPHeader& rtp_info) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 IncomingPayload()
  // Call this API to push incoming payloads when there is no rtp-info.
  // The rtp-info will be created in ACM. One usage for this API is when
  // pre-encoded files are pushed in ACM
  //
  // Inputs:
  //   -incoming_payload   : received payload.
  //   -payload_len_byte   : the length, in bytes, of the received payload.
  //   -payload_type       : the payload-type. This specifies which codec has
  //                         to be used to decode the payload.
  //   -timestamp          : send timestamp of the payload. ACM starts with
  //                         a random value and increment it by the
  //                         packet-size, which is given when the codec in
  //                         question is registered by RegisterReceiveCodec().
  //                         Therefore, it is essential to have the timestamp
  //                         if the frame-size differ from the registered
  //                         value or if the incoming payload contains DTX
  //                         packets.
  //
  // Return value:
  //   -1 if failed to push in the payload
  //    0 if payload is successfully pushed in.
  //
  virtual WebRtc_Word32 IncomingPayload(const WebRtc_UWord8* incoming_payload,
                                        const WebRtc_Word32 payload_len_byte,
                                        const WebRtc_UWord8 payload_type,
                                        const WebRtc_UWord32 timestamp = 0) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 SetMinimumPlayoutDelay()
  // Set Minimum playout delay, used for lip-sync.
  //
  // Input:
  //   -time_ms            : minimum delay in milliseconds.
  //
  // Return value:
  //   -1 if failed to set the delay,
  //    0 if the minimum delay is set.
  //
  virtual WebRtc_Word32 SetMinimumPlayoutDelay(const WebRtc_Word32 time_ms) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 RegisterIncomingMessagesCallback()
  // Used by the module to deliver messages to the codec module/application
  // when a DTMF tone is detected, as well as when it stopped.
  //
  // Inputs:
  //   -in_message_callback: pointer to callback function which will be called
  //                         if DTMF is detected.
  //   -cpt                : enables CPT (Call Progress Tone) detection for the
  //                         specified country. c.f. definition of ACMCountries
  //                         in audio_coding_module_typedefs.h for valid
  //                         entries. The default value disables CPT
  //                         detection.
  //
  // Return value:
  //   -1 if the message callback could not be registered
  //    0 if registration is successful.
  //
  virtual WebRtc_Word32
      RegisterIncomingMessagesCallback(
          AudioCodingFeedback* in_message_callback,
          const ACMCountries cpt = ACMDisableCountryDetection) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 SetDtmfPlayoutStatus()
  // Configure DTMF playout, i.e. whether out-of-band
  // DTMF tones are played or not.
  //
  // Input:
  //   -enable             : if true to enable playout out-of-band DTMF tones,
  //                         false to disable.
  //
  // Return value:
  //   -1 if the method fails, e.g. DTMF playout is not supported.
  //    0 if the status is set successfully.
  //
  virtual WebRtc_Word32 SetDtmfPlayoutStatus(const bool enable) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // bool DtmfPlayoutStatus()
  // Get Dtmf playout status.
  //
  // Return value:
  //   true if out-of-band Dtmf tones are played,
  //   false if playout of Dtmf tones is disabled.
  //
  virtual bool DtmfPlayoutStatus() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 SetBackgroundNoiseMode()
  // Sets the mode of the background noise playout in an event of long
  // packet loss burst. For the valid modes see the declaration of
  // ACMBackgroundNoiseMode in audio_coding_module_typedefs.h.
  //
  // Input:
  //   -mode               : the mode for the background noise playout.
  //
  // Return value:
  //   -1 if failed to set the mode.
  //    0 if succeeded in setting the mode.
  //
  virtual WebRtc_Word32 SetBackgroundNoiseMode(
      const ACMBackgroundNoiseMode mode) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 BackgroundNoiseMode()
  // Call this method to get the mode of the background noise playout.
  // Playout of background noise is a result of a long packet loss burst.
  // See ACMBackgroundNoiseMode in audio_coding_module_typedefs.h for
  // possible modes.
  //
  // Output:
  //   -mode             : a reference to ACMBackgroundNoiseMode enumerator.
  //
  // Return value:
  //    0 if the output is a valid mode.
  //   -1 if ACM failed to output a valid mode.
  //
  virtual WebRtc_Word32 BackgroundNoiseMode(ACMBackgroundNoiseMode& mode) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 PlayoutTimestamp()
  // The send timestamp of an RTP packet is associated with the decoded
  // audio of the packet in question. This function returns the timestamp of
  // the latest audio obtained by calling PlayoutData10ms().
  //
  // Input:
  //   -timestamp          : a reference to a WebRtc_UWord32 to receive the
  //                         timestamp.
  // Return value:
  //    0 if the output is a correct timestamp.
  //   -1 if failed to output the correct timestamp.
  //
  //
  virtual WebRtc_Word32 PlayoutTimestamp(WebRtc_UWord32& timestamp) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 DecoderEstimatedBandwidth()
  // Get the estimate of the Bandwidth, in bits/second, based on the incoming
  // stream. This API is useful in one-way communication scenarios, where
  // the bandwidth information is sent in an out-of-band fashion.
  // Currently only supported if iSAC is registered as a receiver.
  //
  // Return value:
  //   >0 bandwidth in bits/second.
  //   -1 if failed to get a bandwidth estimate.
  //
  virtual WebRtc_Word32 DecoderEstimatedBandwidth() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 SetPlayoutMode()
  // Call this API to set the playout mode. Playout mode could be optimized
  // for i) voice, ii) FAX or iii) streaming. In Voice mode, NetEQ is
  // optimized to deliver highest audio quality while maintaining a minimum
  // delay. In FAX mode, NetEQ is optimized to have few delay changes as
  // possible and maintain a constant delay, perhaps large relative to voice
  // mode, to avoid PLC. In streaming mode, we tolerate a little more delay
  // to achieve better jitter robustness.
  //
  // Input:
  //   -mode               : playout mode. Possible inputs are:
  //                         "voice",
  //                         "fax" and
  //                         "streaming".
  //
  // Return value:
  //   -1 if failed to set the mode,
  //    0 if succeeding.
  //
  virtual WebRtc_Word32 SetPlayoutMode(const AudioPlayoutMode mode) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // AudioPlayoutMode PlayoutMode()
  // Get playout mode, i.e. whether it is speech, FAX or streaming. See
  // audio_coding_module_typedefs.h for definition of AudioPlayoutMode.
  //
  // Return value:
  //   voice:       is for voice output,
  //   fax:         a mode that is optimized for receiving FAX signals.
  //                In this mode NetEq tries to maintain a constant high
  //                delay to avoid PLC if possible.
  //   streaming:   a mode that is suitable for streaming. In this mode we
  //                accept longer delay to improve jitter robustness.
  //
  virtual AudioPlayoutMode PlayoutMode() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 PlayoutData10Ms(
  // Get 10 milliseconds of raw audio data for playout, at the given sampling
  // frequency. ACM will perform a resampling if required.
  //
  // Input:
  //   -desired_freq_hz    : the desired sampling frequency, in Hertz, of the
  //                         output audio. If set to -1, the function returns the
  //                         audio at the current sampling frequency.
  //
  // Output:
  //   -audio_frame        : output audio frame which contains raw audio data
  //                         and other relevant parameters, c.f.
  //                         module_common_types.h for the definition of
  //                         AudioFrame.
  //
  // Return value:
  //   -1 if the function fails,
  //    0 if the function succeeds.
  //
  virtual WebRtc_Word32
      PlayoutData10Ms(const WebRtc_Word32 desired_freq_hz,
                      AudioFrame &audio_frame) = 0;

  ///////////////////////////////////////////////////////////////////////////
  //   (CNG) Comfort Noise Generation
  //   Generate comfort noise when receiving DTX packets
  //

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word16 SetReceiveVADMode()
  // Configure VAD aggressiveness on the incoming stream.
  //
  // Input:
  //   -mode               : aggressiveness of the VAD on incoming stream.
  //                         See audio_coding_module_typedefs.h for the
  //                         definition of ACMVADMode, and possible
  //                         values for aggressiveness.
  //
  // Return value:
  //   -1 if fails to set the mode,
  //    0 if the mode is set successfully.
  //
  virtual WebRtc_Word16 SetReceiveVADMode(const ACMVADMode mode) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // ACMVADMode ReceiveVADMode()
  // Get VAD aggressiveness on the incoming stream.
  //
  // Return value:
  //   aggressiveness of VAD, running on the incoming stream. A more
  //   aggressive mode means more audio frames will be labeled as in-active.
  //   See audio_coding_module_typedefs.h for the definition of
  //   ACMVADMode.
  //
  virtual ACMVADMode ReceiveVADMode() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  //   Codec specific
  //

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 SetISACMaxRate()
  // Set the maximum instantaneous rate of iSAC. For a payload of B bits
  // with a frame-size of T sec the instantaneous rate is B/T bits per
  // second. Therefore, (B/T < |max_rate_bps|) and
  // (B < |max_payload_len_bytes| * 8) are always satisfied for iSAC payloads,
  // c.f SetISACMaxPayloadSize().
  //
  // Input:
  //   -max_rate_bps       : maximum instantaneous bit-rate given in bits/sec.
  //
  // Return value:
  //   -1 if failed to set the maximum rate.
  //    0 if the maximum rate is set successfully.
  //
  virtual WebRtc_Word32 SetISACMaxRate(
      const WebRtc_UWord32 max_rate_bps) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 SetISACMaxPayloadSize()
  // Set the maximum payload size of iSAC packets. No iSAC payload,
  // regardless of its frame-size, may exceed the given limit. For
  // an iSAC payload of size B bits and frame-size T seconds we have;
  // (B < |max_payload_len_bytes| * 8) and (B/T < |max_rate_bps|), c.f.
  // SetISACMaxRate().
  //
  // Input:
  //   -max_payload_len_bytes : maximum payload size in bytes.
  //
  // Return value:
  //   -1 if failed to set the maximum  payload-size.
  //    0 if the given length is set successfully.
  //
  virtual WebRtc_Word32 SetISACMaxPayloadSize(
      const WebRtc_UWord16 max_payload_len_bytes) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 ConfigISACBandwidthEstimator()
  // Call this function to configure the bandwidth estimator of ISAC.
  // During the adaptation of bit-rate, iSAC automatically adjusts the
  // frame-size (either 30 or 60 ms) to save on RTP header. The initial
  // frame-size can be specified by the first argument. The configuration also
  // regards the initial estimate of bandwidths. The estimator starts from
  // this point and converges to the actual bottleneck. This is given by the
  // second parameter. Furthermore, it is also possible to control the
  // adaptation of frame-size. This is specified by the last parameter.
  //
  // Input:
  //   -init_frame_size_ms : initial frame-size in milliseconds. For iSAC-wb
  //                         30 ms and 60 ms (default) are acceptable values,
  //                         and for iSAC-swb 30 ms is the only acceptable
  //                         value. Zero indicates default value.
  //   -init_rate_bps      : initial estimate of the bandwidth. Values
  //                         between 10000 and 58000 are acceptable.
  //   -enforce_srame_size : if true, the frame-size will not be adapted.
  //
  // Return value:
  //   -1 if failed to configure the bandwidth estimator,
  //    0 if the configuration was successfully applied.
  //
  virtual WebRtc_Word32 ConfigISACBandwidthEstimator(
      const WebRtc_UWord8 init_frame_size_ms,
      const WebRtc_UWord16 init_rate_bps,
      const bool enforce_frame_size = false) = 0;

  ///////////////////////////////////////////////////////////////////////////
  //   statistics
  //

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32  NetworkStatistics()
  // Get network statistics.
  //
  // Input:
  //   -network_statistics : a structure that contains network statistics.
  //
  // Return value:
  //   -1 if failed to set the network statistics,
  //    0 if statistics are set successfully.
  //
  virtual WebRtc_Word32 NetworkStatistics(
      ACMNetworkStatistics& network_statistics) const = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_INTERFACE_AUDIO_CODING_MODULE_H
