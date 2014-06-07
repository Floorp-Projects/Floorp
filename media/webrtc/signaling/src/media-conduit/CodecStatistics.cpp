/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CodecStatistics.h"

#include "CSFLog.h"

using namespace mozilla;
using namespace webrtc;

// use the same tag as VideoConduit
static const char* logTag ="WebrtcVideoSessionConduit";

VideoCodecStatistics::VideoCodecStatistics(int channel,
                                           ViECodec* codec,
                                           bool encoder) :
  mChannel(channel),
  mSentRawFrames(0),
  mPtrViECodec(codec),
  mEncoderDroppedFrames(0),
  mDecoderDiscardedPackets(0),
  mEncoderMode(encoder)
{
  MOZ_ASSERT(mPtrViECodec);
  if (mEncoderMode) {
    mPtrViECodec->RegisterEncoderObserver(mChannel, *this);
  } else {
    mPtrViECodec->RegisterDecoderObserver(mChannel, *this);
  }
}

VideoCodecStatistics::~VideoCodecStatistics()
{
  if (mEncoderMode) {
    mPtrViECodec->DeregisterEncoderObserver(mChannel);
  } else {
    mPtrViECodec->DeregisterDecoderObserver(mChannel);
  }
}

void VideoCodecStatistics::OutgoingRate(const int video_channel,
                                        const uint32_t framerate,
                                        const uint32_t bitrate)
{
  unsigned int keyFrames, deltaFrames;
  mPtrViECodec->GetSendCodecStatistics(video_channel, keyFrames, deltaFrames);
  uint32_t dropped = mSentRawFrames - (keyFrames + deltaFrames);
  CSFLogDebug(logTag,
              "encoder statistics - framerate: %u, bitrate: %u, dropped frames: %u",
              framerate, bitrate, dropped);
  mEncoderBitRate.Push(bitrate);
  mEncoderFps.Push(framerate);
  mEncoderDroppedFrames += dropped;
}

void VideoCodecStatistics::IncomingCodecChanged(const int video_channel,
                                                const VideoCodec& video_codec)
{
  CSFLogDebug(logTag,
              "channel %d change codec to \"%s\" ",
              video_channel, video_codec.plName);
}

void VideoCodecStatistics::IncomingRate(const int video_channel,
                                        const unsigned int framerate,
                                        const unsigned int bitrate)
{
  unsigned int discarded = mPtrViECodec->GetDiscardedPackets(video_channel);
  CSFLogDebug(logTag,
      "decoder statistics - framerate: %u, bitrate: %u, discarded packets %u",
      framerate, bitrate, discarded);
  mDecoderBitRate.Push(bitrate);
  mDecoderFps.Push(framerate);
  mDecoderDiscardedPackets += discarded;
}

void VideoCodecStatistics::SentFrame()
{
  mSentRawFrames++;
}

void VideoCodecStatistics::Dump()
{
  Dump(mEncoderBitRate, "encoder bitrate");
  Dump(mEncoderFps, "encoder fps");
  Dump(mDecoderBitRate, "decoder bitrate");
  Dump(mDecoderFps, "decoder fps");
}

void VideoCodecStatistics::Dump(RunningStat& s, const char *name)
{
  CSFLogDebug(logTag,
              "%s, mean: %f, variance: %f, standard deviation: %f",
              name, s.Mean(), s.Variance(), s.StandardDeviation());
}
