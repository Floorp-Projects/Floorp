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

namespace webrtc
{

enum { kBitrateMaxFrameSamples = 60 };
enum { kBitrateAverageWinMs    = 1000 };

class TickTimeBase;
class VCMContentMetricsProcessing;
class VCMFrameDropper;

struct VCMEncodedFrameSample
{
    VCMEncodedFrameSample() : _sizeBytes(-1), _timeCompleteMs(-1) {}

    WebRtc_Word64     _sizeBytes;
    WebRtc_Word64     _timeCompleteMs;
};

class VCMMediaOptimization
{
public:
    VCMMediaOptimization(WebRtc_Word32 id, TickTimeBase* clock);
    ~VCMMediaOptimization(void);
    /*
    * Reset the Media Optimization module
    */
    WebRtc_Word32 Reset();
    /**
    * Set target Rates for the encoder given the channel parameters
    * Inputs:       bitRate - target bitRate, in the conference case this is the rate
    *                         between the sending client and the server
    *               fractionLost - packet loss in % in the network
    *               roundTripTimeMs - round trip time in miliseconds
    *               minBitRate - the bit rate of the end-point with lowest rate
    *               maxBitRate - the bit rate of the end-point with highest rate
    */
    WebRtc_UWord32 SetTargetRates(WebRtc_UWord32 bitRate,
                                  WebRtc_UWord8 &fractionLost,
                                  WebRtc_UWord32 roundTripTimeMs);

    /**
    * Inform media optimization of initial encoding state
    */
    WebRtc_Word32 SetEncodingData(VideoCodecType sendCodecType,
                                  WebRtc_Word32 maxBitRate,
                                  WebRtc_UWord32 frameRate,
                                  WebRtc_UWord32 bitRate,
                                  WebRtc_UWord16 width,
                                  WebRtc_UWord16 height,
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
    void SetMtu(WebRtc_Word32 mtu);
    /*
    * Get actual input frame rate
    */
    WebRtc_UWord32 InputFrameRate();

    /*
    * Get actual sent frame rate
    */
    float SentFrameRate();
    /*
    * Get actual sent bit rate
    */
    float SentBitRate();
    /*
    * Get maximum allowed bit rate
    */
    WebRtc_Word32 MaxBitRate();
    /*
    * Inform Media Optimization of encoding output: Length and frame type
    */
    WebRtc_Word32 UpdateWithEncodedData(WebRtc_Word32 encodedLength,
                                        FrameType encodedFrameType);
    /*
    * Register a protection callback to be used to inform the user about the
    * protection methods used
    */
    WebRtc_Word32 RegisterProtectionCallback(VCMProtectionCallback*
                                             protectionCallback);
    /*
    * Register a quality settings callback to be used to inform VPM/user about
    */
    WebRtc_Word32 RegisterVideoQMCallback(VCMQMSettingsCallback* videoQMSettings);
    void EnableFrameDropper(bool enable);

    bool DropFrame();

      /*
    * Get number of key/delta frames encoded
    */
    WebRtc_Word32 SentFrameCount(VCMFrameCount &frameCount) const;

    /*
    *  update incoming frame rate value
    */
    void UpdateIncomingFrameRate();

    /**
    * Update content metric Data
    */
    void updateContentData(const VideoContentMetrics* contentMetrics);

    /**
    * Compute new Quality Mode
    */
    WebRtc_Word32 SelectQuality();

private:

    /*
     *  Update protection callback with protection settings
     */
    int UpdateProtectionCallback(VCMProtectionMethod *selected_method,
                                 uint32_t* total_video_rate_bps,
                                 uint32_t* nack_overhead_rate_bps,
                                 uint32_t* fec_overhead_rate_bps);

    void UpdateBitRateEstimate(WebRtc_Word64 encodedLength, WebRtc_Word64 nowMs);
    /*
    * verify if QM settings differ from default, i.e. if an update is required
    * Compute actual values, as will be sent to the encoder
    */
    bool QMUpdate(VCMResolutionScale* qm);
    /**
    * check if we should make a QM change
    * will return 1 if yes, 0 otherwise
    */
    bool checkStatusForQMchange();

    void ProcessIncomingFrameRate(WebRtc_Word64 now);

    enum { kFrameCountHistorySize = 90};
    enum { kFrameHistoryWinMs = 2000};

    WebRtc_Word32                     _id;
    TickTimeBase*                     _clock;
    WebRtc_Word32                     _maxBitRate;
    VideoCodecType                    _sendCodecType;
    WebRtc_UWord16                    _codecWidth;
    WebRtc_UWord16                    _codecHeight;
    float                             _userFrameRate;

    VCMFrameDropper*                  _frameDropper;
    VCMLossProtectionLogic*           _lossProtLogic;
    WebRtc_UWord8                     _fractionLost;


    WebRtc_UWord32                    _sendStatistics[4];
    WebRtc_UWord32                    _sendStatisticsZeroEncode;
    WebRtc_Word32                     _maxPayloadSize;
    WebRtc_UWord32                    _targetBitRate;

    float                             _incomingFrameRate;
    WebRtc_Word64                     _incomingFrameTimes[kFrameCountHistorySize];

    bool                              _enableQm;

    VCMProtectionCallback*            _videoProtectionCallback;
    VCMQMSettingsCallback*            _videoQMSettingsCallback;

    VCMEncodedFrameSample             _encodedFrameSamples[kBitrateMaxFrameSamples];
    float                             _avgSentBitRateBps;

    WebRtc_UWord32                    _keyFrameCnt;
    WebRtc_UWord32                    _deltaFrameCnt;

    VCMContentMetricsProcessing*      _content;
    VCMQmResolution*                  _qmResolution;

    WebRtc_Word64                     _lastQMUpdateTime;
    WebRtc_Word64                     _lastChangeTime; // content/user triggered
    int                               _numLayers;


}; // end of VCMMediaOptimization class definition

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_MEDIA_OPTIMIZATION_H_
