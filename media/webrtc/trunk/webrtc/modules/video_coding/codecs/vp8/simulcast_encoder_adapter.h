/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SIMULCAST_ENCODER_ADAPTER_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SIMULCAST_ENCODER_ADAPTER_H_

#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"

namespace webrtc {

class VideoEncoderFactory {
 public:
  virtual VideoEncoder* Create() = 0;
  virtual void Destroy(VideoEncoder* encoder) = 0;
  virtual ~VideoEncoderFactory() {}
};

// SimulcastEncoderAdapter implements simulcast support by creating multiple
// webrtc::VideoEncoder instances with the given VideoEncoderFactory.
// All the public interfaces are expected to be called from the same thread,
// e.g the encoder thread.
class SimulcastEncoderAdapter : public VP8Encoder,
                                public EncodedImageCallback {
 public:
  explicit SimulcastEncoderAdapter(VideoEncoderFactory* factory);
  virtual ~SimulcastEncoderAdapter();

  // Implements VideoEncoder
  int Release() override;
  int InitEncode(const VideoCodec* inst,
                 int number_of_cores,
                 size_t max_payload_size) override;
  int Encode(const I420VideoFrame& input_image,
             const CodecSpecificInfo* codec_specific_info,
             const std::vector<VideoFrameType>* frame_types) override;
  int RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override;
  int SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;
  int SetRates(uint32_t new_bitrate_kbit, uint32_t new_framerate) override;

  // Implements EncodedImageCallback
  int32_t Encoded(const EncodedImage& encodedImage,
                  const CodecSpecificInfo* codecSpecificInfo = NULL,
                  const RTPFragmentationHeader* fragmentation = NULL) override;

 private:
  struct StreamInfo {
    StreamInfo()
        : encoder(NULL), width(0), height(0),
          key_frame_request(false), send_stream(true) {}
    StreamInfo(VideoEncoder* encoder,
               unsigned short width,
               unsigned short height,
               bool send_stream)
        : encoder(encoder),
          width(width),
          height(height),
          key_frame_request(false),
          send_stream(send_stream) {}
    // Deleted by SimulcastEncoderAdapter::Release().
    VideoEncoder* encoder;
    unsigned short width;
    unsigned short height;
    bool key_frame_request;
    bool send_stream;
  };

  // Get the stream bitrate, for the stream |stream_idx|, given the bitrate
  // |new_bitrate_kbit|. The function also returns whether there's enough
  // bandwidth to send this stream via |send_stream|.
  uint32_t GetStreamBitrate(int stream_idx,
                            uint32_t new_bitrate_kbit,
                            bool* send_stream) const;

  // Populate the codec settings for each stream.
  void PopulateStreamCodec(const webrtc::VideoCodec* inst,
                           int stream_index,
                           bool highest_resolution_stream,
                           webrtc::VideoCodec* stream_codec,
                           bool* send_stream);

  // Get the stream index according to |encodedImage|.
  size_t GetStreamIndex(const EncodedImage& encodedImage);

  bool Initialized() const;

  rtc::scoped_ptr<VideoEncoderFactory> factory_;
  rtc::scoped_ptr<Config> screensharing_extra_options_;
  VideoCodec codec_;
  std::vector<StreamInfo> streaminfos_;
  EncodedImageCallback* encoded_complete_callback_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SIMULCAST_ENCODER_ADAPTER_H_

