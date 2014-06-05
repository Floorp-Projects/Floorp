/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CODEC_STATISTICS_H_
#define CODEC_STATISTICS_H_
#include <math.h>

#include "nsTArray.h"
#include "nsISupportsImpl.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "MediaEngineWrapper.h"
#include "RunningStat.h"

namespace mozilla {

// Statistics-gathering observer for Video Encoder and Decoder

class VideoCodecStatistics : public webrtc::ViEEncoderObserver
                           , public webrtc::ViEDecoderObserver
{
public:
  VideoCodecStatistics(int channel, webrtc::ViECodec* vieCodec);
  ~VideoCodecStatistics();

  void SentFrame();
  virtual void OutgoingRate(const int video_channel,
    const unsigned int framerate, const unsigned int bitrate) MOZ_OVERRIDE;

  virtual void IncomingCodecChanged(const int video_channel,
    const webrtc::VideoCodec& video_codec) MOZ_OVERRIDE;

  virtual void IncomingRate(const int video_channel,
                            const unsigned int framerate,
                            const unsigned int bitrate) MOZ_OVERRIDE;

  virtual void RequestNewKeyFrame(const int video_channel) MOZ_OVERRIDE {};

  virtual void SuspendChange(int video_channel, bool is_suspended) MOZ_OVERRIDE {};
  virtual void DecoderTiming(int decode_ms,
                             int max_decode_ms,
                             int current_delay_ms,
                             int target_delay_ms,
                             int jitter_buffer_ms,
                             int min_playout_delay_ms,
                             int render_delay_ms) MOZ_OVERRIDE {}
  void Dump();
private:
  void Dump(RunningStat& s, const char *name);

  int mChannel;
  uint32_t mSentRawFrames;
  ScopedCustomReleasePtr<webrtc::ViECodec> mPtrViECodec; // back-pointer

  RunningStat mEncoderBitRate;
  RunningStat mEncoderFps;
  uint32_t mEncoderDroppedFrames;
  RunningStat mDecoderBitRate;
  RunningStat mDecoderFps;
  uint32_t mDecoderDiscardedPackets;
};
}

#endif //CODEC_STATISTICS_H_
