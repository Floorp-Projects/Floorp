/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_CODEC_DATABASE_H_
#define WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_CODEC_DATABASE_H_

#include <map>

#include "modules/video_coding/codecs/interface/video_codec_interface.h"
#include "modules/video_coding/main/interface/video_coding.h"
#include "modules/video_coding/main/source/generic_decoder.h"
#include "modules/video_coding/main/source/generic_encoder.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "typedefs.h"

namespace webrtc {

enum VCMCodecDBProperties {
  kDefaultPayloadSize = 1440
};

struct VCMDecoderMapItem {
 public:
  VCMDecoderMapItem(VideoCodec* settings,
                    int number_of_cores,
                    bool require_key_frame);

  scoped_ptr<VideoCodec> settings;
  int number_of_cores;
  bool require_key_frame;
};

struct VCMExtDecoderMapItem {
 public:
  VCMExtDecoderMapItem(VideoDecoder* external_decoder_instance,
                       uint8_t payload_type,
                       bool internal_render_timing);

  uint8_t payload_type;
  VideoDecoder* external_decoder_instance;
  bool internal_render_timing;
};

class VCMCodecDataBase {
 public:
  explicit VCMCodecDataBase(int id);
  ~VCMCodecDataBase();

  // Sender Side
  // Returns the number of supported codecs (or -1 in case of error).
  static int NumberOfCodecs();

  // Returns the default settings for the codec with id |list_id|.
  static bool Codec(int list_id, VideoCodec* settings);

  // Returns the default settings for the codec with type |codec_type|.
  static bool Codec(VideoCodecType codec_type, VideoCodec* settings);

  void ResetSender();

  // Sets the sender side codec and initiates the desired codec given the
  // VideoCodec struct.
  // Returns true if the codec was successfully registered, false otherwise.
  bool SetSendCodec(const VideoCodec* send_codec,
                    int number_of_cores,
                    int max_payload_size,
                    VCMEncodedFrameCallback* encoded_frame_callback);

  // Gets the current send codec. Relevant for internal codecs only.
  // Returns true if there is a send codec, false otherwise.
  bool SendCodec(VideoCodec* current_send_codec) const;

  // Gets current send side codec type. Relevant for internal codecs only.
  // Returns kVideoCodecUnknown if there is no send codec.
  VideoCodecType SendCodec() const;

  // Registers and initializes an external encoder object.
  // |internal_source| should be set to true if the codec has an internal
  // video source and doesn't need the user to provide it with frames via
  // the Encode() method.
  void RegisterExternalEncoder(VideoEncoder* external_encoder,
                               uint8_t payload_type,
                               bool internal_source);

  // Deregisters an external encoder. Returns true if the encoder was
  // found and deregistered, false otherwise. |was_send_codec| is set to true
  // if the external encoder was the send codec before being deregistered.
  bool DeregisterExternalEncoder(uint8_t payload_type, bool* was_send_codec);

  VCMGenericEncoder* GetEncoder();

  bool SetPeriodicKeyFrames(bool enable);

  // Receiver Side
  void ResetReceiver();

  // Deregisters an external decoder object specified by |payload_type|.
  bool DeregisterExternalDecoder(uint8_t payload_type);

  // Registers an external decoder object to the payload type |payload_type|.
  // |internal_render_timing| is set to true if the |external_decoder| has
  // built in rendering which is able to obey the render timestamps of the
  // encoded frames.
  bool RegisterExternalDecoder(VideoDecoder* external_decoder,
                               uint8_t payload_type,
                               bool internal_render_timing);

  bool DecoderRegistered() const;

  bool RegisterReceiveCodec(const VideoCodec* receive_codec,
                            int number_of_cores,
                            bool require_key_frame);

  bool DeregisterReceiveCodec(uint8_t payload_type);

  // Get current receive side codec. Relevant for internal codecs only.
  bool ReceiveCodec(VideoCodec* current_receive_codec) const;

  // Get current receive side codec type. Relevant for internal codecs only.
  VideoCodecType ReceiveCodec() const;

  // Returns a decoder specified by |payload_type|. The decoded frame callback
  // of the encoder is set to |decoded_frame_callback|. If no such decoder
  // already exists an instance will be created and initialized.
  // NULL is returned if no encoder with the specified payload type was found
  // and the function failed to create one.
  VCMGenericDecoder* GetDecoder(
      uint8_t payload_type, VCMDecodedFrameCallback* decoded_frame_callback);

  // Returns a deep copy of the currently active decoder.
  VCMGenericDecoder* CreateDecoderCopy() const;

  // Deletes the memory of the decoder instance |decoder|. Used to delete
  // deep copies returned by CreateDecoderCopy().
  void ReleaseDecoder(VCMGenericDecoder* decoder) const;

  // Creates a deep copy of |decoder| and replaces the currently used decoder
  // with it.
  void CopyDecoder(const VCMGenericDecoder& decoder);

  // Returns true if the currently active decoder supports render scheduling,
  // that is, it is able to render frames according to the render timestamp of
  // the encoded frames.
  bool SupportsRenderScheduling() const;

 private:
  typedef std::map<uint8_t, VCMDecoderMapItem*> DecoderMap;
  typedef std::map<uint8_t, VCMExtDecoderMapItem*> ExternalDecoderMap;

  VCMGenericDecoder* CreateAndInitDecoder(uint8_t payload_type,
                                          VideoCodec* new_codec,
                                          bool* external) const;

  // Determines whether a new codec has to be created or not.
  // Checks every setting apart from maxFramerate and startBitrate.
  bool RequiresEncoderReset(const VideoCodec& send_codec);
  // Create an internal encoder given a codec type.
  VCMGenericEncoder* CreateEncoder(const VideoCodecType type) const;

  void DeleteEncoder();

  // Create an internal Decoder given a codec type
  VCMGenericDecoder* CreateDecoder(VideoCodecType type) const;

  const VCMDecoderMapItem* FindDecoderItem(uint8_t payload_type) const;

  const VCMExtDecoderMapItem* FindExternalDecoderItem(
      uint8_t payload_type) const;

  int id_;
  int number_of_cores_;
  int max_payload_size_;
  bool periodic_key_frames_;
  bool pending_encoder_reset_;
  bool current_enc_is_external_;
  VideoCodec send_codec_;
  VideoCodec receive_codec_;
  uint8_t external_payload_type_;
  VideoEncoder* external_encoder_;
  bool internal_source_;
  VCMGenericEncoder* ptr_encoder_;
  VCMGenericDecoder* ptr_decoder_;
  bool current_dec_is_external_;
  DecoderMap dec_map_;
  ExternalDecoderMap dec_external_map_;
};  // VCMCodecDataBase

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_CODEC_DATABASE_H_
