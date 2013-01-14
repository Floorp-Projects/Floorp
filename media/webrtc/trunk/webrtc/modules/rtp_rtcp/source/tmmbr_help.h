/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_TMMBR_HELP_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_TMMBR_HELP_H_

#include <vector>
#include "typedefs.h"

#include "critical_section_wrapper.h"

#ifndef NULL
    #define NULL    0
#endif

namespace webrtc {
class TMMBRSet
{
public:
    TMMBRSet();
    ~TMMBRSet();

    void VerifyAndAllocateSet(WebRtc_UWord32 minimumSize);
    void VerifyAndAllocateSetKeepingData(WebRtc_UWord32 minimumSize);
    // Number of valid data items in set.
    WebRtc_UWord32 lengthOfSet() const { return _lengthOfSet; }
    // Presently allocated max size of set.
    WebRtc_UWord32 sizeOfSet() const { return _sizeOfSet; }
    void clearSet() {
      _lengthOfSet = 0;
    }
    WebRtc_UWord32 Tmmbr(int i) const {
      return _data.at(i).tmmbr;
    }
    WebRtc_UWord32 PacketOH(int i) const {
      return _data.at(i).packet_oh;
    }
    WebRtc_UWord32 Ssrc(int i) const {
      return _data.at(i).ssrc;
    }
    void SetEntry(unsigned int i,
                  WebRtc_UWord32 tmmbrSet,
                  WebRtc_UWord32 packetOHSet,
                  WebRtc_UWord32 ssrcSet);

    void AddEntry(WebRtc_UWord32 tmmbrSet,
                  WebRtc_UWord32 packetOHSet,
                  WebRtc_UWord32 ssrcSet);

    // Remove one entry from table, and move all others down.
    void RemoveEntry(WebRtc_UWord32 sourceIdx);

    void SwapEntries(WebRtc_UWord32 firstIdx,
                     WebRtc_UWord32 secondIdx);

    // Set entry data to zero, but keep it in table.
    void ClearEntry(WebRtc_UWord32 idx);

 private:
    class SetElement {
      public:
        SetElement() : tmmbr(0), packet_oh(0), ssrc(0) {}
        WebRtc_UWord32 tmmbr;
        WebRtc_UWord32 packet_oh;
        WebRtc_UWord32 ssrc;
    };

    std::vector<SetElement> _data;
    // Number of places allocated.
    WebRtc_UWord32    _sizeOfSet;
    // NUmber of places currently in use.
    WebRtc_UWord32    _lengthOfSet;
};

class TMMBRHelp
{
public:
    TMMBRHelp();
    virtual ~TMMBRHelp();

    TMMBRSet* BoundingSet(); // used for debuging
    TMMBRSet* CandidateSet();
    TMMBRSet* BoundingSetToSend();

    TMMBRSet* VerifyAndAllocateCandidateSet(const WebRtc_UWord32 minimumSize);
    WebRtc_Word32 FindTMMBRBoundingSet(TMMBRSet*& boundingSet);
    WebRtc_Word32 SetTMMBRBoundingSetToSend(
        const TMMBRSet* boundingSetToSend,
        const WebRtc_UWord32 maxBitrateKbit);

    bool IsOwner(const WebRtc_UWord32 ssrc, const WebRtc_UWord32 length) const;

    bool CalcMinBitRate(WebRtc_UWord32* minBitrateKbit) const;

protected:
    TMMBRSet*   VerifyAndAllocateBoundingSet(WebRtc_UWord32 minimumSize);
    WebRtc_Word32 VerifyAndAllocateBoundingSetToSend(WebRtc_UWord32 minimumSize);

    WebRtc_Word32 FindTMMBRBoundingSet(WebRtc_Word32 numCandidates, TMMBRSet& candidateSet);

private:
    CriticalSectionWrapper* _criticalSection;
    TMMBRSet                _candidateSet;
    TMMBRSet                _boundingSet;
    TMMBRSet                _boundingSetToSend;

    float*                  _ptrIntersectionBoundingSet;
    float*                  _ptrMaxPRBoundingSet;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_TMMBR_HELP_H_
