/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_MEDIA_OPT_UTIL_H_
#define WEBRTC_MODULES_VIDEO_CODING_MEDIA_OPT_UTIL_H_

#include "typedefs.h"
#include "trace.h"
#include "exp_filter.h"
#include "internal_defines.h"
#include "qm_select.h"

#include <cmath>
#include <cstdlib>


namespace webrtc
{
// Number of time periods used for (max) window filter for packet loss
// TODO (marpan): set reasonable window size for filtered packet loss,
// adjustment should be based on logged/real data of loss stats/correlation.
enum { kLossPrHistorySize = 10 };

// 1000 ms, total filter length is (kLossPrHistorySize * 1000) ms
enum { kLossPrShortFilterWinMs = 1000 };

// The type of filter used on the received packet loss reports.
enum FilterPacketLossMode {
  kNoFilter,    // No filtering on received loss.
  kAvgFilter,   // Recursive average filter.
  kMaxFilter    // Max-window filter, over the time interval of:
                // (kLossPrHistorySize * kLossPrShortFilterWinMs) ms.
};

// Thresholds for hybrid NACK/FEC
// common to media optimization and the jitter buffer.
enum HybridNackTH {
    kHighRttNackMs = 100,
    kLowRttNackMs = 20
};

struct VCMProtectionParameters
{
    VCMProtectionParameters() : rtt(0), lossPr(0.0f), bitRate(0.0f),
        packetsPerFrame(0.0f), packetsPerFrameKey(0.0f), frameRate(0.0f),
        keyFrameSize(0.0f), fecRateDelta(0), fecRateKey(0),
        residualPacketLossFec(0.0f), codecWidth(0), codecHeight(0),
        numLayers(1)
        {}

    int                 rtt;
    float               lossPr;
    float               bitRate;
    float               packetsPerFrame;
    float               packetsPerFrameKey;
    float               frameRate;
    float               keyFrameSize;
    WebRtc_UWord8       fecRateDelta;
    WebRtc_UWord8       fecRateKey;
    float               residualPacketLossFec;
    WebRtc_UWord16      codecWidth;
    WebRtc_UWord16      codecHeight;
    int                 numLayers;
};


/******************************/
/* VCMProtectionMethod class    */
/****************************/

enum VCMProtectionMethodEnum
{
    kNack,
    kFec,
    kNackFec,
    kNone
};

class VCMLossProbabilitySample
{
public:
    VCMLossProbabilitySample() : lossPr255(0), timeMs(-1) {};

    WebRtc_UWord8     lossPr255;
    WebRtc_Word64     timeMs;
};


class VCMProtectionMethod
{
public:
    VCMProtectionMethod();
    virtual ~VCMProtectionMethod();

    // Updates the efficiency of the method using the parameters provided
    //
    // Input:
    //         - parameters         : Parameters used to calculate efficiency
    //
    // Return value                 : True if this method is recommended in
    //                                the given conditions.
    virtual bool UpdateParameters(const VCMProtectionParameters* parameters) = 0;

    // Returns the protection type
    //
    // Return value                 : The protection type
    enum VCMProtectionMethodEnum Type() const { return _type; }

    // Returns the bit rate required by this protection method
    // during these conditions.
    //
    // Return value                 : Required bit rate
    virtual float RequiredBitRate() { return _efficiency; }

    // Returns the effective packet loss for ER, required by this protection method
    //
    // Return value                 : Required effective packet loss
    virtual WebRtc_UWord8 RequiredPacketLossER() { return _effectivePacketLoss; }

    // Extracts the FEC protection factor for Key frame, required by this protection method
    //
    // Return value                 : Required protectionFactor for Key frame
    virtual WebRtc_UWord8 RequiredProtectionFactorK() { return _protectionFactorK; }

    // Extracts the FEC protection factor for Delta frame, required by this protection method
    //
    // Return value                 : Required protectionFactor for delta frame
    virtual WebRtc_UWord8 RequiredProtectionFactorD() { return _protectionFactorD; }

    // Extracts whether the FEC Unequal protection (UEP) is used for Key frame.
    //
    // Return value                 : Required Unequal protection on/off state.
    virtual bool RequiredUepProtectionK() { return _useUepProtectionK; }

    // Extracts whether the the FEC Unequal protection (UEP) is used for Delta frame.
    //
    // Return value                 : Required Unequal protection on/off state.
    virtual bool RequiredUepProtectionD() { return _useUepProtectionD; }

    virtual int MaxFramesFec() const { return 1; }

    // Updates content metrics
    void UpdateContentMetrics(const VideoContentMetrics* contentMetrics);

protected:

    WebRtc_UWord8                        _effectivePacketLoss;
    WebRtc_UWord8                        _protectionFactorK;
    WebRtc_UWord8                        _protectionFactorD;
    // Estimation of residual loss after the FEC
    float                                _residualPacketLossFec;
    float                                _scaleProtKey;
    WebRtc_Word32                        _maxPayloadSize;

    VCMQmRobustness*                     _qmRobustness;
    bool                                 _useUepProtectionK;
    bool                                 _useUepProtectionD;
    float                                _corrFecCost;
    enum VCMProtectionMethodEnum         _type;
    float                                _efficiency;
};

class VCMNackMethod : public VCMProtectionMethod
{
public:
    VCMNackMethod();
    virtual ~VCMNackMethod();
    virtual bool UpdateParameters(const VCMProtectionParameters* parameters);
    // Get the effective packet loss
    bool EffectivePacketLoss(const VCMProtectionParameters* parameter);
};

class VCMFecMethod : public VCMProtectionMethod
{
public:
    VCMFecMethod();
    virtual ~VCMFecMethod();
    virtual bool UpdateParameters(const VCMProtectionParameters* parameters);
    // Get the effective packet loss for ER
    bool EffectivePacketLoss(const VCMProtectionParameters* parameters);
    // Get the FEC protection factors
    bool ProtectionFactor(const VCMProtectionParameters* parameters);
    // Get the boost for key frame protection
    WebRtc_UWord8 BoostCodeRateKey(WebRtc_UWord8 packetFrameDelta,
                                   WebRtc_UWord8 packetFrameKey) const;
    // Convert the rates: defined relative to total# packets or source# packets
    WebRtc_UWord8 ConvertFECRate(WebRtc_UWord8 codeRate) const;
    // Get the average effective recovery from FEC: for random loss model
    float AvgRecoveryFEC(const VCMProtectionParameters* parameters) const;
    // Update FEC with protectionFactorD
    void UpdateProtectionFactorD(WebRtc_UWord8 protectionFactorD);
    // Update FEC with protectionFactorK
    void UpdateProtectionFactorK(WebRtc_UWord8 protectionFactorK);
    // Compute the bits per frame. Account for temporal layers when applicable.
    int BitsPerFrame(const VCMProtectionParameters* parameters);

protected:
    enum { kUpperLimitFramesFec = 6 };
};


class VCMNackFecMethod : public VCMFecMethod
{
public:
    VCMNackFecMethod(int lowRttNackThresholdMs,
                     int highRttNackThresholdMs);
    virtual ~VCMNackFecMethod();
    virtual bool UpdateParameters(const VCMProtectionParameters* parameters);
    // Get the effective packet loss for ER
    bool EffectivePacketLoss(const VCMProtectionParameters* parameters);
    // Get the protection factors
    bool ProtectionFactor(const VCMProtectionParameters* parameters);
    // Get the max number of frames the FEC is allowed to be based on.
    int MaxFramesFec() const;

private:
    int ComputeMaxFramesFec(const VCMProtectionParameters* parameters);

    int _lowRttNackMs;
    int _highRttNackMs;
    int _maxFramesFec;
};

class VCMLossProtectionLogic
{
public:
    VCMLossProtectionLogic(int64_t nowMs);
    ~VCMLossProtectionLogic();

    // Set the protection method to be used
    //
    // Input:
    //        - newMethodType    : New requested protection method type. If one
    //                           is already set, it will be deleted and replaced
    // Return value:             Returns true on update
    bool SetMethod(enum VCMProtectionMethodEnum newMethodType);

    // Remove requested protection method
    // Input:
    //        - method          : method to be removed (if currently selected)
    //
    // Return value:             Returns true on update
    bool RemoveMethod(enum VCMProtectionMethodEnum method);

    // Return required bit rate per selected protectin method
    float RequiredBitRate() const;

    // Update the round-trip time
    //
    // Input:
    //          - rtt           : Round-trip time in seconds.
    void UpdateRtt(WebRtc_UWord32 rtt);

    // Update residual packet loss
    //
    // Input:
    //          - residualPacketLoss  : residual packet loss:
    //                                  effective loss after FEC recovery
    void UpdateResidualPacketLoss(float _residualPacketLoss);

    // Update the filtered packet loss.
    //
    // Input:
    //          - packetLossEnc :  The reported packet loss filtered
    //                             (max window or average)
    void UpdateFilteredLossPr(WebRtc_UWord8 packetLossEnc);

    // Update the current target bit rate.
    //
    // Input:
    //          - bitRate          : The current target bit rate in kbits/s
    void UpdateBitRate(float bitRate);

    // Update the number of packets per frame estimate, for delta frames
    //
    // Input:
    //          - nPackets         : Number of packets in the latest sent frame.
    void UpdatePacketsPerFrame(float nPackets, int64_t nowMs);

   // Update the number of packets per frame estimate, for key frames
    //
    // Input:
    //          - nPackets         : umber of packets in the latest sent frame.
    void UpdatePacketsPerFrameKey(float nPackets, int64_t nowMs);

    // Update the keyFrameSize estimate
    //
    // Input:
    //          - keyFrameSize     : The size of the latest sent key frame.
    void UpdateKeyFrameSize(float keyFrameSize);

    // Update the frame rate
    //
    // Input:
    //          - frameRate        : The current target frame rate.
    void UpdateFrameRate(float frameRate) { _frameRate = frameRate; }

    // Update the frame size
    //
    // Input:
    //          - width        : The codec frame width.
    //          - height       : The codec frame height.
    void UpdateFrameSize(WebRtc_UWord16 width, WebRtc_UWord16 height);

    // Update the number of active layers
    //
    // Input:
    //          - numLayers    : Number of layers used.
    void UpdateNumLayers(int numLayers);

    // The amount of packet loss to cover for with FEC.
    //
    // Input:
    //          - fecRateKey      : Packet loss to cover for with FEC when
    //                              sending key frames.
    //          - fecRateDelta    : Packet loss to cover for with FEC when
    //                              sending delta frames.
    void UpdateFECRates(WebRtc_UWord8 fecRateKey, WebRtc_UWord8 fecRateDelta)
                       { _fecRateKey = fecRateKey;
                         _fecRateDelta = fecRateDelta; }

    // Update the protection methods with the current VCMProtectionParameters
    // and set the requested protection settings.
    // Return value     : Returns true on update
    bool UpdateMethod();

    // Returns the method currently selected.
    //
    // Return value                 : The protection method currently selected.
    VCMProtectionMethod* SelectedMethod() const;

    // Return the protection type of the currently selected method
    VCMProtectionMethodEnum SelectedType() const;

    // Updates the filtered loss for the average and max window packet loss,
    // and returns the filtered loss probability in the interval [0, 255].
    // The returned filtered loss value depends on the parameter |filter_mode|.
    // The input parameter |lossPr255| is the received packet loss.

    // Return value                 : The filtered loss probability
    WebRtc_UWord8 FilteredLoss(int64_t nowMs, FilterPacketLossMode filter_mode,
                               WebRtc_UWord8 lossPr255);

    void Reset(int64_t nowMs);

    void Release();

private:
    // Sets the available loss protection methods.
    void UpdateMaxLossHistory(WebRtc_UWord8 lossPr255, WebRtc_Word64 now);
    WebRtc_UWord8 MaxFilteredLossPr(WebRtc_Word64 nowMs) const;
    VCMProtectionMethod*      _selectedMethod;
    VCMProtectionParameters   _currentParameters;
    WebRtc_UWord32            _rtt;
    float                     _lossPr;
    float                     _bitRate;
    float                     _frameRate;
    float                     _keyFrameSize;
    WebRtc_UWord8             _fecRateKey;
    WebRtc_UWord8             _fecRateDelta;
    WebRtc_Word64             _lastPrUpdateT;
    WebRtc_Word64             _lastPacketPerFrameUpdateT;
    WebRtc_Word64             _lastPacketPerFrameUpdateTKey;
    VCMExpFilter              _lossPr255;
    VCMLossProbabilitySample  _lossPrHistory[kLossPrHistorySize];
    WebRtc_UWord8             _shortMaxLossPr255;
    VCMExpFilter              _packetsPerFrame;
    VCMExpFilter              _packetsPerFrameKey;
    float                     _residualPacketLossFec;
    WebRtc_UWord8             _boostRateKey;
    WebRtc_UWord16            _codecWidth;
    WebRtc_UWord16            _codecHeight;
    int                       _numLayers;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_MEDIA_OPT_UTIL_H_
