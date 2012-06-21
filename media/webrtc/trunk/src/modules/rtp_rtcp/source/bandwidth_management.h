/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_BANDWIDTH_MANAGEMENT_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_BANDWIDTH_MANAGEMENT_H_

#include "typedefs.h"
#include "rtp_rtcp_config.h"
#include "critical_section_wrapper.h"

/*
*   FEC and NACK added bitrate is handled outside class
*/

namespace webrtc {
class BandwidthManagement
{
public:
    BandwidthManagement(const WebRtc_Word32 id);
    ~BandwidthManagement();

    // Call when we receive a RTCP message with TMMBR or REMB
    WebRtc_Word32 UpdateBandwidthEstimate(const WebRtc_UWord16 bandWidthKbit,
                                          WebRtc_UWord32* newBitrate,
                                          WebRtc_UWord8* fractionLost,
                                          WebRtc_UWord16* roundTripTime);

   // Call when we receive a RTCP message with a ReceiveBlock
    WebRtc_Word32 UpdatePacketLoss(
        const WebRtc_UWord32 lastReceivedExtendedHighSeqNum,
        WebRtc_UWord32 sentBitrate,
        const WebRtc_UWord16 rtt,
        WebRtc_UWord8* loss,
        WebRtc_UWord32* newBitrate,
        WebRtc_Word64 nowMS);

    // If no bandwidth estimate is available or if |bandwidthKbit| is NULL,
    // -1 is returned.
    WebRtc_Word32 AvailableBandwidth(WebRtc_UWord32* bandwidthKbit) const;

    void SetSendBitrate(const WebRtc_UWord32 startBitrate,
                        const WebRtc_UWord16 minBitrateKbit,
                        const WebRtc_UWord16 maxBitrateKbit);

    WebRtc_Word32 MaxConfiguredBitrate(WebRtc_UWord16* maxBitrateKbit);

protected:
    WebRtc_UWord32 ShapeSimple(WebRtc_Word32 packetLoss,
                               WebRtc_Word32 rtt,
                               WebRtc_UWord32 sentBitrate,
                               WebRtc_Word64 nowMS);

    WebRtc_Word32 CalcTFRCbps(WebRtc_Word16 avgPackSizeBytes,
                              WebRtc_Word32 rttMs,
                              WebRtc_Word32 packetLoss);

private:
    enum { kBWEUpdateIntervalMs = 1000 };

    WebRtc_Word32         _id;

    CriticalSectionWrapper* _critsect;

    // incoming filters
    WebRtc_UWord32        _lastPacketLossExtendedHighSeqNum;
    bool                  _lastReportAllLost;
    WebRtc_UWord8         _lastLoss;
    int                   _accumulateLostPacketsQ8;
    int                   _accumulateExpectedPackets;

    // bitrate
    WebRtc_UWord32        _bitRate;
    WebRtc_UWord32        _minBitRateConfigured;
    WebRtc_UWord32        _maxBitRateConfigured;

    WebRtc_UWord8         _last_fraction_loss;
    WebRtc_UWord16        _last_round_trip_time;

    // bandwidth estimate
    WebRtc_UWord32        _bwEstimateIncoming;
    WebRtc_Word64         _timeLastIncrease;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_BANDWIDTH_MANAGEMENT_H_
