/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/tmmbr_help.h"

#include <assert.h>
#include <limits>
#include <string.h>
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"

namespace webrtc {
TMMBRSet::TMMBRSet() :
    _sizeOfSet(0),
    _lengthOfSet(0)
{
}

TMMBRSet::~TMMBRSet()
{
    _sizeOfSet = 0;
    _lengthOfSet = 0;
}

void
TMMBRSet::VerifyAndAllocateSet(uint32_t minimumSize)
{
    if(minimumSize > _sizeOfSet)
    {
        // make sure that our buffers are big enough
        _data.resize(minimumSize);
        _sizeOfSet = minimumSize;
    }
    // reset memory
    for(uint32_t i = 0; i < _sizeOfSet; i++)
    {
        _data.at(i).tmmbr = 0;
        _data.at(i).packet_oh = 0;
        _data.at(i).ssrc = 0;
    }
    _lengthOfSet = 0;
}

void
TMMBRSet::VerifyAndAllocateSetKeepingData(uint32_t minimumSize)
{
    if(minimumSize > _sizeOfSet)
    {
        {
          _data.resize(minimumSize);
        }
        _sizeOfSet = minimumSize;
    }
}

void TMMBRSet::SetEntry(unsigned int i,
                         uint32_t tmmbrSet,
                         uint32_t packetOHSet,
                         uint32_t ssrcSet) {
  assert(i < _sizeOfSet);
  _data.at(i).tmmbr = tmmbrSet;
  _data.at(i).packet_oh = packetOHSet;
  _data.at(i).ssrc = ssrcSet;
  if (i >= _lengthOfSet) {
    _lengthOfSet = i + 1;
  }
}

void TMMBRSet::AddEntry(uint32_t tmmbrSet,
                        uint32_t packetOHSet,
                        uint32_t ssrcSet) {
  assert(_lengthOfSet < _sizeOfSet);
  SetEntry(_lengthOfSet, tmmbrSet, packetOHSet, ssrcSet);
}

void TMMBRSet::RemoveEntry(uint32_t sourceIdx) {
  assert(sourceIdx < _lengthOfSet);
  _data.erase(_data.begin() + sourceIdx);
  _lengthOfSet--;
  _data.resize(_sizeOfSet);  // Ensure that size remains the same.
}

void TMMBRSet::SwapEntries(uint32_t i, uint32_t j) {
    SetElement temp;
    temp = _data[i];
    _data[i] = _data[j];
    _data[j] = temp;
}

void TMMBRSet::ClearEntry(uint32_t idx) {
  SetEntry(idx, 0, 0, 0);
}

TMMBRHelp::TMMBRHelp()
    : _criticalSection(CriticalSectionWrapper::CreateCriticalSection()),
      _candidateSet(),
      _boundingSet(),
      _boundingSetToSend(),
      _ptrIntersectionBoundingSet(NULL),
      _ptrMaxPRBoundingSet(NULL) {
}

TMMBRHelp::~TMMBRHelp() {
  delete [] _ptrIntersectionBoundingSet;
  delete [] _ptrMaxPRBoundingSet;
  _ptrIntersectionBoundingSet = 0;
  _ptrMaxPRBoundingSet = 0;
  delete _criticalSection;
}

TMMBRSet*
TMMBRHelp::VerifyAndAllocateBoundingSet(uint32_t minimumSize)
{
    CriticalSectionScoped lock(_criticalSection);

    if(minimumSize > _boundingSet.sizeOfSet())
    {
        // make sure that our buffers are big enough
        if(_ptrIntersectionBoundingSet)
        {
            delete [] _ptrIntersectionBoundingSet;
            delete [] _ptrMaxPRBoundingSet;
        }
        _ptrIntersectionBoundingSet = new float[minimumSize];
        _ptrMaxPRBoundingSet = new float[minimumSize];
    }
    _boundingSet.VerifyAndAllocateSet(minimumSize);
    return &_boundingSet;
}

TMMBRSet* TMMBRHelp::BoundingSet() {
  return &_boundingSet;
}

int32_t
TMMBRHelp::SetTMMBRBoundingSetToSend(const TMMBRSet* boundingSetToSend,
                                     const uint32_t maxBitrateKbit)
{
    CriticalSectionScoped lock(_criticalSection);

    if (boundingSetToSend == NULL)
    {
        _boundingSetToSend.clearSet();
        return 0;
    }

    VerifyAndAllocateBoundingSetToSend(boundingSetToSend->lengthOfSet());
    _boundingSetToSend.clearSet();
    for (uint32_t i = 0; i < boundingSetToSend->lengthOfSet(); i++)
    {
        // cap at our configured max bitrate
        uint32_t bitrate = boundingSetToSend->Tmmbr(i);
        if(maxBitrateKbit)
        {
            // do we have a configured max bitrate?
            if(bitrate > maxBitrateKbit)
            {
                bitrate = maxBitrateKbit;
            }
        }
        _boundingSetToSend.SetEntry(i, bitrate,
                                    boundingSetToSend->PacketOH(i),
                                    boundingSetToSend->Ssrc(i));
    }
    return 0;
}

int32_t
TMMBRHelp::VerifyAndAllocateBoundingSetToSend(uint32_t minimumSize)
{
    CriticalSectionScoped lock(_criticalSection);

    _boundingSetToSend.VerifyAndAllocateSet(minimumSize);
    return 0;
}

TMMBRSet*
TMMBRHelp::VerifyAndAllocateCandidateSet(uint32_t minimumSize)
{
    CriticalSectionScoped lock(_criticalSection);

    _candidateSet.VerifyAndAllocateSet(minimumSize);
    return &_candidateSet;
}

TMMBRSet*
TMMBRHelp::CandidateSet()
{
    return &_candidateSet;
}

TMMBRSet*
TMMBRHelp::BoundingSetToSend()
{
    return &_boundingSetToSend;
}

int32_t
TMMBRHelp::FindTMMBRBoundingSet(TMMBRSet*& boundingSet)
{
    CriticalSectionScoped lock(_criticalSection);

    // Work on local variable, will be modified
    TMMBRSet    candidateSet;
    candidateSet.VerifyAndAllocateSet(_candidateSet.sizeOfSet());

    // TODO(hta) Figure out if this should be lengthOfSet instead.
    for (uint32_t i = 0; i < _candidateSet.sizeOfSet(); i++)
    {
        if(_candidateSet.Tmmbr(i))
        {
            candidateSet.AddEntry(_candidateSet.Tmmbr(i),
                                  _candidateSet.PacketOH(i),
                                  _candidateSet.Ssrc(i));
        }
        else
        {
            // make sure this is zero if tmmbr = 0
            assert(_candidateSet.PacketOH(i) == 0);
            // Old code:
            // _candidateSet.ptrPacketOHSet[i] = 0;
        }
    }

    // Number of set candidates
    int32_t numSetCandidates = candidateSet.lengthOfSet();
    // Find bounding set
    uint32_t numBoundingSet = 0;
    if (numSetCandidates > 0)
    {
        numBoundingSet =  FindTMMBRBoundingSet(numSetCandidates, candidateSet);
        if(numBoundingSet < 1 || (numBoundingSet > _candidateSet.sizeOfSet()))
        {
            return -1;
        }
        boundingSet = &_boundingSet;
    }
    return numBoundingSet;
}


int32_t
TMMBRHelp::FindTMMBRBoundingSet(int32_t numCandidates, TMMBRSet& candidateSet)
{
    CriticalSectionScoped lock(_criticalSection);

    uint32_t numBoundingSet = 0;
    VerifyAndAllocateBoundingSet(candidateSet.sizeOfSet());

    if (numCandidates == 1)
    {
        // TODO(hta): lengthOfSet instead of sizeOfSet?
        for (uint32_t i = 0; i < candidateSet.sizeOfSet(); i++)
        {
            if (candidateSet.Tmmbr(i) > 0)
            {
                _boundingSet.AddEntry(candidateSet.Tmmbr(i),
                                    candidateSet.PacketOH(i),
                                    candidateSet.Ssrc(i));
                numBoundingSet++;
            }
        }
        if (numBoundingSet != 1)
        {
            numBoundingSet = -1;
        }
    } else
    {
        // 1. Sort by increasing packetOH
        for (int i = candidateSet.sizeOfSet() - 1; i >= 0; i--)
        {
            for (int j = 1; j <= i; j++)
            {
                if (candidateSet.PacketOH(j-1) > candidateSet.PacketOH(j))
                {
                    candidateSet.SwapEntries(j-1, j);
                }
            }
        }
        // 2. For tuples with same OH, keep the one w/ the lowest bitrate
        for (uint32_t i = 0; i < candidateSet.sizeOfSet(); i++)
        {
            if (candidateSet.Tmmbr(i) > 0)
            {
                // get min bitrate for packets w/ same OH
                uint32_t currentPacketOH = candidateSet.PacketOH(i);
                uint32_t currentMinTMMBR = candidateSet.Tmmbr(i);
                uint32_t currentMinIndexTMMBR = i;
                for (uint32_t j = i+1; j < candidateSet.sizeOfSet(); j++)
                {
                    if(candidateSet.PacketOH(j) == currentPacketOH)
                    {
                        if(candidateSet.Tmmbr(j) < currentMinTMMBR)
                        {
                            currentMinTMMBR = candidateSet.Tmmbr(j);
                            currentMinIndexTMMBR = j;
                        }
                    }
                }
                // keep lowest bitrate
                for (uint32_t j = 0; j < candidateSet.sizeOfSet(); j++)
                {
                  if(candidateSet.PacketOH(j) == currentPacketOH
                     && j != currentMinIndexTMMBR)
                    {
                        candidateSet.ClearEntry(j);
                    }
                }
            }
        }
        // 3. Select and remove tuple w/ lowest tmmbr.
        // (If more than 1, choose the one w/ highest OH).
        uint32_t minTMMBR = 0;
        uint32_t minIndexTMMBR = 0;
        for (uint32_t i = 0; i < candidateSet.sizeOfSet(); i++)
        {
            if (candidateSet.Tmmbr(i) > 0)
            {
                minTMMBR = candidateSet.Tmmbr(i);
                minIndexTMMBR = i;
                break;
            }
        }

        for (uint32_t i = 0; i < candidateSet.sizeOfSet(); i++)
        {
            if (candidateSet.Tmmbr(i) > 0 && candidateSet.Tmmbr(i) <= minTMMBR)
            {
                // get min bitrate
                minTMMBR = candidateSet.Tmmbr(i);
                minIndexTMMBR = i;
            }
        }
        // first member of selected list
        _boundingSet.SetEntry(numBoundingSet,
                              candidateSet.Tmmbr(minIndexTMMBR),
                              candidateSet.PacketOH(minIndexTMMBR),
                              candidateSet.Ssrc(minIndexTMMBR));

        // set intersection value
        _ptrIntersectionBoundingSet[numBoundingSet] = 0;
        // calculate its maximum packet rate (where its line crosses x-axis)
        _ptrMaxPRBoundingSet[numBoundingSet]
            = _boundingSet.Tmmbr(numBoundingSet) * 1000
            / float(8 * _boundingSet.PacketOH(numBoundingSet));
        numBoundingSet++;
        // remove from candidate list
        candidateSet.ClearEntry(minIndexTMMBR);
        numCandidates--;

        // 4. Discard from candidate list all tuple w/ lower OH
        // (next tuple must be steeper)
        for (uint32_t i = 0; i < candidateSet.sizeOfSet(); i++)
        {
            if(candidateSet.Tmmbr(i) > 0
               && candidateSet.PacketOH(i) < _boundingSet.PacketOH(0))
            {
                candidateSet.ClearEntry(i);
                numCandidates--;
            }
        }

        if (numCandidates == 0)
        {
            // Should be true already:_boundingSet.lengthOfSet = numBoundingSet;
            assert(_boundingSet.lengthOfSet() == numBoundingSet);
            return numBoundingSet;
        }

        bool getNewCandidate = true;
        int curCandidateTMMBR = 0;
        int curCandidateIndex = 0;
        int curCandidatePacketOH = 0;
        int curCandidateSSRC = 0;
        do
        {
            if (getNewCandidate)
            {
                // 5. Remove first remaining tuple from candidate list
                for (uint32_t i = 0; i < candidateSet.sizeOfSet(); i++)
                {
                    if (candidateSet.Tmmbr(i) > 0)
                    {
                        curCandidateTMMBR    = candidateSet.Tmmbr(i);
                        curCandidatePacketOH = candidateSet.PacketOH(i);
                        curCandidateSSRC     = candidateSet.Ssrc(i);
                        curCandidateIndex    = i;
                        candidateSet.ClearEntry(curCandidateIndex);
                        break;
                    }
                }
            }

            // 6. Calculate packet rate and intersection of the current
            // line with line of last tuple in selected list
            float packetRate
                = float(curCandidateTMMBR
                        - _boundingSet.Tmmbr(numBoundingSet-1))*1000
                / (8*(curCandidatePacketOH
                      - _boundingSet.PacketOH(numBoundingSet-1)));

            // 7. If the packet rate is equal or lower than intersection of
            //    last tuple in selected list,
            //    remove last tuple in selected list & go back to step 6
            if(packetRate <= _ptrIntersectionBoundingSet[numBoundingSet-1])
            {
                // remove last tuple and goto step 6
                numBoundingSet--;
                _boundingSet.ClearEntry(numBoundingSet);
                _ptrIntersectionBoundingSet[numBoundingSet] = 0;
                _ptrMaxPRBoundingSet[numBoundingSet]        = 0;
                getNewCandidate = false;
            } else
            {
                // 8. If packet rate is lower than maximum packet rate of
                // last tuple in selected list, add current tuple to selected
                // list
                if (packetRate < _ptrMaxPRBoundingSet[numBoundingSet-1])
                {
                    _boundingSet.SetEntry(numBoundingSet,
                                          curCandidateTMMBR,
                                          curCandidatePacketOH,
                                          curCandidateSSRC);
                    _ptrIntersectionBoundingSet[numBoundingSet] = packetRate;
                    _ptrMaxPRBoundingSet[numBoundingSet]
                        = _boundingSet.Tmmbr(numBoundingSet)*1000
                        / float(8*_boundingSet.PacketOH(numBoundingSet));
                    numBoundingSet++;
                }
                numCandidates--;
                getNewCandidate = true;
            }

            // 9. Go back to step 5 if any tuple remains in candidate list
        } while (numCandidates > 0);
    }
    return numBoundingSet;
}

bool TMMBRHelp::IsOwner(const uint32_t ssrc,
                        const uint32_t length) const {
  CriticalSectionScoped lock(_criticalSection);

  if (length == 0) {
    // Empty bounding set.
    return false;
  }
  for(uint32_t i = 0;
      (i < length) && (i < _boundingSet.sizeOfSet()); ++i) {
    if(_boundingSet.Ssrc(i) == ssrc) {
      return true;
    }
  }
  return false;
}

bool TMMBRHelp::CalcMinBitRate( uint32_t* minBitrateKbit) const {
  CriticalSectionScoped lock(_criticalSection);

  if (_candidateSet.sizeOfSet() == 0) {
    // Empty bounding set.
    return false;
  }
  *minBitrateKbit = std::numeric_limits<uint32_t>::max();

  for (uint32_t i = 0; i < _candidateSet.lengthOfSet(); ++i) {
    uint32_t curNetBitRateKbit = _candidateSet.Tmmbr(i);
    if (curNetBitRateKbit < MIN_VIDEO_BW_MANAGEMENT_BITRATE) {
      curNetBitRateKbit = MIN_VIDEO_BW_MANAGEMENT_BITRATE;
    }
    *minBitrateKbit = curNetBitRateKbit < *minBitrateKbit ?
        curNetBitRateKbit : *minBitrateKbit;
  }
  return true;
}
} // namespace webrtc
