/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtcp_receiver_help.h"

#include <string.h>  // memset
#include <cassert>  // assert

#include "modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {
using namespace RTCPHelp;

RTCPPacketInformation::RTCPPacketInformation()
    : rtcpPacketTypeFlags(0),
      remoteSSRC(0),
      nackSequenceNumbers(0),
      nackSequenceNumbersLength(0),
      applicationSubType(0),
      applicationName(0),
      applicationData(),
      applicationLength(0),
      reportBlock(false),
      fractionLost(0),
      roundTripTime(0),
      lastReceivedExtendedHighSeqNum(0),
      jitter(0),
      interArrivalJitter(0),
      sliPictureId(0),
      rpsiPictureId(0),
      receiverEstimatedMaxBitrate(0),
      ntp_secs(0),
      ntp_frac(0),
      rtp_timestamp(0),
      VoIPMetric(NULL) {
}

RTCPPacketInformation::~RTCPPacketInformation()
{
    delete [] nackSequenceNumbers;
    delete [] applicationData;
    delete VoIPMetric;
}

void
RTCPPacketInformation::AddVoIPMetric(const RTCPVoIPMetric* metric)
{
    VoIPMetric = new RTCPVoIPMetric();
    memcpy(VoIPMetric, metric, sizeof(RTCPVoIPMetric));
}

void RTCPPacketInformation::AddApplicationData(const WebRtc_UWord8* data,
                                               const WebRtc_UWord16 size) {
    WebRtc_UWord8* oldData = applicationData;
    WebRtc_UWord16 oldLength = applicationLength;

    // Don't copy more than kRtcpAppCode_DATA_SIZE bytes.
    WebRtc_UWord16 copySize = size;
    if (size > kRtcpAppCode_DATA_SIZE) {
        copySize = kRtcpAppCode_DATA_SIZE;
    }

    applicationLength += copySize;
    applicationData = new WebRtc_UWord8[applicationLength];

    if (oldData)
    {
        memcpy(applicationData, oldData, oldLength);
        memcpy(applicationData+oldLength, data, copySize);
        delete [] oldData;
    } else
    {
        memcpy(applicationData, data, copySize);
    }
}

void
RTCPPacketInformation::ResetNACKPacketIdArray()
{
    if (NULL == nackSequenceNumbers)
    {
        nackSequenceNumbers = new WebRtc_UWord16[NACK_PACKETS_MAX_SIZE];
    }
    nackSequenceNumbersLength = 0;
}

void
RTCPPacketInformation::AddNACKPacket(const WebRtc_UWord16 packetID)
{
    assert(nackSequenceNumbers);

    WebRtc_UWord16& idx = nackSequenceNumbersLength;
    if (idx < NACK_PACKETS_MAX_SIZE)
    {
        nackSequenceNumbers[idx++] = packetID;
    }
}

void
RTCPPacketInformation::AddReportInfo(const WebRtc_UWord8 fraction,
                                     const WebRtc_UWord16 rtt,
                                     const WebRtc_UWord32 extendedHighSeqNum,
                                     const WebRtc_UWord32 j)
{
    reportBlock = true;
    fractionLost = fraction;
    roundTripTime = rtt;
    jitter = j;
    lastReceivedExtendedHighSeqNum = extendedHighSeqNum;
}

RTCPReportBlockInformation::RTCPReportBlockInformation():
    remoteReceiveBlock(),
    remoteMaxJitter(0),
    RTT(0),
    minRTT(0),
    maxRTT(0),
    avgRTT(0),
    numAverageCalcs(0)
{
    memset(&remoteReceiveBlock,0,sizeof(remoteReceiveBlock));
}

RTCPReportBlockInformation::~RTCPReportBlockInformation()
{
}

RTCPReceiveInformation::RTCPReceiveInformation()
    : lastTimeReceived(0),
      lastFIRSequenceNumber(-1),
      lastFIRRequest(0),
      readyForDelete(false) {
}

RTCPReceiveInformation::~RTCPReceiveInformation() {
}

// Increase size of TMMBRSet if needed, and also take care of
// the _tmmbrSetTimeouts vector.
void RTCPReceiveInformation::VerifyAndAllocateTMMBRSet(
    const WebRtc_UWord32 minimumSize) {
  if (minimumSize > TmmbrSet.sizeOfSet()) {
    TmmbrSet.VerifyAndAllocateSetKeepingData(minimumSize);
    // make sure that our buffers are big enough
    _tmmbrSetTimeouts.reserve(minimumSize);
  }
}

void RTCPReceiveInformation::InsertTMMBRItem(
    const WebRtc_UWord32 senderSSRC,
    const RTCPUtility::RTCPPacketRTPFBTMMBRItem& TMMBRItem,
    const WebRtc_Word64 currentTimeMS) {
  // serach to see if we have it in our list
  for (WebRtc_UWord32 i = 0; i < TmmbrSet.lengthOfSet(); i++)  {
    if (TmmbrSet.Ssrc(i) == senderSSRC) {
      // we already have this SSRC in our list update it
      TmmbrSet.SetEntry(i,
                        TMMBRItem.MaxTotalMediaBitRate,
                        TMMBRItem.MeasuredOverhead,
                        senderSSRC);
      _tmmbrSetTimeouts[i] = currentTimeMS;
      return;
    }
  }
  VerifyAndAllocateTMMBRSet(TmmbrSet.lengthOfSet() + 1);
  TmmbrSet.AddEntry(TMMBRItem.MaxTotalMediaBitRate,
                    TMMBRItem.MeasuredOverhead,
                    senderSSRC);
  _tmmbrSetTimeouts.push_back(currentTimeMS);
}

WebRtc_Word32 RTCPReceiveInformation::GetTMMBRSet(
    const WebRtc_UWord32 sourceIdx,
    const WebRtc_UWord32 targetIdx,
    TMMBRSet* candidateSet,
    const WebRtc_Word64 currentTimeMS) {
  if (sourceIdx >= TmmbrSet.lengthOfSet()) {
    return -1;
  }
  if (targetIdx >= candidateSet->sizeOfSet()) {
    return -1;
  }
  // use audio define since we don't know what interval the remote peer is using
  if (currentTimeMS - _tmmbrSetTimeouts[sourceIdx] >
      5 * RTCP_INTERVAL_AUDIO_MS) {
    // value timed out
    TmmbrSet.RemoveEntry(sourceIdx);
    _tmmbrSetTimeouts.erase(_tmmbrSetTimeouts.begin() + sourceIdx);
    return -1;
  }
  candidateSet->SetEntry(targetIdx,
                         TmmbrSet.Tmmbr(sourceIdx),
                         TmmbrSet.PacketOH(sourceIdx),
                         TmmbrSet.Ssrc(sourceIdx));
  return 0;
}

void RTCPReceiveInformation::VerifyAndAllocateBoundingSet(
    const WebRtc_UWord32 minimumSize) {
  TmmbnBoundingSet.VerifyAndAllocateSet(minimumSize);
}
} // namespace webrtc
