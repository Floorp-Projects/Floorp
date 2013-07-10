/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/source/media_optimization.h"

#include "webrtc/modules/video_coding/utility/include/frame_dropper.h"
#include "webrtc/modules/video_coding/main/source/content_metrics_processing.h"
#include "webrtc/modules/video_coding/main/source/qm_select.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {
namespace media_optimization {

VCMMediaOptimization::VCMMediaOptimization(int32_t id,
                                           Clock* clock):
_id(id),
_clock(clock),
_maxBitRate(0),
_sendCodecType(kVideoCodecUnknown),
_codecWidth(0),
_codecHeight(0),
_userFrameRate(0),
_fractionLost(0),
_sendStatisticsZeroEncode(0),
_maxPayloadSize(1460),
_targetBitRate(0),
_incomingFrameRate(0),
_enableQm(false),
_videoProtectionCallback(NULL),
_videoQMSettingsCallback(NULL),
_encodedFrameSamples(),
_avgSentBitRateBps(0),
_avgSentFramerate(0),
_keyFrameCnt(0),
_deltaFrameCnt(0),
_lastQMUpdateTime(0),
_lastChangeTime(0),
_numLayers(0)
{
    memset(_sendStatistics, 0, sizeof(_sendStatistics));
    memset(_incomingFrameTimes, -1, sizeof(_incomingFrameTimes));

    _frameDropper  = new FrameDropper;
    _lossProtLogic = new VCMLossProtectionLogic(_clock->TimeInMilliseconds());
    _content = new VCMContentMetricsProcessing();
    _qmResolution = new VCMQmResolution();
}

VCMMediaOptimization::~VCMMediaOptimization(void)
{
    _lossProtLogic->Release();
    delete _lossProtLogic;
    delete _frameDropper;
    delete _content;
    delete _qmResolution;
}

int32_t
VCMMediaOptimization::Reset()
{
    memset(_incomingFrameTimes, -1, sizeof(_incomingFrameTimes));
    _incomingFrameRate = 0.0;
    _frameDropper->Reset();
    _lossProtLogic->Reset(_clock->TimeInMilliseconds());
    _frameDropper->SetRates(0, 0);
    _content->Reset();
    _qmResolution->Reset();
    _lossProtLogic->UpdateFrameRate(_incomingFrameRate);
    _lossProtLogic->Reset(_clock->TimeInMilliseconds());
    _sendStatisticsZeroEncode = 0;
    _targetBitRate = 0;
    _codecWidth = 0;
    _codecHeight = 0;
    _userFrameRate = 0;
    _keyFrameCnt = 0;
    _deltaFrameCnt = 0;
    _lastQMUpdateTime = 0;
    _lastChangeTime = 0;
    _encodedFrameSamples.clear();
    _avgSentBitRateBps = 0;
    _numLayers = 1;
    return VCM_OK;
}

uint32_t
VCMMediaOptimization::SetTargetRates(uint32_t target_bitrate,
                                     uint8_t &fractionLost,
                                     uint32_t roundTripTimeMs)
{
    // TODO(holmer): Consider putting this threshold only on the video bitrate,
    // and not on protection.
    if (_maxBitRate > 0 &&
        target_bitrate > static_cast<uint32_t>(_maxBitRate)) {
      target_bitrate = _maxBitRate;
    }
    VCMProtectionMethod *selectedMethod = _lossProtLogic->SelectedMethod();
    float target_bitrate_kbps = static_cast<float>(target_bitrate) / 1000.0f;
    _lossProtLogic->UpdateBitRate(target_bitrate_kbps);
    _lossProtLogic->UpdateRtt(roundTripTimeMs);
    _lossProtLogic->UpdateResidualPacketLoss(static_cast<float>(fractionLost));

    // Get frame rate for encoder: this is the actual/sent frame rate
    float actualFrameRate = SentFrameRate();

    // sanity
    if (actualFrameRate  < 1.0)
    {
        actualFrameRate = 1.0;
    }

    // Update frame rate for the loss protection logic class: frame rate should
    // be the actual/sent rate
    _lossProtLogic->UpdateFrameRate(actualFrameRate);

    _fractionLost = fractionLost;

    // Returns the filtered packet loss, used for the protection setting.
    // The filtered loss may be the received loss (no filter), or some
    // filtered value (average or max window filter).
    // Use max window filter for now.
    FilterPacketLossMode filter_mode = kMaxFilter;
    uint8_t packetLossEnc = _lossProtLogic->FilteredLoss(
        _clock->TimeInMilliseconds(), filter_mode, fractionLost);

    // For now use the filtered loss for computing the robustness settings
    _lossProtLogic->UpdateFilteredLossPr(packetLossEnc);

    // Rate cost of the protection methods
    uint32_t protection_overhead_bps = 0;

    // Update protection settings, when applicable
    float sent_video_rate_kbps = 0.0f;
    if (selectedMethod)
    {
        // Update protection method with content metrics
        selectedMethod->UpdateContentMetrics(_content->ShortTermAvgData());

        // Update method will compute the robustness settings for the given
        // protection method and the overhead cost
        // the protection method is set by the user via SetVideoProtection.
        _lossProtLogic->UpdateMethod();

        // Update protection callback with protection settings.
        uint32_t sent_video_rate_bps = 0;
        uint32_t sent_nack_rate_bps = 0;
        uint32_t sent_fec_rate_bps = 0;
        // Get the bit cost of protection method, based on the amount of
        // overhead data actually transmitted (including headers) the last
        // second.
        UpdateProtectionCallback(selectedMethod,
                                 &sent_video_rate_bps,
                                 &sent_nack_rate_bps,
                                 &sent_fec_rate_bps);
        uint32_t sent_total_rate_bps = sent_video_rate_bps +
            sent_nack_rate_bps + sent_fec_rate_bps;
        // Estimate the overhead costs of the next second as staying the same
        // wrt the source bitrate.
        if (sent_total_rate_bps > 0) {
          protection_overhead_bps = static_cast<uint32_t>(target_bitrate *
              static_cast<double>(sent_nack_rate_bps + sent_fec_rate_bps) /
              sent_total_rate_bps + 0.5);
        }
        // Cap the overhead estimate to 50%.
        if (protection_overhead_bps > target_bitrate / 2)
          protection_overhead_bps = target_bitrate / 2;

        // Get the effective packet loss for encoder ER
        // when applicable, should be passed to encoder via fractionLost
        packetLossEnc = selectedMethod->RequiredPacketLossER();
        sent_video_rate_kbps =
            static_cast<float>(sent_video_rate_bps) / 1000.0f;
    }

    // Source coding rate: total rate - protection overhead
    _targetBitRate = target_bitrate - protection_overhead_bps;

    // Update encoding rates following protection settings
    float target_video_bitrate_kbps =
        static_cast<float>(_targetBitRate) / 1000.0f;
    _frameDropper->SetRates(target_video_bitrate_kbps, _incomingFrameRate);

    if (_enableQm)
    {
        // Update QM with rates
        _qmResolution->UpdateRates(target_video_bitrate_kbps,
                                   sent_video_rate_kbps, _incomingFrameRate,
                                   _fractionLost);
        // Check for QM selection
        bool selectQM = CheckStatusForQMchange();
        if (selectQM)
        {
            SelectQuality();
        }
        // Reset the short-term averaged content data.
        _content->ResetShortTermAvgData();
    }

    return _targetBitRate;
}

int VCMMediaOptimization::UpdateProtectionCallback(
    VCMProtectionMethod *selected_method,
    uint32_t* video_rate_bps,
    uint32_t* nack_overhead_rate_bps,
    uint32_t* fec_overhead_rate_bps)
{
    if (!_videoProtectionCallback)
    {
        return VCM_OK;
    }
    FecProtectionParams delta_fec_params;
    FecProtectionParams key_fec_params;
    // Get the FEC code rate for Key frames (set to 0 when NA)
    key_fec_params.fec_rate = selected_method->RequiredProtectionFactorK();

    // Get the FEC code rate for Delta frames (set to 0 when NA)
    delta_fec_params.fec_rate =
        selected_method->RequiredProtectionFactorD();

    // Get the FEC-UEP protection status for Key frames: UEP on/off
    key_fec_params.use_uep_protection =
        selected_method->RequiredUepProtectionK();

    // Get the FEC-UEP protection status for Delta frames: UEP on/off
    delta_fec_params.use_uep_protection =
        selected_method->RequiredUepProtectionD();

    // The RTP module currently requires the same |max_fec_frames| for both
    // key and delta frames.
    delta_fec_params.max_fec_frames = selected_method->MaxFramesFec();
    key_fec_params.max_fec_frames = selected_method->MaxFramesFec();

    // Set the FEC packet mask type. |kFecMaskBursty| is more effective for
    // consecutive losses and little/no packet re-ordering. As we currently
    // do not have feedback data on the degree of correlated losses and packet
    // re-ordering, we keep default setting to |kFecMaskRandom| for now.
    delta_fec_params.fec_mask_type = kFecMaskRandom;
    key_fec_params.fec_mask_type = kFecMaskRandom;

    // TODO(Marco): Pass FEC protection values per layer.
    return _videoProtectionCallback->ProtectionRequest(&delta_fec_params,
                                                       &key_fec_params,
                                                       video_rate_bps,
                                                       nack_overhead_rate_bps,
                                                       fec_overhead_rate_bps);
}

bool
VCMMediaOptimization::DropFrame()
{
    // leak appropriate number of bytes
    _frameDropper->Leak((uint32_t)(InputFrameRate() + 0.5f));

    return _frameDropper->DropFrame();
}

int32_t
VCMMediaOptimization::SentFrameCount(VCMFrameCount &frameCount) const
{
    frameCount.numDeltaFrames = _deltaFrameCnt;
    frameCount.numKeyFrames = _keyFrameCnt;
    return VCM_OK;
}

int32_t
VCMMediaOptimization::SetEncodingData(VideoCodecType sendCodecType,
                                      int32_t maxBitRate,
                                      uint32_t frameRate,
                                      uint32_t target_bitrate,
                                      uint16_t width,
                                      uint16_t height,
                                      int numLayers)
{
    // Everything codec specific should be reset here since this means the codec
    // has changed. If native dimension values have changed, then either user
    // initiated change, or QM initiated change. Will be able to determine only
    // after the processing of the first frame.
    _lastChangeTime = _clock->TimeInMilliseconds();
    _content->Reset();
    _content->UpdateFrameRate(frameRate);

    _maxBitRate = maxBitRate;
    _sendCodecType = sendCodecType;
    _targetBitRate = target_bitrate;
    float target_bitrate_kbps = static_cast<float>(target_bitrate) / 1000.0f;
    _lossProtLogic->UpdateBitRate(target_bitrate_kbps);
    _lossProtLogic->UpdateFrameRate(static_cast<float>(frameRate));
    _lossProtLogic->UpdateFrameSize(width, height);
    _lossProtLogic->UpdateNumLayers(numLayers);
    _frameDropper->Reset();
    _frameDropper->SetRates(target_bitrate_kbps, static_cast<float>(frameRate));
    _userFrameRate = static_cast<float>(frameRate);
    _codecWidth = width;
    _codecHeight = height;
    _numLayers = (numLayers <= 1) ? 1 : numLayers;  // Can also be zero.
    int32_t ret = VCM_OK;
    ret = _qmResolution->Initialize(target_bitrate_kbps, _userFrameRate,
                                    _codecWidth, _codecHeight, _numLayers);
    return ret;
}

int32_t
VCMMediaOptimization::RegisterProtectionCallback(VCMProtectionCallback*
                                                 protectionCallback)
{
    _videoProtectionCallback = protectionCallback;
    return VCM_OK;

}

void
VCMMediaOptimization::EnableFrameDropper(bool enable)
{
    _frameDropper->Enable(enable);
}

void
VCMMediaOptimization::EnableProtectionMethod(bool enable,
                                             VCMProtectionMethodEnum method)
{
    bool updated = false;
    if (enable)
    {
        updated = _lossProtLogic->SetMethod(method);
    }
    else
    {
        _lossProtLogic->RemoveMethod(method);
    }
    if (updated)
    {
        _lossProtLogic->UpdateMethod();
    }
}

bool
VCMMediaOptimization::IsProtectionMethodEnabled(VCMProtectionMethodEnum method)
{
    return (_lossProtLogic->SelectedType() == method);
}

void
VCMMediaOptimization::SetMtu(int32_t mtu)
{
    _maxPayloadSize = mtu;
}

uint32_t
VCMMediaOptimization::SentFrameRate()
{
  PurgeOldFrameSamples(_clock->TimeInMilliseconds());
  UpdateSentFramerate();
  return _avgSentFramerate;
}

uint32_t
VCMMediaOptimization::SentBitRate()
{
    const int64_t now_ms = _clock->TimeInMilliseconds();
    PurgeOldFrameSamples(now_ms);
    UpdateSentBitrate(now_ms);
    return _avgSentBitRateBps;
}

int32_t
VCMMediaOptimization::MaxBitRate()
{
    return _maxBitRate;
}

int32_t
VCMMediaOptimization::UpdateWithEncodedData(int encodedLength,
                                            uint32_t timestamp,
                                            FrameType encodedFrameType)
{
    const int64_t now_ms = _clock->TimeInMilliseconds();
    PurgeOldFrameSamples(now_ms);
    _encodedFrameSamples.push_back(VCMEncodedFrameSample(
        encodedLength, timestamp, now_ms));
    UpdateSentBitrate(now_ms);
    UpdateSentFramerate();
    if(encodedLength > 0)
    {
        const bool deltaFrame = (encodedFrameType != kVideoFrameKey &&
                                 encodedFrameType != kVideoFrameGolden);

        _frameDropper->Fill(encodedLength, deltaFrame);
        if (_maxPayloadSize > 0 && encodedLength > 0)
        {
            const float minPacketsPerFrame = encodedLength /
                                             static_cast<float>(_maxPayloadSize);
            if (deltaFrame)
            {
                _lossProtLogic->UpdatePacketsPerFrame(
                    minPacketsPerFrame, _clock->TimeInMilliseconds());
            }
            else
            {
                _lossProtLogic->UpdatePacketsPerFrameKey(
                    minPacketsPerFrame, _clock->TimeInMilliseconds());
            }

            if (_enableQm)
            {
                // update quality select with encoded length
                _qmResolution->UpdateEncodedSize(encodedLength,
                                                 encodedFrameType);
            }
        }
        if (!deltaFrame && encodedLength > 0)
        {
            _lossProtLogic->UpdateKeyFrameSize(static_cast<float>(encodedLength));
        }

        // updating counters
        if (deltaFrame)
        {
            _deltaFrameCnt++;
        }
        else
        {
            _keyFrameCnt++;
        }

    }

     return VCM_OK;

}

void VCMMediaOptimization::PurgeOldFrameSamples(int64_t now_ms) {
  while (!_encodedFrameSamples.empty()) {
    if (now_ms - _encodedFrameSamples.front().time_complete_ms >
        kBitrateAverageWinMs) {
      _encodedFrameSamples.pop_front();
    } else {
      break;
    }
  }
}

void VCMMediaOptimization::UpdateSentBitrate(int64_t now_ms) {
  if (_encodedFrameSamples.empty()) {
    _avgSentBitRateBps = 0;
    return;
  }
  int framesize_sum = 0;
  for (FrameSampleList::iterator it = _encodedFrameSamples.begin();
       it != _encodedFrameSamples.end(); ++it) {
    framesize_sum += it->size_bytes;
  }
  float denom = static_cast<float>(
      now_ms - _encodedFrameSamples.back().time_complete_ms);
  if (denom >= 1.0f) {
    _avgSentBitRateBps = static_cast<uint32_t>(framesize_sum * 8 * 1000 /
                                               denom + 0.5f);
  } else {
    _avgSentBitRateBps = framesize_sum * 8;
  }
}

void VCMMediaOptimization::UpdateSentFramerate() {
  if (_encodedFrameSamples.size() <= 1) {
    _avgSentFramerate = _encodedFrameSamples.size();
    return;
  }
  int denom = _encodedFrameSamples.back().timestamp -
      _encodedFrameSamples.front().timestamp;
  if (denom > 0) {
    _avgSentFramerate = (90000 * (_encodedFrameSamples.size() - 1) + denom / 2)
        / denom;
  } else {
    _avgSentFramerate = _encodedFrameSamples.size();
  }
}

int32_t
VCMMediaOptimization::RegisterVideoQMCallback(VCMQMSettingsCallback*
                                              videoQMSettings)
{
    _videoQMSettingsCallback = videoQMSettings;
    // Callback setting controls QM
    if (_videoQMSettingsCallback != NULL)
    {
        _enableQm = true;
    }
    else
    {
        _enableQm = false;
    }
    return VCM_OK;
}

void
VCMMediaOptimization::UpdateContentData(const VideoContentMetrics*
                                        contentMetrics)
{
    // Updating content metrics
    if (contentMetrics == NULL)
    {
         // Disable QM if metrics are NULL
         _enableQm = false;
         _qmResolution->Reset();
    }
    else
    {
        _content->UpdateContentData(contentMetrics);
    }
}

int32_t
VCMMediaOptimization::SelectQuality()
{
    // Reset quantities for QM select
    _qmResolution->ResetQM();

    // Update QM will long-term averaged content metrics.
    _qmResolution->UpdateContent(_content->LongTermAvgData());

    // Select quality mode
    VCMResolutionScale* qm = NULL;
    int32_t ret = _qmResolution->SelectResolution(&qm);
    if (ret < 0)
    {
        return ret;
    }

    // Check for updates to spatial/temporal modes
    QMUpdate(qm);

    // Reset all the rate and related frame counters quantities
    _qmResolution->ResetRates();

    // Reset counters
    _lastQMUpdateTime = _clock->TimeInMilliseconds();

    // Reset content metrics
    _content->Reset();

    return VCM_OK;
}


// Check timing constraints and look for significant change in:
// (1) scene content
// (2) target bit rate

bool
VCMMediaOptimization::CheckStatusForQMchange()
{

    bool status  = true;

    // Check that we do not call QMSelect too often, and that we waited some time
    // (to sample the metrics) from the event lastChangeTime
    // lastChangeTime is the time where user changed the size/rate/frame rate
    // (via SetEncodingData)
    int64_t now = _clock->TimeInMilliseconds();
    if ((now - _lastQMUpdateTime) < kQmMinIntervalMs ||
        (now  - _lastChangeTime) <  kQmMinIntervalMs)
    {
        status = false;
    }

    return status;

}

bool VCMMediaOptimization::QMUpdate(VCMResolutionScale* qm) {
  // Check for no change
  if (!qm->change_resolution_spatial && !qm->change_resolution_temporal) {
    return false;
  }

  // Check for change in frame rate.
  if (qm->change_resolution_temporal) {
    _incomingFrameRate = qm->frame_rate;
    // Reset frame rate estimate.
    memset(_incomingFrameTimes, -1, sizeof(_incomingFrameTimes));
  }

  // Check for change in frame size.
  if (qm->change_resolution_spatial) {
    _codecWidth = qm->codec_width;
    _codecHeight = qm->codec_height;
  }

  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, _id,
               "Resolution change from QM select: W = %d, H = %d, FR = %f",
               qm->codec_width, qm->codec_height, qm->frame_rate);

  // Update VPM with new target frame rate and frame size.
  // Note: use |qm->frame_rate| instead of |_incomingFrameRate| for updating
  // target frame rate in VPM frame dropper. The quantity |_incomingFrameRate|
  // will vary/fluctuate, and since we don't want to change the state of the
  // VPM frame dropper, unless a temporal action was selected, we use the
  // quantity |qm->frame_rate| for updating.
  _videoQMSettingsCallback->SetVideoQMSettings(qm->frame_rate,
                                               _codecWidth,
                                               _codecHeight);
  _content->UpdateFrameRate(qm->frame_rate);
  _qmResolution->UpdateCodecParameters(qm->frame_rate, _codecWidth,
                                       _codecHeight);
  return true;
}

void
VCMMediaOptimization::UpdateIncomingFrameRate()
{
    int64_t now = _clock->TimeInMilliseconds();
    if (_incomingFrameTimes[0] == 0)
    {
        // first no shift
    } else
    {
        // shift
        for(int32_t i = (kFrameCountHistorySize - 2); i >= 0 ; i--)
        {
            _incomingFrameTimes[i+1] = _incomingFrameTimes[i];
        }
    }
    _incomingFrameTimes[0] = now;
    ProcessIncomingFrameRate(now);
}

// allowing VCM to keep track of incoming frame rate
void
VCMMediaOptimization::ProcessIncomingFrameRate(int64_t now)
{
    int32_t num = 0;
    int32_t nrOfFrames = 0;
    for (num = 1; num < (kFrameCountHistorySize - 1); num++)
    {
        if (_incomingFrameTimes[num] <= 0 ||
            // don't use data older than 2 s
            now - _incomingFrameTimes[num] > kFrameHistoryWinMs)
        {
            break;
        } else
        {
            nrOfFrames++;
        }
    }
    if (num > 1)
    {
        const int64_t diff = now - _incomingFrameTimes[num-1];
        _incomingFrameRate = 1.0;
        if(diff >0)
        {
            _incomingFrameRate = nrOfFrames * 1000.0f / static_cast<float>(diff);
        }
    }
}

uint32_t
VCMMediaOptimization::InputFrameRate()
{
    ProcessIncomingFrameRate(_clock->TimeInMilliseconds());
    return uint32_t (_incomingFrameRate + 0.5f);
}

}  // namespace media_optimization
}  // namespace webrtc
