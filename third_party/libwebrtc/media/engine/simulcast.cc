/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/simulcast.h"

#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <string>

#include "absl/types/optional.h"
#include "api/video/video_codec_constants.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/min_video_bitrate_experiment.h"
#include "rtc_base/experiments/normalize_simulcast_size_experiment.h"
#include "rtc_base/experiments/rate_control_settings.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace cricket {

namespace {

constexpr webrtc::DataRate Interpolate(const webrtc::DataRate& a,
                                       const webrtc::DataRate& b,
                                       float rate) {
  return a * (1.0 - rate) + b * rate;
}

constexpr char kUseLegacySimulcastLayerLimitFieldTrial[] =
    "WebRTC-LegacySimulcastLayerLimit";

// Limits for legacy conference screensharing mode. Currently used for the
// lower of the two simulcast streams.
constexpr webrtc::DataRate kScreenshareDefaultTl0Bitrate =
    webrtc::DataRate::KilobitsPerSec(200);
constexpr webrtc::DataRate kScreenshareDefaultTl1Bitrate =
    webrtc::DataRate::KilobitsPerSec(1000);

// Min/max bitrate for the higher one of the two simulcast stream used for
// screen content.
constexpr webrtc::DataRate kScreenshareHighStreamMinBitrate =
    webrtc::DataRate::KilobitsPerSec(600);
constexpr webrtc::DataRate kScreenshareHighStreamMaxBitrate =
    webrtc::DataRate::KilobitsPerSec(1250);

}  // namespace

struct SimulcastFormat {
  int width;
  int height;
  // The maximum number of simulcast layers can be used for
  // resolutions at |widthxheigh| for legacy applications.
  size_t max_layers;
  // The maximum bitrate for encoding stream at |widthxheight|, when we are
  // not sending the next higher spatial stream.
  webrtc::DataRate max_bitrate;
  // The target bitrate for encoding stream at |widthxheight|, when this layer
  // is not the highest layer (i.e., when we are sending another higher spatial
  // stream).
  webrtc::DataRate target_bitrate;
  // The minimum bitrate needed for encoding stream at |widthxheight|.
  webrtc::DataRate min_bitrate;
};

// These tables describe from which resolution we can use how many
// simulcast layers at what bitrates (maximum, target, and minimum).
// Important!! Keep this table from high resolution to low resolution.
constexpr const SimulcastFormat kSimulcastFormats[] = {
    {1920, 1080, 3, webrtc::DataRate::KilobitsPerSec(5000),
     webrtc::DataRate::KilobitsPerSec(4000),
     webrtc::DataRate::KilobitsPerSec(800)},
    {1280, 720, 3, webrtc::DataRate::KilobitsPerSec(2500),
     webrtc::DataRate::KilobitsPerSec(2500),
     webrtc::DataRate::KilobitsPerSec(600)},
    {960, 540, 3, webrtc::DataRate::KilobitsPerSec(1200),
     webrtc::DataRate::KilobitsPerSec(1200),
     webrtc::DataRate::KilobitsPerSec(350)},
    {640, 360, 2, webrtc::DataRate::KilobitsPerSec(700),
     webrtc::DataRate::KilobitsPerSec(500),
     webrtc::DataRate::KilobitsPerSec(150)},
    {480, 270, 2, webrtc::DataRate::KilobitsPerSec(450),
     webrtc::DataRate::KilobitsPerSec(350),
     webrtc::DataRate::KilobitsPerSec(150)},
    {320, 180, 1, webrtc::DataRate::KilobitsPerSec(200),
     webrtc::DataRate::KilobitsPerSec(150),
     webrtc::DataRate::KilobitsPerSec(30)},
    {0, 0, 1, webrtc::DataRate::KilobitsPerSec(200),
     webrtc::DataRate::KilobitsPerSec(150),
     webrtc::DataRate::KilobitsPerSec(30)}};

const int kMaxScreenshareSimulcastLayers = 2;

// Multiway: Number of temporal layers for each simulcast stream.
int DefaultNumberOfTemporalLayers(int simulcast_id, bool screenshare) {
  RTC_CHECK_GE(simulcast_id, 0);
  RTC_CHECK_LT(simulcast_id, webrtc::kMaxSimulcastStreams);

  const int kDefaultNumTemporalLayers = 3;
  const int kDefaultNumScreenshareTemporalLayers = 2;
  int default_num_temporal_layers = screenshare
                                        ? kDefaultNumScreenshareTemporalLayers
                                        : kDefaultNumTemporalLayers;

  const std::string group_name =
      screenshare ? webrtc::field_trial::FindFullName(
                        "WebRTC-VP8ScreenshareTemporalLayers")
                  : webrtc::field_trial::FindFullName(
                        "WebRTC-VP8ConferenceTemporalLayers");
  if (group_name.empty())
    return default_num_temporal_layers;

  int num_temporal_layers = default_num_temporal_layers;
  if (sscanf(group_name.c_str(), "%d", &num_temporal_layers) == 1 &&
      num_temporal_layers > 0 &&
      num_temporal_layers <= webrtc::kMaxTemporalStreams) {
    return num_temporal_layers;
  }

  RTC_LOG(LS_WARNING) << "Attempt to set number of temporal layers to "
                         "incorrect value: "
                      << group_name;

  return default_num_temporal_layers;
}

int FindSimulcastFormatIndex(int width, int height) {
  RTC_DCHECK_GE(width, 0);
  RTC_DCHECK_GE(height, 0);
  for (uint32_t i = 0; i < arraysize(kSimulcastFormats); ++i) {
    if (width * height >=
        kSimulcastFormats[i].width * kSimulcastFormats[i].height) {
      return i;
    }
  }
  RTC_NOTREACHED();
  return -1;
}

// Round size to nearest simulcast-friendly size.
// Simulcast stream width and height must both be dividable by
// |2 ^ (simulcast_layers - 1)|.
int NormalizeSimulcastSize(int size, size_t simulcast_layers) {
  int base2_exponent = static_cast<int>(simulcast_layers) - 1;
  const absl::optional<int> experimental_base2_exponent =
      webrtc::NormalizeSimulcastSizeExperiment::GetBase2Exponent();
  if (experimental_base2_exponent &&
      (size > (1 << *experimental_base2_exponent))) {
    base2_exponent = *experimental_base2_exponent;
  }
  return ((size >> base2_exponent) << base2_exponent);
}

SimulcastFormat InterpolateSimulcastFormat(int width, int height) {
  const int index = FindSimulcastFormatIndex(width, height);
  if (index == 0)
    return kSimulcastFormats[index];
  const int total_pixels_up =
      kSimulcastFormats[index - 1].width * kSimulcastFormats[index - 1].height;
  const int total_pixels_down =
      kSimulcastFormats[index].width * kSimulcastFormats[index].height;
  const int total_pixels = width * height;
  const float rate = (total_pixels_up - total_pixels) /
                     static_cast<float>(total_pixels_up - total_pixels_down);

  size_t max_layers = kSimulcastFormats[index].max_layers;
  webrtc::DataRate max_bitrate =
      Interpolate(kSimulcastFormats[index - 1].max_bitrate,
                  kSimulcastFormats[index].max_bitrate, rate);
  webrtc::DataRate target_bitrate =
      Interpolate(kSimulcastFormats[index - 1].target_bitrate,
                  kSimulcastFormats[index].target_bitrate, rate);
  webrtc::DataRate min_bitrate =
      Interpolate(kSimulcastFormats[index - 1].min_bitrate,
                  kSimulcastFormats[index].min_bitrate, rate);

  return {width, height, max_layers, max_bitrate, target_bitrate, min_bitrate};
}

webrtc::DataRate FindSimulcastMaxBitrate(int width, int height) {
  return InterpolateSimulcastFormat(width, height).max_bitrate;
}

webrtc::DataRate FindSimulcastTargetBitrate(int width, int height) {
  return InterpolateSimulcastFormat(width, height).target_bitrate;
}

webrtc::DataRate FindSimulcastMinBitrate(int width, int height) {
  return InterpolateSimulcastFormat(width, height).min_bitrate;
}

void BoostMaxSimulcastLayer(webrtc::DataRate max_bitrate,
                            std::vector<webrtc::VideoStream>* layers) {
  if (layers->empty())
    return;

  const webrtc::DataRate total_bitrate = GetTotalMaxBitrate(*layers);

  // We're still not using all available bits.
  if (total_bitrate < max_bitrate) {
    // Spend additional bits to boost the max layer.
    const webrtc::DataRate bitrate_left = max_bitrate - total_bitrate;
    layers->back().max_bitrate_bps += bitrate_left.bps();
  }
}

webrtc::DataRate GetTotalMaxBitrate(
    const std::vector<webrtc::VideoStream>& layers) {
  if (layers.empty())
    return webrtc::DataRate::Zero();

  int total_max_bitrate_bps = 0;
  for (size_t s = 0; s < layers.size() - 1; ++s) {
    total_max_bitrate_bps += layers[s].target_bitrate_bps;
  }
  total_max_bitrate_bps += layers.back().max_bitrate_bps;
  return webrtc::DataRate::BitsPerSec(total_max_bitrate_bps);
}

size_t LimitSimulcastLayerCount(int width,
                                int height,
                                size_t need_layers,
                                size_t layer_count) {
  if (!webrtc::field_trial::IsDisabled(
          kUseLegacySimulcastLayerLimitFieldTrial)) {
    size_t adaptive_layer_count = std::max(
        need_layers,
        kSimulcastFormats[FindSimulcastFormatIndex(width, height)].max_layers);
    if (layer_count > adaptive_layer_count) {
      RTC_LOG(LS_WARNING) << "Reducing simulcast layer count from "
                          << layer_count << " to " << adaptive_layer_count;
      layer_count = adaptive_layer_count;
    }
  }
  return layer_count;
}

std::vector<webrtc::VideoStream> GetSimulcastConfig(
    size_t min_layers,
    size_t max_layers,
    int width,
    int height,
    double bitrate_priority,
    int max_qp,
    bool is_screenshare_with_conference_mode,
    bool temporal_layers_supported) {
  RTC_DCHECK_LE(min_layers, max_layers);
  RTC_DCHECK(max_layers > 1 || is_screenshare_with_conference_mode);

  const bool base_heavy_tl3_rate_alloc =
      webrtc::RateControlSettings::ParseFromFieldTrials()
          .Vp8BaseHeavyTl3RateAllocation();
  if (is_screenshare_with_conference_mode) {
    return GetScreenshareLayers(max_layers, width, height, bitrate_priority,
                                max_qp, temporal_layers_supported,
                                base_heavy_tl3_rate_alloc);
  } else {
    // Some applications rely on the old behavior limiting the simulcast layer
    // count based on the resolution automatically, which they can get through
    // the WebRTC-LegacySimulcastLayerLimit field trial until they update.
    max_layers =
        LimitSimulcastLayerCount(width, height, min_layers, max_layers);

    return GetNormalSimulcastLayers(max_layers, width, height, bitrate_priority,
                                    max_qp, temporal_layers_supported,
                                    base_heavy_tl3_rate_alloc);
  }
}

std::vector<webrtc::VideoStream> GetNormalSimulcastLayers(
    size_t layer_count,
    int width,
    int height,
    double bitrate_priority,
    int max_qp,
    bool temporal_layers_supported,
    bool base_heavy_tl3_rate_alloc) {
  std::vector<webrtc::VideoStream> layers(layer_count);

  // Format width and height has to be divisible by |2 ^ num_simulcast_layers -
  // 1|.
  width = NormalizeSimulcastSize(width, layer_count);
  height = NormalizeSimulcastSize(height, layer_count);
  // Add simulcast streams, from highest resolution (|s| = num_simulcast_layers
  // -1) to lowest resolution at |s| = 0.
  for (size_t s = layer_count - 1;; --s) {
    layers[s].width = width;
    layers[s].height = height;
    // TODO(pbos): Fill actual temporal-layer bitrate thresholds.
    layers[s].max_qp = max_qp;
    layers[s].num_temporal_layers =
        temporal_layers_supported ? DefaultNumberOfTemporalLayers(s, false) : 1;
    layers[s].max_bitrate_bps = FindSimulcastMaxBitrate(width, height).bps();
    layers[s].target_bitrate_bps =
        FindSimulcastTargetBitrate(width, height).bps();
    int num_temporal_layers = DefaultNumberOfTemporalLayers(s, false);
    if (s == 0) {
      // If alternative temporal rate allocation is selected, adjust the
      // bitrate of the lowest simulcast stream so that absolute bitrate for
      // the base temporal layer matches the bitrate for the base temporal
      // layer with the default 3 simulcast streams. Otherwise we risk a
      // higher threshold for receiving a feed at all.
      float rate_factor = 1.0;
      if (num_temporal_layers == 3) {
        if (base_heavy_tl3_rate_alloc) {
          // Base heavy allocation increases TL0 bitrate from 40% to 60%.
          rate_factor = 0.4 / 0.6;
        }
      } else {
        rate_factor =
            webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
                3, 0, /*base_heavy_tl3_rate_alloc=*/false) /
            webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
                num_temporal_layers, 0, /*base_heavy_tl3_rate_alloc=*/false);
      }

      layers[s].max_bitrate_bps =
          static_cast<int>(layers[s].max_bitrate_bps * rate_factor);
      layers[s].target_bitrate_bps =
          static_cast<int>(layers[s].target_bitrate_bps * rate_factor);
    }
    layers[s].min_bitrate_bps = FindSimulcastMinBitrate(width, height).bps();
    layers[s].max_framerate = kDefaultVideoMaxFramerate;

    width /= 2;
    height /= 2;

    if (s == 0) {
      break;
    }
  }
  // Currently the relative bitrate priority of the sender is controlled by
  // the value of the lowest VideoStream.
  // TODO(bugs.webrtc.org/8630): The web specification describes being able to
  // control relative bitrate for each individual simulcast layer, but this
  // is currently just implemented per rtp sender.
  layers[0].bitrate_priority = bitrate_priority;
  return layers;
}

std::vector<webrtc::VideoStream> GetScreenshareLayers(
    size_t max_layers,
    int width,
    int height,
    double bitrate_priority,
    int max_qp,
    bool temporal_layers_supported,
    bool base_heavy_tl3_rate_alloc) {
  auto max_screenshare_layers = kMaxScreenshareSimulcastLayers;
  size_t num_simulcast_layers =
      std::min<int>(max_layers, max_screenshare_layers);

  std::vector<webrtc::VideoStream> layers(num_simulcast_layers);
  // For legacy screenshare in conference mode, tl0 and tl1 bitrates are
  // piggybacked on the VideoCodec struct as target and max bitrates,
  // respectively. See eg. webrtc::LibvpxVp8Encoder::SetRates().
  layers[0].width = width;
  layers[0].height = height;
  layers[0].max_qp = max_qp;
  layers[0].max_framerate = 5;
  layers[0].min_bitrate_bps = webrtc::kDefaultMinVideoBitrateBps;
  layers[0].target_bitrate_bps = kScreenshareDefaultTl0Bitrate.bps();
  layers[0].max_bitrate_bps = kScreenshareDefaultTl1Bitrate.bps();
  layers[0].num_temporal_layers = temporal_layers_supported ? 2 : 1;

  // With simulcast enabled, add another spatial layer. This one will have a
  // more normal layout, with the regular 3 temporal layer pattern and no fps
  // restrictions. The base simulcast layer will still use legacy setup.
  if (num_simulcast_layers == kMaxScreenshareSimulcastLayers) {
    // Add optional upper simulcast layer.
    const int num_temporal_layers = DefaultNumberOfTemporalLayers(1, true);
    int max_bitrate_bps;
    bool using_boosted_bitrate = false;
    if (!temporal_layers_supported) {
      // Set the max bitrate to where the base layer would have been if temporal
      // layers were enabled.
      max_bitrate_bps = static_cast<int>(
          kScreenshareHighStreamMaxBitrate.bps() *
          webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
              num_temporal_layers, 0, base_heavy_tl3_rate_alloc));
    } else if (DefaultNumberOfTemporalLayers(1, true) != 3 ||
               base_heavy_tl3_rate_alloc) {
      // Experimental temporal layer mode used, use increased max bitrate.
      max_bitrate_bps = kScreenshareHighStreamMaxBitrate.bps();
      using_boosted_bitrate = true;
    } else {
      // Keep current bitrates with default 3tl/8 frame settings.
      // Lowest temporal layers of a 3 layer setup will have 40% of the total
      // bitrate allocation for that simulcast layer. Make sure the gap between
      // the target of the lower simulcast layer and first temporal layer of the
      // higher one is at most 2x the bitrate, so that upswitching is not
      // hampered by stalled bitrate estimates.
      max_bitrate_bps = 2 * ((layers[0].target_bitrate_bps * 10) / 4);
    }

    layers[1].width = width;
    layers[1].height = height;
    layers[1].max_qp = max_qp;
    layers[1].max_framerate = kDefaultVideoMaxFramerate;
    layers[1].num_temporal_layers =
        temporal_layers_supported ? DefaultNumberOfTemporalLayers(1, true) : 1;
    layers[1].min_bitrate_bps = using_boosted_bitrate
                                    ? kScreenshareHighStreamMinBitrate.bps()
                                    : layers[0].target_bitrate_bps * 2;

    // Cap max bitrate so it isn't overly high for the given resolution.
    int resolution_limited_bitrate =
        std::max<int>(FindSimulcastMaxBitrate(width, height).bps(),
                      layers[1].min_bitrate_bps);
    max_bitrate_bps =
        std::min<int>(max_bitrate_bps, resolution_limited_bitrate);

    layers[1].target_bitrate_bps = max_bitrate_bps;
    layers[1].max_bitrate_bps = max_bitrate_bps;
  }

  // The bitrate priority currently implemented on a per-sender level, so we
  // just set it for the first simulcast layer.
  layers[0].bitrate_priority = bitrate_priority;
  return layers;
}

}  // namespace cricket
