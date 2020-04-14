/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "VideoStreamFactory.h"

#include "CSFLog.h"
#include "nsThreadUtils.h"
#include "VideoConduit.h"

namespace mozilla {

#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG "WebrtcVideoSessionConduit"

#define MB_OF(w, h) \
  ((unsigned int)((((w + 15) >> 4)) * ((unsigned int)((h + 15) >> 4))))
// For now, try to set the max rates well above the knee in the curve.
// Chosen somewhat arbitrarily; it's hard to find good data oriented for
// realtime interactive/talking-head recording.  These rates assume
// 30fps.

// XXX Populate this based on a pref (which we should consider sorting because
// people won't assume they need to).
static VideoStreamFactory::ResolutionAndBitrateLimits
    kResolutionAndBitrateLimits[] = {
        // clang-format off
  {MB_OF(1920, 1200), KBPS(1500), KBPS(2000), KBPS(10000)}, // >HD (3K, 4K, etc)
  {MB_OF(1280, 720), KBPS(1200), KBPS(1500), KBPS(5000)}, // HD ~1080-1200
  {MB_OF(800, 480), KBPS(600), KBPS(800), KBPS(2500)}, // HD ~720
  {MB_OF(480, 270), KBPS(150), KBPS(500), KBPS(2000)}, // WVGA
  {tl::Max<MB_OF(400, 240), MB_OF(352, 288)>::value, KBPS(125), KBPS(300), KBPS(1300)}, // VGA
  {MB_OF(176, 144), KBPS(100), KBPS(150), KBPS(500)}, // WQVGA, CIF
  {0 , KBPS(40), KBPS(80), KBPS(250)} // QCIF and below
        // clang-format on
};

static VideoStreamFactory::ResolutionAndBitrateLimits GetLimitsFor(
    unsigned int aWidth, unsigned int aHeight, int aCapBps = 0) {
  // max bandwidth should be proportional (not linearly!) to resolution, and
  // proportional (perhaps linearly, or close) to current frame rate.
  int fs = MB_OF(aWidth, aHeight);

  for (const auto& resAndLimits : kResolutionAndBitrateLimits) {
    if (fs > resAndLimits.resolution_in_mb &&
        // pick the highest range where at least start rate is within cap
        // (or if we're at the end of the array).
        (aCapBps == 0 || resAndLimits.start_bitrate_bps <= aCapBps ||
         resAndLimits.resolution_in_mb == 0)) {
      return resAndLimits;
    }
  }

  MOZ_CRASH("Loop should have handled fallback");
}

/**
 * Function to set the encoding bitrate limits based on incoming frame size and
 * rate
 * @param width, height: dimensions of the frame
 * @param min: minimum bitrate in bps
 * @param start: bitrate in bps that the encoder should start with
 * @param cap: user-enforced max bitrate, or 0
 * @param pref_cap: cap enforced by prefs
 * @param negotiated_cap: cap negotiated through SDP
 * @param aVideoStream stream to apply bitrates to
 */
static void SelectBitrates(unsigned short width, unsigned short height, int min,
                           int start, int cap, int pref_cap, int negotiated_cap,
                           webrtc::VideoStream& aVideoStream) {
  int& out_min = aVideoStream.min_bitrate_bps;
  int& out_start = aVideoStream.target_bitrate_bps;
  int& out_max = aVideoStream.max_bitrate_bps;

  VideoStreamFactory::ResolutionAndBitrateLimits resAndLimits =
      GetLimitsFor(width, height);
  out_min = MinIgnoreZero(resAndLimits.min_bitrate_bps, cap);
  out_start = MinIgnoreZero(resAndLimits.start_bitrate_bps, cap);
  out_max = MinIgnoreZero(resAndLimits.max_bitrate_bps, cap);

  // Note: negotiated_cap is the max transport bitrate - it applies to
  // a single codec encoding, but should also apply to the sum of all
  // simulcast layers in this encoding! So sum(layers.maxBitrate) <=
  // negotiated_cap
  // Note that out_max already has had pref_cap applied to it
  out_max = MinIgnoreZero(negotiated_cap, out_max);
  out_min = std::min(out_min, out_max);
  out_start = std::min(out_start, out_max);

  if (min && min > out_min) {
    out_min = min;
  }
  // If we try to set a minimum bitrate that is too low, ViE will reject it.
  out_min = std::max(kViEMinCodecBitrate_bps, out_min);
  out_max = std::max(kViEMinCodecBitrate_bps, out_max);
  if (start && start > out_start) {
    out_start = start;
  }

  // Ensure that min <= start <= max
  if (out_min > out_max) {
    out_min = out_max;
  }
  out_start = std::min(out_max, std::max(out_start, out_min));

  MOZ_ASSERT(pref_cap == 0 || out_max <= pref_cap);
}

void VideoStreamFactory::SetCodecMode(webrtc::VideoCodecMode aCodecMode) {
  MOZ_ASSERT(NS_IsMainThread());
  mCodecMode = aCodecMode;
}

void VideoStreamFactory::SetSendingFramerate(unsigned int aSendingFramerate) {
  MOZ_ASSERT(NS_IsMainThread());
  mSendingFramerate = aSendingFramerate;
}

std::vector<webrtc::VideoStream> VideoStreamFactory::CreateEncoderStreams(
    int width, int height, const webrtc::VideoEncoderConfig& config) {
  // We only allow one layer when screensharing
  const size_t streamCount =
      mCodecMode == webrtc::VideoCodecMode::kScreensharing
          ? 1
          : config.number_of_streams;

  MOZ_RELEASE_ASSERT(streamCount >= 1, "Should request at least one stream");

  std::vector<webrtc::VideoStream> streams;
  streams.reserve(streamCount);

  // We assume that the first stream is the full-resolution stream.

  // This ensures all simulcast layers will be of the same aspect ratio as the
  // input.
  mSimulcastAdapter->OnOutputFormatRequest(
      cricket::VideoFormat(width, height, 0, 0));

  for (int idx = streamCount - 1; idx >= 0; --idx) {
    webrtc::VideoStream video_stream;
    auto& encoding = mCodecConfig.mEncodings[idx];
    MOZ_ASSERT(encoding.constraints.scaleDownBy >= 1.0);

    // All streams' dimensions must retain the aspect ratio of the input stream.
    // Note that the first stream might already have been scaled by us.
    // Webrtc.org doesn't know this, so we have to adjust lower layers manually.
    int unusedCropWidth, unusedCropHeight, outWidth, outHeight;
    if (idx == 0) {
      // This is the highest-resolution stream. We avoid calling
      // AdaptFrameResolution on this because precision errors in VideoAdapter
      // can cause the out-resolution to be an odd pixel smaller than the
      // source (1920x1419 has caused this). We shortcut this instead.
      outWidth = width;
      outHeight = height;
    } else {
      float effectiveScaleDownBy =
          encoding.constraints.scaleDownBy /
          mCodecConfig.mEncodings[0].constraints.scaleDownBy;
      MOZ_ASSERT(effectiveScaleDownBy >= 1.0);
      mSimulcastAdapter->OnScaleResolutionBy(
          effectiveScaleDownBy > 1.0
              ? rtc::Optional<float>(effectiveScaleDownBy)
              : rtc::Optional<float>());
      bool rv = mSimulcastAdapter->AdaptFrameResolution(
          width, height,
          0,  // Ok, since we don't request an output format with an interval
          &unusedCropWidth, &unusedCropHeight, &outWidth, &outHeight);

      if (!rv) {
        // The only thing that can make AdaptFrameResolution fail in this case
        // is if this layer is scaled so far down that it has less than one
        // pixel.
        outWidth = 0;
        outHeight = 0;
      }
    }

    if (outWidth == 0 || outHeight == 0) {
      CSFLogInfo(LOGTAG,
                 "%s Stream with RID %s ignored because of no resolution.",
                 __FUNCTION__, encoding.rid.c_str());
      continue;
    }

    MOZ_ASSERT(outWidth > 0);
    MOZ_ASSERT(outHeight > 0);
    video_stream.width = outWidth;
    video_stream.height = outHeight;

    CSFLogInfo(LOGTAG, "%s Input frame %ux%u, RID %s scaling to %zux%zu",
               __FUNCTION__, width, height, encoding.rid.c_str(),
               video_stream.width, video_stream.height);

    if (video_stream.width * height != width * video_stream.height) {
      CSFLogInfo(LOGTAG,
                 "%s Stream with RID %s ignored because of bad aspect ratio.",
                 __FUNCTION__, encoding.rid.c_str());
      continue;
    }

    // We want to ensure this picks up the current framerate, so indirect
    video_stream.max_framerate = mSendingFramerate;

    SelectBitrates(video_stream.width, video_stream.height, mMinBitrate,
                   mStartBitrate, encoding.constraints.maxBr, mPrefMaxBitrate,
                   mNegotiatedMaxBitrate, video_stream);

    video_stream.max_qp = kQpMax;
    video_stream.SetRid(encoding.rid);

    // leave vector temporal_layer_thresholds_bps empty for non-simulcast
    video_stream.temporal_layer_thresholds_bps.clear();
    if (streamCount > 1) {
      // XXX Note: in simulcast.cc in upstream code, the array value is
      // 3(-1) for all streams, though it's in an array, except for screencasts,
      // which use 1 (i.e 2 layers).

      // Oddly, though this is a 'bps' array, nothing really looks at the
      // values for normal video, just the size of the array to know the
      // number of temporal layers.
      // For VideoEncoderConfig::ContentType::kScreen, though, in
      // video_codec_initializer.cc it uses [0] to set the target bitrate
      // for the screenshare.
      if (mCodecMode == webrtc::VideoCodecMode::kScreensharing) {
        video_stream.temporal_layer_thresholds_bps.push_back(
            video_stream.target_bitrate_bps);
      } else {
        video_stream.temporal_layer_thresholds_bps.resize(2);
      }
      // XXX Bug 1390215 investigate using more of
      // simulcast.cc:GetSimulcastConfig() or our own algorithm to replace it
    }

    if (mCodecConfig.mName == "H264") {
      if (mCodecConfig.mEncodingConstraints.maxMbps > 0) {
        // Not supported yet!
        CSFLogError(LOGTAG, "%s H.264 max_mbps not supported yet",
                    __FUNCTION__);
      }
    }
    streams.push_back(video_stream);
  }

  MOZ_RELEASE_ASSERT(streams.size(), "Should configure at least one stream");
  return streams;
}

}  // namespace mozilla
