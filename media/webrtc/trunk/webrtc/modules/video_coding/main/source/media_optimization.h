/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_MEDIA_OPTIMIZATION_H_
#define WEBRTC_MODULES_VIDEO_CODING_MEDIA_OPTIMIZATION_H_

#include "module_common_types.h"
#include "video_coding.h"
#include "trace.h"
#include "media_opt_util.h"
#include "qm_select.h"

#include <list>

namespace webrtc {

class Clock;
class FrameDropper;
class VCMContentMetricsProcessing;

namespace media_optimization {

enum { kBitrateMaxFrameSamples = 60 };
enum { kBitrateAverageWinMs    = 1000 };

struct VCMEncodedFrameSample {
  VCMEncodedFrameSample(int size_bytes, uint32_t timestamp,
                        int64_t time_complete_ms)
      : size_bytes(size_bytes),
        timestamp(timestamp),
        time_complete_ms(time_complete_ms) {}

  uint32_t size_bytes;
  uint32_t timestamp;
  int64_t time_complete_ms;
};

class VCMMediaOptimization
{
public:
    VCMMediaOptimization(int32_t id, Clock* clock);
    ~VCMMediaOptimization(void);
    /*
    * Reset the Media Optimization module
    */
    int32_t Reset();
    /**
    * Set target Rates for the encoder given the channel parameters
    * Inputs:       target bitrate - the encoder target bitrate in bits/s.
    *               fractionLost - packet loss in % in the network
    *               roundTripTimeMs - round trip time in milliseconds
    *               minBitRate - the bit rate of the end-point with lowest rate
    *               maxBitRate - the bit rate of the end-point with highest rate
    */
    uint32_t SetTargetRates(uint32_t target_bitrate,
                                  uint8_t &fractionLost,
                                  uint32_t roundTripTimeMs);

    /**
    * Inform media optimization of initial encoding state
    */
    int32_t SetEncodingData(VideoCodecType sendCodecType,
                                  int32_t maxBitRate,
                                  uint32_t frameRate,
                                  uint32_t bitRate,
                                  uint16_t width,
                                  uint16_t height,
                                  int numTemporalLayers);
    /**
    * Enable protection method
    */
    void EnableProtectionMethod(bool enable, VCMProtectionMethodEnum method);
    /**
    * Returns weather or not protection method is enabled
    */
    bool IsProtectionMethodEnabled(VCMProtectionMethodEnum method);
    /**
    * Updates the max pay load size
    */
    void SetMtu(int32_t mtu);
    /*
    * Get actual input frame rate
    */
    uint32_t InputFrameRate();

    /*
    * Get actual sent frame rate
    */
    uint32_t SentFrameRate();
    /*
    * Get actual sent bit rate
    */
    uint32_t SentBitRate();
    /*
    * Get maximum allowed bit rate
    */
    int32_t MaxBitRate();
    /*
    * Inform Media Optimization of encoding output: Length and frame type
    */
    int32_t UpdateWithEncodedData(int encodedLength,
                                        uint32_t timestamp,
                                        FrameType encodedFrameType);
    /*
    * Register a protection callback to be used to inform the user about the
    * protection methods used
    */
    int32_t RegisterProtectionCallback(VCMProtectionCallback*
                                             protectionCallback);
    /*
    * Register a quality settings callback to be used to inform VPM/user about
    */
    int32_t RegisterVideoQMCallback(VCMQMSettingsCallback* videoQMSettings);
    void EnableFrameDropper(bool enable);

    bool DropFrame();

      /*
    * Get number of key/delta frames encoded
    */
    int32_t SentFrameCount(VCMFrameCount &frameCount) const;

    /*
    *  update incoming frame rate value
    */
    void UpdateIncomingFrameRate();

    /**
    * Update content metric Data
    */
    void UpdateContentData(const VideoContentMetrics* contentMetrics);

    /**
    * Compute new Quality Mode
    */
    int32_t SelectQuality();

private:
    typedef std::list<VCMEncodedFrameSample> FrameSampleList;

    /*
     *  Update protection callback with protection settings
     */
    int UpdateProtectionCallback(VCMProtectionMethod *selected_method,
                                 uint32_t* total_video_rate_bps,
                                 uint32_t* nack_overhead_rate_bps,
                                 uint32_t* fec_overhead_rate_bps);

    void PurgeOldFrameSamples(int64_t now_ms);
    void UpdateSentBitrate(int64_t nowMs);
    void UpdateSentFramerate();

    /*
    * verify if QM settings differ from default, i.e. if an update is required
    * Compute actual values, as will be sent to the encoder
    */
    bool QMUpdate(VCMResolutionScale* qm);
    /**
    * check if we should make a QM change
    * will return 1 if yes, 0 otherwise
    */
    bool CheckStatusForQMchange();

    void ProcessIncomingFrameRate(int64_t now);

    enum { kFrameCountHistorySize = 90};
    enum { kFrameHistoryWinMs = 2000};

    int32_t                     _id;
    Clock*                            _clock;
    int32_t                     _maxBitRate;
    VideoCodecType                    _sendCodecType;
    uint16_t                    _codecWidth;
    uint16_t                    _codecHeight;
    float                             _userFrameRate;

    FrameDropper*                     _frameDropper;
    VCMLossProtectionLogic*           _lossProtLogic;
    uint8_t                     _fractionLost;


    uint32_t                    _sendStatistics[4];
    uint32_t                    _sendStatisticsZeroEncode;
    int32_t                     _maxPayloadSize;
    uint32_t                    _targetBitRate;

    float                             _incomingFrameRate;
    int64_t                     _incomingFrameTimes[kFrameCountHistorySize];

    bool                              _enableQm;

    VCMProtectionCallback*            _videoProtectionCallback;
    VCMQMSettingsCallback*            _videoQMSettingsCallback;

    std::list<VCMEncodedFrameSample>  _encodedFrameSamples;
    uint32_t                          _avgSentBitRateBps;
    uint32_t                          _avgSentFramerate;

    uint32_t                    _keyFrameCnt;
    uint32_t                    _deltaFrameCnt;

    VCMContentMetricsProcessing*      _content;
    VCMQmResolution*                  _qmResolution;

    int64_t                     _lastQMUpdateTime;
    int64_t                     _lastChangeTime; // content/user triggered
    int                               _numLayers;


}; // end of VCMMediaOptimization class definition

}  // namespace media_optimization
}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_MEDIA_OPTIMIZATION_H_
