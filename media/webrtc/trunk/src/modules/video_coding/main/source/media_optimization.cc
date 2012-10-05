/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media_optimization.h"

#include "content_metrics_processing.h"
#include "frame_dropper.h"
#include "qm_select.h"
#include "modules/video_coding/main/source/tick_time_base.h"

namespace webrtc {

VCMMediaOptimization::VCMMediaOptimization(WebRtc_Word32 id,
                                           TickTimeBase* clock):
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
_avgSentBitRateBps(0.0f),
_keyFrameCnt(0),
_deltaFrameCnt(0),
_lastQMUpdateTime(0),
_lastChangeTime(0),
_numLayers(0)
{
    memset(_sendStatistics, 0, sizeof(_sendStatistics));
    memset(_incomingFrameTimes, -1, sizeof(_incomingFrameTimes));

    _frameDropper  = new VCMFrameDropper(_id);
    _lossProtLogic = new VCMLossProtectionLogic(_clock->MillisecondTimestamp());
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

WebRtc_Word32
VCMMediaOptimization::Reset()
{
    memset(_incomingFrameTimes, -1, sizeof(_incomingFrameTimes));
    _incomingFrameRate = 0.0;
    _frameDropper->Reset();
    _lossProtLogic->Reset(_clock->MillisecondTimestamp());
    _frameDropper->SetRates(0, 0);
    _content->Reset();
    _qmResolution->Reset();
    _lossProtLogic->UpdateFrameRate(_incomingFrameRate);
    _lossProtLogic->Reset(_clock->MillisecondTimestamp());
    _sendStatisticsZeroEncode = 0;
    _targetBitRate = 0;
    _codecWidth = 0;
    _codecHeight = 0;
    _userFrameRate = 0;
    _keyFrameCnt = 0;
    _deltaFrameCnt = 0;
    _lastQMUpdateTime = 0;
    _lastChangeTime = 0;
    for (WebRtc_Word32 i = 0; i < kBitrateMaxFrameSamples; i++)
    {
        _encodedFrameSamples[i]._sizeBytes = -1;
        _encodedFrameSamples[i]._timeCompleteMs = -1;
    }
    _avgSentBitRateBps = 0.0f;
    _numLayers = 1;
    return VCM_OK;
}

WebRtc_UWord32
VCMMediaOptimization::SetTargetRates(WebRtc_UWord32 bitRate,
                                     WebRtc_UWord8 &fractionLost,
                                     WebRtc_UWord32 roundTripTimeMs)
{
    VCMProtectionMethod *selectedMethod = _lossProtLogic->SelectedMethod();
    _lossProtLogic->UpdateBitRate(static_cast<float>(bitRate));
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
    WebRtc_UWord8 packetLossEnc = _lossProtLogic->FilteredLoss(
        _clock->MillisecondTimestamp(), filter_mode, fractionLost);

    // For now use the filtered loss for computing the robustness settings
    _lossProtLogic->UpdateFilteredLossPr(packetLossEnc);

    // Rate cost of the protection methods
    uint32_t protection_overhead_kbps = 0;

    // Update protection settings, when applicable
    float sent_video_rate = 0.0f;
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
          protection_overhead_kbps = static_cast<uint32_t>(bitRate *
              static_cast<double>(sent_nack_rate_bps + sent_fec_rate_bps) /
              sent_total_rate_bps + 0.5);
        }
        // Cap the overhead estimate to 50%.
        if (protection_overhead_kbps > bitRate / 2)
          protection_overhead_kbps = bitRate / 2;

        // Get the effective packet loss for encoder ER
        // when applicable, should be passed to encoder via fractionLost
        packetLossEnc = selectedMethod->RequiredPacketLossER();
        sent_video_rate =  static_cast<float>(sent_video_rate_bps / 1000.0);
    }

    // Source coding rate: total rate - protection overhead
    _targetBitRate = bitRate - protection_overhead_kbps;

    // Update encoding rates following protection settings
    _frameDropper->SetRates(static_cast<float>(_targetBitRate), 0);

    if (_enableQm)
    {
        // Update QM with rates
        _qmResolution->UpdateRates((float)_targetBitRate, sent_video_rate,
                                  _incomingFrameRate, _fractionLost);
        // Check for QM selection
        bool selectQM = checkStatusForQMchange();
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
    _frameDropper->Leak((WebRtc_UWord32)(InputFrameRate() + 0.5f));
    return _frameDropper->DropFrame();
}

WebRtc_Word32
VCMMediaOptimization::SentFrameCount(VCMFrameCount &frameCount) const
{
    frameCount.numDeltaFrames = _deltaFrameCnt;
    frameCount.numKeyFrames = _keyFrameCnt;
    return VCM_OK;
}

WebRtc_Word32
VCMMediaOptimization::SetEncodingData(VideoCodecType sendCodecType,
                                      WebRtc_Word32 maxBitRate,
                                      WebRtc_UWord32 frameRate,
                                      WebRtc_UWord32 bitRate,
                                      WebRtc_UWord16 width,
                                      WebRtc_UWord16 height,
                                      int numLayers)
{
    // Everything codec specific should be reset here since this means the codec
    // has changed. If native dimension values have changed, then either user
    // initiated change, or QM initiated change. Will be able to determine only
    // after the processing of the first frame.
    _lastChangeTime = _clock->MillisecondTimestamp();
    _content->Reset();
    _content->UpdateFrameRate(frameRate);

    _maxBitRate = maxBitRate;
    _sendCodecType = sendCodecType;
    _targetBitRate = bitRate;
    _lossProtLogic->UpdateBitRate(static_cast<float>(bitRate));
    _lossProtLogic->UpdateFrameRate(static_cast<float>(frameRate));
    _lossProtLogic->UpdateFrameSize(width, height);
    _lossProtLogic->UpdateNumLayers(numLayers);
    _frameDropper->Reset();
    _frameDropper->SetRates(static_cast<float>(bitRate),
                            static_cast<float>(frameRate));
    _userFrameRate = static_cast<float>(frameRate);
    _codecWidth = width;
    _codecHeight = height;
    _numLayers = (numLayers <= 1) ? 1 : numLayers;  // Can also be zero.
    WebRtc_Word32 ret = VCM_OK;
    ret = _qmResolution->Initialize((float)_targetBitRate, _userFrameRate,
                                    _codecWidth, _codecHeight, _numLayers);
    return ret;
}

WebRtc_Word32
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
VCMMediaOptimization::SetMtu(WebRtc_Word32 mtu)
{
    _maxPayloadSize = mtu;
}

float
VCMMediaOptimization::SentFrameRate()
{
    if (_frameDropper)
    {
        return _frameDropper->ActualFrameRate((WebRtc_UWord32)(InputFrameRate()
                                                               + 0.5f));
    }

    return VCM_CODEC_ERROR;
}

float
VCMMediaOptimization::SentBitRate()
{
    UpdateBitRateEstimate(-1, _clock->MillisecondTimestamp());
    return _avgSentBitRateBps / 1000.0f;
}

WebRtc_Word32
VCMMediaOptimization::MaxBitRate()
{
    return _maxBitRate;
}

WebRtc_Word32
VCMMediaOptimization::UpdateWithEncodedData(WebRtc_Word32 encodedLength,
                                            FrameType encodedFrameType)
{
    // look into the ViE version - debug mode - needs also number of layers.
    UpdateBitRateEstimate(encodedLength, _clock->MillisecondTimestamp());
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
                    minPacketsPerFrame, _clock->MillisecondTimestamp());
            }
            else
            {
                _lossProtLogic->UpdatePacketsPerFrameKey(
                    minPacketsPerFrame, _clock->MillisecondTimestamp());
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

void VCMMediaOptimization::UpdateBitRateEstimate(WebRtc_Word64 encodedLength,
                                                 WebRtc_Word64 nowMs)
{
    int i = kBitrateMaxFrameSamples - 1;
    WebRtc_UWord32 frameSizeSum = 0;
    WebRtc_Word64 timeOldest = -1;
    // Find an empty slot for storing the new sample and at the same time
    // accumulate the history.
    for (; i >= 0; i--)
    {
        if (_encodedFrameSamples[i]._sizeBytes == -1)
        {
            // Found empty slot
            break;
        }
        if (nowMs - _encodedFrameSamples[i]._timeCompleteMs <
            kBitrateAverageWinMs)
        {
            frameSizeSum += static_cast<WebRtc_UWord32>
                            (_encodedFrameSamples[i]._sizeBytes);
            if (timeOldest == -1)
            {
                timeOldest = _encodedFrameSamples[i]._timeCompleteMs;
            }
        }
    }
    if (encodedLength > 0)
    {
        if (i < 0)
        {
            // No empty slot, shift
            for (i = kBitrateMaxFrameSamples - 2; i >= 0; i--)
            {
                _encodedFrameSamples[i + 1] = _encodedFrameSamples[i];
            }
            i++;
        }
        // Insert new sample
        _encodedFrameSamples[i]._sizeBytes = encodedLength;
        _encodedFrameSamples[i]._timeCompleteMs = nowMs;
    }
    if (timeOldest > -1)
    {
        // Update average bit rate
        float denom = static_cast<float>(nowMs - timeOldest);
        if (denom < 1.0)
        {
            denom = 1.0;
        }
        _avgSentBitRateBps = (frameSizeSum + encodedLength) * 8 * 1000 / denom;
    }
    else if (encodedLength > 0)
    {
        _avgSentBitRateBps = static_cast<float>(encodedLength * 8);
    }
    else
    {
        _avgSentBitRateBps = 0;
    }
}


WebRtc_Word32
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
VCMMediaOptimization::updateContentData(const VideoContentMetrics*
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

WebRtc_Word32
VCMMediaOptimization::SelectQuality()
{
    // Reset quantities for QM select
    _qmResolution->ResetQM();

    // Update QM will long-term averaged content metrics.
    _qmResolution->UpdateContent(_content->LongTermAvgData());

    // Select quality mode
    VCMResolutionScale* qm = NULL;
    WebRtc_Word32 ret = _qmResolution->SelectResolution(&qm);
    if (ret < 0)
    {
        return ret;
    }

    // Check for updates to spatial/temporal modes
    QMUpdate(qm);

    // Reset all the rate and related frame counters quantities
    _qmResolution->ResetRates();

    // Reset counters
    _lastQMUpdateTime = _clock->MillisecondTimestamp();

    // Reset content metrics
    _content->Reset();

    return VCM_OK;
}


// Check timing constraints and look for significant change in:
// (1) scene content
// (2) target bit rate

bool
VCMMediaOptimization::checkStatusForQMchange()
{

    bool status  = true;

    // Check that we do not call QMSelect too often, and that we waited some time
    // (to sample the metrics) from the event lastChangeTime
    // lastChangeTime is the time where user changed the size/rate/frame rate
    // (via SetEncodingData)
    WebRtc_Word64 now = _clock->MillisecondTimestamp();
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
    WebRtc_Word64 now = _clock->MillisecondTimestamp();
    if (_incomingFrameTimes[0] == 0)
    {
        // first no shift
    } else
    {
        // shift
        for(WebRtc_Word32 i = (kFrameCountHistorySize - 2); i >= 0 ; i--)
        {
            _incomingFrameTimes[i+1] = _incomingFrameTimes[i];
        }
    }
    _incomingFrameTimes[0] = now;
    ProcessIncomingFrameRate(now);
}

// allowing VCM to keep track of incoming frame rate
void
VCMMediaOptimization::ProcessIncomingFrameRate(WebRtc_Word64 now)
{
    WebRtc_Word32 num = 0;
    WebRtc_Word32 nrOfFrames = 0;
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
        const WebRtc_Word64 diff = now - _incomingFrameTimes[num-1];
        _incomingFrameRate = 1.0;
        if(diff >0)
        {
            _incomingFrameRate = nrOfFrames * 1000.0f / static_cast<float>(diff);
        }
    }
}

WebRtc_UWord32
VCMMediaOptimization::InputFrameRate()
{
    ProcessIncomingFrameRate(_clock->MillisecondTimestamp());
    return WebRtc_UWord32 (_incomingFrameRate + 0.5f);
}

}
