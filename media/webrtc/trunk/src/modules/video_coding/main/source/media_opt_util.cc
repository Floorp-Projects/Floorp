/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/main/source/media_opt_util.h"

#include <algorithm>
#include <math.h>
#include <float.h>
#include <limits.h>

#include "modules/interface/module_common_types.h"
#include "modules/video_coding/codecs/vp8/main/interface/vp8_common_types.h"
#include "modules/video_coding/main/interface/video_coding_defines.h"
#include "modules/video_coding/main/source/er_tables_xor.h"
#include "modules/video_coding/main/source/fec_tables_xor.h"
#include "modules/video_coding/main/source/nack_fec_tables.h"

namespace webrtc {

VCMProtectionMethod::VCMProtectionMethod():
_effectivePacketLoss(0),
_protectionFactorK(0),
_protectionFactorD(0),
_residualPacketLossFec(0.0f),
_scaleProtKey(2.0f),
_maxPayloadSize(1460),
_qmRobustness(new VCMQmRobustness()),
_useUepProtectionK(false),
_useUepProtectionD(true),
_corrFecCost(1.0),
_type(kNone),
_efficiency(0)
{
    //
}

VCMProtectionMethod::~VCMProtectionMethod()
{
    delete _qmRobustness;
}
void
VCMProtectionMethod::UpdateContentMetrics(const
                                          VideoContentMetrics* contentMetrics)
{
    _qmRobustness->UpdateContent(contentMetrics);
}

VCMNackFecMethod::VCMNackFecMethod(int lowRttNackThresholdMs,
                                   int highRttNackThresholdMs)
    : VCMFecMethod(),
      _lowRttNackMs(lowRttNackThresholdMs),
      _highRttNackMs(highRttNackThresholdMs),
      _maxFramesFec(1) {
  assert(lowRttNackThresholdMs >= -1 && highRttNackThresholdMs >= -1);
  assert(highRttNackThresholdMs == -1 ||
         lowRttNackThresholdMs <= highRttNackThresholdMs);
  assert(lowRttNackThresholdMs > -1 || highRttNackThresholdMs == -1);
  _type = kNackFec;
}

VCMNackFecMethod::~VCMNackFecMethod()
{
    //
}
bool
VCMNackFecMethod::ProtectionFactor(const VCMProtectionParameters* parameters)
{
    // Hybrid Nack FEC has three operational modes:
    // 1. Low RTT (below kLowRttNackMs) - Nack only: Set FEC rate
    //    (_protectionFactorD) to zero. -1 means no FEC.
    // 2. High RTT (above _highRttNackMs) - FEC Only: Keep FEC factors.
    //    -1 means always allow NACK.
    // 3. Medium RTT values - Hybrid mode: We will only nack the
    //    residual following the decoding of the FEC (refer to JB logic). FEC
    //    delta protection factor will be adjusted based on the RTT.

    // Otherwise: we count on FEC; if the RTT is below a threshold, then we
    // nack the residual, based on a decision made in the JB.

    // Compute the protection factors
    VCMFecMethod::ProtectionFactor(parameters);
    if (_lowRttNackMs == -1 || parameters->rtt < _lowRttNackMs)
    {
        _protectionFactorD = 0;
        VCMFecMethod::UpdateProtectionFactorD(_protectionFactorD);
    }

    // When in Hybrid mode (RTT range), adjust FEC rates based on the
    // RTT (NACK effectiveness) - adjustment factor is in the range [0,1].
    else if (_highRttNackMs == -1 || parameters->rtt < _highRttNackMs)
    {
        // TODO(mikhal): Disabling adjustment temporarily.
        // WebRtc_UWord16 rttIndex = (WebRtc_UWord16) parameters->rtt;
        float adjustRtt = 1.0f;// (float)VCMNackFecTable[rttIndex] / 100.0f;

        // Adjust FEC with NACK on (for delta frame only)
        // table depends on RTT relative to rttMax (NACK Threshold)
        _protectionFactorD = static_cast<WebRtc_UWord8>
                            (adjustRtt *
                             static_cast<float>(_protectionFactorD));
        // update FEC rates after applying adjustment
        VCMFecMethod::UpdateProtectionFactorD(_protectionFactorD);
    }

    return true;
}

int VCMNackFecMethod::ComputeMaxFramesFec(
    const VCMProtectionParameters* parameters) {
  if (parameters->numLayers > 2) {
    // For more than 2 temporal layers we will only have FEC on the base layer,
    // and the base layers will be pretty far apart. Therefore we force one
    // frame FEC.
    return 1;
  }
  // We set the max number of frames to base the FEC on so that on average
  // we will have complete frames in one RTT. Note that this is an upper
  // bound, and that the actual number of frames used for FEC is decided by the
  // RTP module based on the actual number of packets and the protection factor.
  float base_layer_framerate = parameters->frameRate /
      static_cast<float>(1 << (parameters->numLayers - 1));
  int max_frames_fec = std::max(static_cast<int>(
      2.0f * base_layer_framerate * parameters->rtt /
      1000.0f + 0.5f), 1);
  // |kUpperLimitFramesFec| is the upper limit on how many frames we
  // allow any FEC to be based on.
  if (max_frames_fec > kUpperLimitFramesFec) {
    max_frames_fec = kUpperLimitFramesFec;
  }
  return max_frames_fec;
}

int VCMNackFecMethod::MaxFramesFec() const {
  return _maxFramesFec;
}

bool
VCMNackFecMethod::EffectivePacketLoss(const VCMProtectionParameters* parameters)
{
    // Set the effective packet loss for encoder (based on FEC code).
    // Compute the effective packet loss and residual packet loss due to FEC.
    VCMFecMethod::EffectivePacketLoss(parameters);
    return true;
}

bool
VCMNackFecMethod::UpdateParameters(const VCMProtectionParameters* parameters)
{
    ProtectionFactor(parameters);
    EffectivePacketLoss(parameters);
    _maxFramesFec = ComputeMaxFramesFec(parameters);

    // Efficiency computation is based on FEC and NACK

    // Add FEC cost: ignore I frames for now
    float fecRate = static_cast<float> (_protectionFactorD) / 255.0f;
    _efficiency = parameters->bitRate * fecRate * _corrFecCost;

    // Add NACK cost, when applicable
    if (_highRttNackMs == -1 || parameters->rtt < _highRttNackMs)
    {
        // nackCost  = (bitRate - nackCost) * (lossPr)
        _efficiency += parameters->bitRate * _residualPacketLossFec /
                       (1.0f + _residualPacketLossFec);
    }

    // Protection/fec rates obtained above are defined relative to total number
    // of packets (total rate: source + fec) FEC in RTP module assumes
    // protection factor is defined relative to source number of packets so we
    // should convert the factor to reduce mismatch between mediaOpt's rate and
    // the actual one
    _protectionFactorK = VCMFecMethod::ConvertFECRate(_protectionFactorK);
    _protectionFactorD = VCMFecMethod::ConvertFECRate(_protectionFactorD);

    return true;
}

VCMNackMethod::VCMNackMethod():
VCMProtectionMethod()
{
    _type = kNack;
}

VCMNackMethod::~VCMNackMethod()
{
    //
}

bool
VCMNackMethod::EffectivePacketLoss(const VCMProtectionParameters* parameter)
{
    // Effective Packet Loss, NA in current version.
    _effectivePacketLoss = 0;
    return true;
}

bool
VCMNackMethod::UpdateParameters(const VCMProtectionParameters* parameters)
{
    // Compute the effective packet loss
    EffectivePacketLoss(parameters);

    // nackCost  = (bitRate - nackCost) * (lossPr)
    _efficiency = parameters->bitRate * parameters->lossPr /
                  (1.0f + parameters->lossPr);
    return true;
}

VCMFecMethod::VCMFecMethod():
VCMProtectionMethod()
{
    _type = kFec;
}
VCMFecMethod::~VCMFecMethod()
{
    //
}

WebRtc_UWord8
VCMFecMethod::BoostCodeRateKey(WebRtc_UWord8 packetFrameDelta,
                               WebRtc_UWord8 packetFrameKey) const
{
    WebRtc_UWord8 boostRateKey = 2;
    // Default: ratio scales the FEC protection up for I frames
    WebRtc_UWord8 ratio = 1;

    if (packetFrameDelta > 0)
    {
        ratio = (WebRtc_Word8) (packetFrameKey / packetFrameDelta);
    }
    ratio = VCM_MAX(boostRateKey, ratio);

    return ratio;
}

WebRtc_UWord8
VCMFecMethod::ConvertFECRate(WebRtc_UWord8 codeRateRTP) const
{
    return static_cast<WebRtc_UWord8> (VCM_MIN(255,(0.5 + 255.0 * codeRateRTP /
                                      (float)(255 - codeRateRTP))));
}

// Update FEC with protectionFactorD
void
VCMFecMethod::UpdateProtectionFactorD(WebRtc_UWord8 protectionFactorD)
{
    _protectionFactorD = protectionFactorD;
}

// Update FEC with protectionFactorK
void
VCMFecMethod::UpdateProtectionFactorK(WebRtc_UWord8 protectionFactorK)
{
    _protectionFactorK = protectionFactorK;
}

// AvgRecoveryFEC: computes the residual packet loss (RPL) function.
// This is the average recovery from the FEC, assuming random packet loss model.
// Computed off-line for a range of FEC code parameters and loss rates.
float
VCMFecMethod::AvgRecoveryFEC(const VCMProtectionParameters* parameters) const
{
    // Total (avg) bits available per frame: total rate over actual/sent frame
    // rate units are kbits/frame
    const WebRtc_UWord16 bitRatePerFrame = static_cast<WebRtc_UWord16>
                        (parameters->bitRate / (parameters->frameRate));

    // Total (average) number of packets per frame (source and fec):
    const WebRtc_UWord8 avgTotPackets = 1 + static_cast<WebRtc_UWord8>
                        (static_cast<float> (bitRatePerFrame * 1000.0) /
                         static_cast<float> (8.0 * _maxPayloadSize) + 0.5);

    const float protectionFactor = static_cast<float>(_protectionFactorD) /
                                                      255.0;

    WebRtc_UWord8 fecPacketsPerFrame = static_cast<WebRtc_UWord8>
                                      (0.5 + protectionFactor * avgTotPackets);

    WebRtc_UWord8 sourcePacketsPerFrame = avgTotPackets - fecPacketsPerFrame;

    if ( (fecPacketsPerFrame == 0) || (sourcePacketsPerFrame == 0) )
    {
        // No protection, or rate too low: so average recovery from FEC == 0.
        return 0.0;
    }

    // Table defined up to kMaxNumPackets
    if (sourcePacketsPerFrame > kMaxNumPackets)
    {
        sourcePacketsPerFrame = kMaxNumPackets;
    }

    // Table defined up to kMaxNumPackets
    if (fecPacketsPerFrame > kMaxNumPackets)
    {
        fecPacketsPerFrame = kMaxNumPackets;
    }

    // Code index for tables: up to (kMaxNumPackets * kMaxNumPackets)
    WebRtc_UWord16 codeIndexTable[kMaxNumPackets * kMaxNumPackets];
    WebRtc_UWord16 k = 0;
    for (WebRtc_UWord8 i = 1; i <= kMaxNumPackets; i++)
    {
        for (WebRtc_UWord8 j = 1; j <= i; j++)
        {
            codeIndexTable[(j - 1) * kMaxNumPackets + i - 1] = k;
            k += 1;
        }
    }

    WebRtc_UWord8 lossRate = static_cast<WebRtc_UWord8> (255.0 *
                             parameters->lossPr + 0.5f);

    // Constrain lossRate to 50%: tables defined up to 50%
    if (lossRate >= kPacketLossMax)
    {
        lossRate = kPacketLossMax - 1;
    }

    const WebRtc_UWord16 codeIndex = (fecPacketsPerFrame - 1) * kMaxNumPackets +
                                     (sourcePacketsPerFrame - 1);

    const WebRtc_UWord16 indexTable = codeIndexTable[codeIndex] * kPacketLossMax +
                                      lossRate;

    // Check on table index
    assert(indexTable < kSizeAvgFECRecoveryXOR);
    float avgFecRecov = static_cast<float>(kAvgFECRecoveryXOR[indexTable]);

    return avgFecRecov;
}

bool
VCMFecMethod::ProtectionFactor(const VCMProtectionParameters* parameters)
{
    // FEC PROTECTION SETTINGS: varies with packet loss and bitrate

    // No protection if (filtered) packetLoss is 0
    WebRtc_UWord8 packetLoss = (WebRtc_UWord8) (255 * parameters->lossPr);
    if (packetLoss == 0)
    {
        _protectionFactorK = 0;
        _protectionFactorD = 0;
         return true;
    }

    // Parameters for FEC setting:
    // first partition size, thresholds, table pars, spatial resoln fac.

    // First partition protection: ~ 20%
    WebRtc_UWord8 firstPartitionProt = (WebRtc_UWord8) (255 * 0.20);

    // Minimum protection level needed to generate one FEC packet for one
    // source packet/frame (in RTP sender)
    WebRtc_UWord8 minProtLevelFec = 85;

    // Threshold on packetLoss and bitRrate/frameRate (=average #packets),
    // above which we allocate protection to cover at least first partition.
    WebRtc_UWord8 lossThr = 0;
    WebRtc_UWord8 packetNumThr = 1;

    // Parameters for range of rate index of table.
    const WebRtc_UWord8 ratePar1 = 5;
    const WebRtc_UWord8 ratePar2 = 49;

    // Spatial resolution size, relative to a reference size.
    float spatialSizeToRef = static_cast<float>
                           (parameters->codecWidth * parameters->codecHeight) /
                           (static_cast<float>(704 * 576));
    // resolnFac: This parameter will generally increase/decrease the FEC rate
    // (for fixed bitRate and packetLoss) based on system size.
    // Use a smaller exponent (< 1) to control/soften system size effect.
    const float resolnFac = 1.0 / powf(spatialSizeToRef, 0.3f);

    const int bitRatePerFrame = BitsPerFrame(parameters);


    // Average number of packets per frame (source and fec):
    const WebRtc_UWord8 avgTotPackets = 1 + (WebRtc_UWord8)
                                        ((float) bitRatePerFrame * 1000.0
                                       / (float) (8.0 * _maxPayloadSize) + 0.5);

    // FEC rate parameters: for P and I frame
    WebRtc_UWord8 codeRateDelta = 0;
    WebRtc_UWord8 codeRateKey = 0;

    // Get index for table: the FEC protection depends on an effective rate.
    // The range on the rate index corresponds to rates (bps)
    // from ~200k to ~8000k, for 30fps
    const WebRtc_UWord16 effRateFecTable = static_cast<WebRtc_UWord16>
                                           (resolnFac * bitRatePerFrame);
    WebRtc_UWord8 rateIndexTable =
        (WebRtc_UWord8) VCM_MAX(VCM_MIN((effRateFecTable - ratePar1) /
                                         ratePar1, ratePar2), 0);

    // Restrict packet loss range to 50:
    // current tables defined only up to 50%
    if (packetLoss >= kPacketLossMax)
    {
        packetLoss = kPacketLossMax - 1;
    }
    WebRtc_UWord16 indexTable = rateIndexTable * kPacketLossMax + packetLoss;

    // Check on table index
    assert(indexTable < kSizeCodeRateXORTable);

    // Protection factor for P frame
    codeRateDelta = kCodeRateXORTable[indexTable];

    if (packetLoss > lossThr && avgTotPackets > packetNumThr)
    {
        // Set a minimum based on first partition size.
        if (codeRateDelta < firstPartitionProt)
        {
            codeRateDelta = firstPartitionProt;
        }
    }

    // Check limit on amount of protection for P frame; 50% is max.
    if (codeRateDelta >= kPacketLossMax)
    {
        codeRateDelta = kPacketLossMax - 1;
    }

    float adjustFec = 1.0f;
    // Avoid additional adjustments when layers are active.
    // TODO(mikhal/marco): Update adjusmtent based on layer info.
    if (parameters->numLayers == 1)
    {
        adjustFec = _qmRobustness->AdjustFecFactor(codeRateDelta,
                                                   parameters->bitRate,
                                                   parameters->frameRate,
                                                   parameters->rtt,
                                                   packetLoss);
    }

    codeRateDelta = static_cast<WebRtc_UWord8>(codeRateDelta * adjustFec);

    // For Key frame:
    // Effectively at a higher rate, so we scale/boost the rate
    // The boost factor may depend on several factors: ratio of packet
    // number of I to P frames, how much protection placed on P frames, etc.
    const WebRtc_UWord8 packetFrameDelta = (WebRtc_UWord8)
                                           (0.5 + parameters->packetsPerFrame);
    const WebRtc_UWord8 packetFrameKey = (WebRtc_UWord8)
                                         (0.5 + parameters->packetsPerFrameKey);
    const WebRtc_UWord8 boostKey = BoostCodeRateKey(packetFrameDelta,
                                                    packetFrameKey);

    rateIndexTable = (WebRtc_UWord8) VCM_MAX(VCM_MIN(
                      1 + (boostKey * effRateFecTable - ratePar1) /
                      ratePar1,ratePar2),0);
    WebRtc_UWord16 indexTableKey = rateIndexTable * kPacketLossMax + packetLoss;

    indexTableKey = VCM_MIN(indexTableKey, kSizeCodeRateXORTable);

    // Check on table index
    assert(indexTableKey < kSizeCodeRateXORTable);

    // Protection factor for I frame
    codeRateKey = kCodeRateXORTable[indexTableKey];

    // Boosting for Key frame.
    int boostKeyProt = _scaleProtKey * codeRateDelta;
    if (boostKeyProt >= kPacketLossMax)
    {
        boostKeyProt = kPacketLossMax - 1;
    }

    // Make sure I frame protection is at least larger than P frame protection,
    // and at least as high as filtered packet loss.
    codeRateKey = static_cast<WebRtc_UWord8> (VCM_MAX(packetLoss,
            VCM_MAX(boostKeyProt, codeRateKey)));

    // Check limit on amount of protection for I frame: 50% is max.
    if (codeRateKey >= kPacketLossMax)
    {
        codeRateKey = kPacketLossMax - 1;
    }

    _protectionFactorK = codeRateKey;
    _protectionFactorD = codeRateDelta;

    // Generally there is a rate mis-match between the FEC cost estimated
    // in mediaOpt and the actual FEC cost sent out in RTP module.
    // This is more significant at low rates (small # of source packets), where
    // the granularity of the FEC decreases. In this case, non-zero protection
    // in mediaOpt may generate 0 FEC packets in RTP sender (since actual #FEC
    // is based on rounding off protectionFactor on actual source packet number).
    // The correction factor (_corrFecCost) attempts to corrects this, at least
    // for cases of low rates (small #packets) and low protection levels.

    float numPacketsFl = 1.0f + ((float) bitRatePerFrame * 1000.0
                                / (float) (8.0 * _maxPayloadSize) + 0.5);

    const float estNumFecGen = 0.5f + static_cast<float> (_protectionFactorD *
                                                         numPacketsFl / 255.0f);


    // We reduce cost factor (which will reduce overhead for FEC and
    // hybrid method) and not the protectionFactor.
    _corrFecCost = 1.0f;
    if (estNumFecGen < 1.1f && _protectionFactorD < minProtLevelFec)
    {
        _corrFecCost = 0.5f;
    }
    if (estNumFecGen < 0.9f && _protectionFactorD < minProtLevelFec)
    {
        _corrFecCost = 0.0f;
    }

     // TODO (marpan): Set the UEP protection on/off for Key and Delta frames
    _useUepProtectionK = _qmRobustness->SetUepProtection(codeRateKey,
                                                         parameters->bitRate,
                                                         packetLoss,
                                                         0);

    _useUepProtectionD = _qmRobustness->SetUepProtection(codeRateDelta,
                                                         parameters->bitRate,
                                                         packetLoss,
                                                         1);

    // DONE WITH FEC PROTECTION SETTINGS
    return true;
}

int VCMFecMethod::BitsPerFrame(const VCMProtectionParameters* parameters) {
  // When temporal layers are available FEC will only be applied on the base
  // layer.
  const float bitRateRatio =
    kVp8LayerRateAlloction[parameters->numLayers - 1][0];
  float frameRateRatio = powf(1 / 2, parameters->numLayers - 1);
  float bitRate = parameters->bitRate * bitRateRatio;
  float frameRate = parameters->frameRate * frameRateRatio;

  // TODO(mikhal): Update factor following testing.
  float adjustmentFactor = 1;

  // Average bits per frame (units of kbits)
  return static_cast<int>(adjustmentFactor * bitRate / frameRate);
}

bool
VCMFecMethod::EffectivePacketLoss(const VCMProtectionParameters* parameters)
{
    // Effective packet loss to encoder is based on RPL (residual packet loss)
    // this is a soft setting based on degree of FEC protection
    // RPL = received/input packet loss - average_FEC_recovery
    // note: received/input packet loss may be filtered based on FilteredLoss

    // The packet loss:
    WebRtc_UWord8 packetLoss = (WebRtc_UWord8) (255 * parameters->lossPr);

    float avgFecRecov = AvgRecoveryFEC(parameters);

    // Residual Packet Loss:
    _residualPacketLossFec = (float) (packetLoss - avgFecRecov) / 255.0f;

    // Effective Packet Loss, NA in current version.
    _effectivePacketLoss = 0;

    return true;
}

bool
VCMFecMethod::UpdateParameters(const VCMProtectionParameters* parameters)
{
    // Compute the protection factor
    ProtectionFactor(parameters);

    // Compute the effective packet loss
    EffectivePacketLoss(parameters);

    // Compute the bit cost
    // Ignore key frames for now.
    float fecRate = static_cast<float> (_protectionFactorD) / 255.0f;
    if (fecRate >= 0.0f)
    {
        // use this formula if the fecRate (protection factor) is defined
        // relative to number of source packets
        // this is the case for the previous tables:
        // _efficiency = parameters->bitRate * ( 1.0 - 1.0 / (1.0 + fecRate));

        // in the new tables, the fecRate is defined relative to total number of
        // packets (total rate), so overhead cost is:
        _efficiency = parameters->bitRate * fecRate * _corrFecCost;
    }
    else
    {
        _efficiency = 0.0f;
    }

    // Protection/fec rates obtained above is defined relative to total number
    // of packets (total rate: source+fec) FEC in RTP module assumes protection
    // factor is defined relative to source number of packets so we should
    // convert the factor to reduce mismatch between mediaOpt suggested rate and
    // the actual rate
    _protectionFactorK = ConvertFECRate(_protectionFactorK);
    _protectionFactorD = ConvertFECRate(_protectionFactorD);

    return true;
}
VCMLossProtectionLogic::VCMLossProtectionLogic(int64_t nowMs):
_selectedMethod(NULL),
_currentParameters(),
_rtt(0),
_lossPr(0.0f),
_bitRate(0.0f),
_frameRate(0.0f),
_keyFrameSize(0.0f),
_fecRateKey(0),
_fecRateDelta(0),
_lastPrUpdateT(0),
_lossPr255(0.9999f),
_lossPrHistory(),
_shortMaxLossPr255(0),
_packetsPerFrame(0.9999f),
_packetsPerFrameKey(0.9999f),
_residualPacketLossFec(0),
_boostRateKey(2),
_codecWidth(0),
_codecHeight(0),
_numLayers(1)
{
    Reset(nowMs);
}

VCMLossProtectionLogic::~VCMLossProtectionLogic()
{
    Release();
}

bool
VCMLossProtectionLogic::SetMethod(enum VCMProtectionMethodEnum newMethodType)
{
    if (_selectedMethod != NULL)
    {
        if (_selectedMethod->Type() == newMethodType)
        {
            // Nothing to update
            return false;
        }
        // New method - delete existing one
        delete _selectedMethod;
    }
    VCMProtectionMethod *newMethod = NULL;
    switch (newMethodType)
    {
        case kNack:
        {
            newMethod = new VCMNackMethod();
            break;
        }
        case kFec:
        {
            newMethod  = new VCMFecMethod();
            break;
        }
        case kNackFec:
        {
            // Default to always having NACK enabled for the hybrid mode.
            newMethod =  new VCMNackFecMethod(kLowRttNackMs, -1);
            break;
        }
        default:
        {
          return false;
          break;
        }

    }
    _selectedMethod = newMethod;
    return true;
}
bool
VCMLossProtectionLogic::RemoveMethod(enum VCMProtectionMethodEnum method)
{
    if (_selectedMethod == NULL)
    {
        return false;
    }
    else if (_selectedMethod->Type() == method)
    {
        delete _selectedMethod;
        _selectedMethod = NULL;
    }
    return true;
}

float
VCMLossProtectionLogic::RequiredBitRate() const
{
    float RequiredBitRate = 0.0f;
    if (_selectedMethod != NULL)
    {
        RequiredBitRate = _selectedMethod->RequiredBitRate();
    }
    return RequiredBitRate;
}

void
VCMLossProtectionLogic::UpdateRtt(WebRtc_UWord32 rtt)
{
    _rtt = rtt;
}

void
VCMLossProtectionLogic::UpdateResidualPacketLoss(float residualPacketLoss)
{
    _residualPacketLossFec = residualPacketLoss;
}

void
VCMLossProtectionLogic::UpdateMaxLossHistory(WebRtc_UWord8 lossPr255,
                                             WebRtc_Word64 now)
{
    if (_lossPrHistory[0].timeMs >= 0 &&
        now - _lossPrHistory[0].timeMs < kLossPrShortFilterWinMs)
    {
        if (lossPr255 > _shortMaxLossPr255)
        {
            _shortMaxLossPr255 = lossPr255;
        }
    }
    else
    {
        // Only add a new value to the history once a second
        if (_lossPrHistory[0].timeMs == -1)
        {
            // First, no shift
            _shortMaxLossPr255 = lossPr255;
        }
        else
        {
            // Shift
            for (WebRtc_Word32 i = (kLossPrHistorySize - 2); i >= 0; i--)
            {
                _lossPrHistory[i + 1].lossPr255 = _lossPrHistory[i].lossPr255;
                _lossPrHistory[i + 1].timeMs = _lossPrHistory[i].timeMs;
            }
        }
        if (_shortMaxLossPr255 == 0)
        {
            _shortMaxLossPr255 = lossPr255;
        }

        _lossPrHistory[0].lossPr255 = _shortMaxLossPr255;
        _lossPrHistory[0].timeMs = now;
        _shortMaxLossPr255 = 0;
    }
}

WebRtc_UWord8
VCMLossProtectionLogic::MaxFilteredLossPr(WebRtc_Word64 nowMs) const
{
    WebRtc_UWord8 maxFound = _shortMaxLossPr255;
    if (_lossPrHistory[0].timeMs == -1)
    {
        return maxFound;
    }
    for (WebRtc_Word32 i = 0; i < kLossPrHistorySize; i++)
    {
        if (_lossPrHistory[i].timeMs == -1)
        {
            break;
        }
        if (nowMs - _lossPrHistory[i].timeMs >
            kLossPrHistorySize * kLossPrShortFilterWinMs)
        {
            // This sample (and all samples after this) is too old
            break;
        }
        if (_lossPrHistory[i].lossPr255 > maxFound)
        {
            // This sample is the largest one this far into the history
            maxFound = _lossPrHistory[i].lossPr255;
        }
    }
    return maxFound;
}

WebRtc_UWord8 VCMLossProtectionLogic::FilteredLoss(
    int64_t nowMs,
    FilterPacketLossMode filter_mode,
    WebRtc_UWord8 lossPr255) {

  // Update the max window filter.
  UpdateMaxLossHistory(lossPr255, nowMs);

  // Update the recursive average filter.
  _lossPr255.Apply(static_cast<float> (nowMs - _lastPrUpdateT),
                   static_cast<float> (lossPr255));
  _lastPrUpdateT = nowMs;

  // Filtered loss: default is received loss (no filtering).
  WebRtc_UWord8 filtered_loss = lossPr255;

  switch (filter_mode) {
    case kNoFilter:
      break;
    case kAvgFilter:
      filtered_loss = static_cast<WebRtc_UWord8> (_lossPr255.Value() + 0.5);
      break;
    case kMaxFilter:
      filtered_loss = MaxFilteredLossPr(nowMs);
      break;
  }

  return filtered_loss;
}

void
VCMLossProtectionLogic::UpdateFilteredLossPr(WebRtc_UWord8 packetLossEnc)
{
    _lossPr = (float) packetLossEnc / (float) 255.0;
}

void
VCMLossProtectionLogic::UpdateBitRate(float bitRate)
{
    _bitRate = bitRate;
}

void
VCMLossProtectionLogic::UpdatePacketsPerFrame(float nPackets, int64_t nowMs)
{
    _packetsPerFrame.Apply(static_cast<float>(nowMs - _lastPacketPerFrameUpdateT),
                           nPackets);
    _lastPacketPerFrameUpdateT = nowMs;
}

void
VCMLossProtectionLogic::UpdatePacketsPerFrameKey(float nPackets, int64_t nowMs)
{
    _packetsPerFrameKey.Apply(static_cast<float>(nowMs -
                              _lastPacketPerFrameUpdateTKey), nPackets);
    _lastPacketPerFrameUpdateTKey = nowMs;
}

void
VCMLossProtectionLogic::UpdateKeyFrameSize(float keyFrameSize)
{
    _keyFrameSize = keyFrameSize;
}

void
VCMLossProtectionLogic::UpdateFrameSize(WebRtc_UWord16 width,
                                        WebRtc_UWord16 height)
{
    _codecWidth = width;
    _codecHeight = height;
}

void VCMLossProtectionLogic::UpdateNumLayers(int numLayers) {
  _numLayers = (numLayers == 0) ? 1 : numLayers;
}

bool
VCMLossProtectionLogic::UpdateMethod()
{
    if (_selectedMethod == NULL)
    {
        return false;
    }
    _currentParameters.rtt = _rtt;
    _currentParameters.lossPr = _lossPr;
    _currentParameters.bitRate = _bitRate;
    _currentParameters.frameRate = _frameRate; // rename actual frame rate?
    _currentParameters.keyFrameSize = _keyFrameSize;
    _currentParameters.fecRateDelta = _fecRateDelta;
    _currentParameters.fecRateKey = _fecRateKey;
    _currentParameters.packetsPerFrame = _packetsPerFrame.Value();
    _currentParameters.packetsPerFrameKey = _packetsPerFrameKey.Value();
    _currentParameters.residualPacketLossFec = _residualPacketLossFec;
    _currentParameters.codecWidth = _codecWidth;
    _currentParameters.codecHeight = _codecHeight;
    _currentParameters.numLayers = _numLayers;
    return _selectedMethod->UpdateParameters(&_currentParameters);
}

VCMProtectionMethod*
VCMLossProtectionLogic::SelectedMethod() const
{
    return _selectedMethod;
}

VCMProtectionMethodEnum
VCMLossProtectionLogic::SelectedType() const
{
    return _selectedMethod->Type();
}

void
VCMLossProtectionLogic::Reset(int64_t nowMs)
{
    _lastPrUpdateT = nowMs;
    _lastPacketPerFrameUpdateT = nowMs;
    _lastPacketPerFrameUpdateTKey = nowMs;
    _lossPr255.Reset(0.9999f);
    _packetsPerFrame.Reset(0.9999f);
    _fecRateDelta = _fecRateKey = 0;
    for (WebRtc_Word32 i = 0; i < kLossPrHistorySize; i++)
    {
        _lossPrHistory[i].lossPr255 = 0;
        _lossPrHistory[i].timeMs = -1;
    }
    _shortMaxLossPr255 = 0;
    Release();
}

void
VCMLossProtectionLogic::Release()
{
    delete _selectedMethod;
    _selectedMethod = NULL;
}

}
