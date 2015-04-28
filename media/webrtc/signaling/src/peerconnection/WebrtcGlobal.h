/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WEBRTC_GLOBAL_H_
#define _WEBRTC_GLOBAL_H_

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/RTCStatsReportBinding.h"

typedef mozilla::dom::RTCStatsReportInternal StatsReport;
typedef nsTArray< nsAutoPtr<StatsReport>> RTCReports;
typedef mozilla::dom::Sequence<nsString> WebrtcGlobalLog;

namespace IPC {

template<typename T>
struct ParamTraits<mozilla::dom::Optional<T>>
{
  typedef mozilla::dom::Optional<T> paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    if (aParam.WasPassed()) {
      WriteParam(aMsg, true);
      WriteParam(aMsg, aParam.Value());
      return;
    }

    WriteParam(aMsg, false);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool was_passed = false;

    if (!ReadParam(aMsg, aIter, &was_passed)) {
      return false;
    }

    aResult->Reset(); //XXX Optional_base seems to reach this point with isSome true.

    if (was_passed) {
      if (!ReadParam(aMsg, aIter, &(aResult->Construct()))) {
        return false;
      }
    }

    return true;
  }
};

template<typename T>
struct ParamTraits<mozilla::dom::Sequence<T>>
{
  typedef mozilla::dom::Sequence<T> paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<const FallibleTArray<T>&>(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, dynamic_cast<FallibleTArray<T>*>(aResult));
  }
};

template<>
struct ParamTraits<mozilla::dom::RTCStatsType> :
  public ContiguousEnumSerializer<
    mozilla::dom::RTCStatsType,
    mozilla::dom::RTCStatsType::Inboundrtp,
    mozilla::dom::RTCStatsType::EndGuard_>
{};

template<>
struct ParamTraits<mozilla::dom::RTCStatsIceCandidatePairState> :
  public ContiguousEnumSerializer<
    mozilla::dom::RTCStatsIceCandidatePairState,
    mozilla::dom::RTCStatsIceCandidatePairState::Frozen,
    mozilla::dom::RTCStatsIceCandidatePairState::EndGuard_>
{};

template<>
struct ParamTraits<mozilla::dom::RTCStatsIceCandidateType> :
  public ContiguousEnumSerializer<
    mozilla::dom::RTCStatsIceCandidateType,
    mozilla::dom::RTCStatsIceCandidateType::Host,
    mozilla::dom::RTCStatsIceCandidateType::EndGuard_>
{};

template<>
struct ParamTraits<mozilla::dom::RTCStatsReportInternal>
{
  typedef mozilla::dom::RTCStatsReportInternal paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mClosed);
    WriteParam(aMsg, aParam.mCodecStats);
    WriteParam(aMsg, aParam.mIceCandidatePairStats);
    WriteParam(aMsg, aParam.mIceCandidateStats);
    WriteParam(aMsg, aParam.mIceComponentStats);
    WriteParam(aMsg, aParam.mInboundRTPStreamStats);
    WriteParam(aMsg, aParam.mLocalSdp);
    WriteParam(aMsg, aParam.mMediaStreamStats);
    WriteParam(aMsg, aParam.mMediaStreamTrackStats);
    WriteParam(aMsg, aParam.mOutboundRTPStreamStats);
    WriteParam(aMsg, aParam.mPcid);
    WriteParam(aMsg, aParam.mRemoteSdp);
    WriteParam(aMsg, aParam.mTimestamp);
    WriteParam(aMsg, aParam.mTransportStats);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mClosed)) ||
        !ReadParam(aMsg, aIter, &(aResult->mCodecStats)) ||
        !ReadParam(aMsg, aIter, &(aResult->mIceCandidatePairStats)) ||
        !ReadParam(aMsg, aIter, &(aResult->mIceCandidateStats)) ||
        !ReadParam(aMsg, aIter, &(aResult->mIceComponentStats)) ||
        !ReadParam(aMsg, aIter, &(aResult->mInboundRTPStreamStats)) ||
        !ReadParam(aMsg, aIter, &(aResult->mLocalSdp)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMediaStreamStats)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMediaStreamTrackStats)) ||
        !ReadParam(aMsg, aIter, &(aResult->mOutboundRTPStreamStats)) ||
        !ReadParam(aMsg, aIter, &(aResult->mPcid)) ||
        !ReadParam(aMsg, aIter, &(aResult->mRemoteSdp)) ||
        !ReadParam(aMsg, aIter, &(aResult->mTimestamp)) ||
        !ReadParam(aMsg, aIter, &(aResult->mTransportStats))) {
      return false;
    }

    return true;
  }
};

typedef mozilla::dom::RTCStats RTCStats;

static void WriteRTCStats(Message* aMsg, const RTCStats& aParam)
{
  // RTCStats base class
  WriteParam(aMsg, aParam.mId);
  WriteParam(aMsg, aParam.mTimestamp);
  WriteParam(aMsg, aParam.mType);
}

static bool ReadRTCStats(const Message* aMsg, void** aIter, RTCStats* aResult)
{
  // RTCStats base class
  if (!ReadParam(aMsg, aIter, &(aResult->mId)) ||
      !ReadParam(aMsg, aIter, &(aResult->mTimestamp)) ||
      !ReadParam(aMsg, aIter, &(aResult->mType))) {
    return false;
  }

  return true;
}

template<>
struct ParamTraits<mozilla::dom::RTCCodecStats>
{
  typedef mozilla::dom::RTCCodecStats paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mChannels);
    WriteParam(aMsg, aParam.mClockRate);
    WriteParam(aMsg, aParam.mCodec);
    WriteParam(aMsg, aParam.mParameters);
    WriteParam(aMsg, aParam.mPayloadType);
    WriteRTCStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mChannels)) ||
        !ReadParam(aMsg, aIter, &(aResult->mClockRate)) ||
        !ReadParam(aMsg, aIter, &(aResult->mCodec)) ||
        !ReadParam(aMsg, aIter, &(aResult->mParameters)) ||
        !ReadParam(aMsg, aIter, &(aResult->mPayloadType)) ||
        !ReadRTCStats(aMsg, aIter, aResult)) {
      return false;
    }

    return true;
 }
};

template<>
struct ParamTraits<mozilla::dom::RTCIceCandidatePairStats>
{
  typedef mozilla::dom::RTCIceCandidatePairStats paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mComponentId);
    WriteParam(aMsg, aParam.mLocalCandidateId);
    WriteParam(aMsg, aParam.mMozPriority);
    WriteParam(aMsg, aParam.mNominated);
    WriteParam(aMsg, aParam.mReadable);
    WriteParam(aMsg, aParam.mRemoteCandidateId);
    WriteParam(aMsg, aParam.mSelected);
    WriteParam(aMsg, aParam.mState);
    WriteRTCStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mComponentId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mLocalCandidateId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMozPriority)) ||
        !ReadParam(aMsg, aIter, &(aResult->mNominated)) ||
        !ReadParam(aMsg, aIter, &(aResult->mReadable)) ||
        !ReadParam(aMsg, aIter, &(aResult->mRemoteCandidateId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mSelected)) ||
        !ReadParam(aMsg, aIter, &(aResult->mState)) ||
        !ReadRTCStats(aMsg, aIter, aResult)) {
      return false;
    }

    return true;
 }
};

template<>
struct ParamTraits<mozilla::dom::RTCIceCandidateStats>
{
  typedef mozilla::dom::RTCIceCandidateStats paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mCandidateId);
    WriteParam(aMsg, aParam.mCandidateType);
    WriteParam(aMsg, aParam.mComponentId);
    WriteParam(aMsg, aParam.mIpAddress);
    WriteParam(aMsg, aParam.mMozLocalTransport);
    WriteParam(aMsg, aParam.mPortNumber);
    WriteParam(aMsg, aParam.mTransport);
    WriteRTCStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mCandidateId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mCandidateType)) ||
        !ReadParam(aMsg, aIter, &(aResult->mComponentId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mIpAddress)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMozLocalTransport)) ||
        !ReadParam(aMsg, aIter, &(aResult->mPortNumber)) ||
        !ReadParam(aMsg, aIter, &(aResult->mTransport)) ||
        !ReadRTCStats(aMsg, aIter, aResult)) {
      return false;
    }

    return true;
 }
};

template<>
struct ParamTraits<mozilla::dom::RTCIceComponentStats>
{
  typedef mozilla::dom::RTCIceComponentStats paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mActiveConnection);
    WriteParam(aMsg, aParam.mBytesReceived);
    WriteParam(aMsg, aParam.mBytesSent);
    WriteParam(aMsg, aParam.mComponent);
    WriteParam(aMsg, aParam.mTransportId);
    WriteRTCStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mActiveConnection)) ||
        !ReadParam(aMsg, aIter, &(aResult->mBytesReceived)) ||
        !ReadParam(aMsg, aIter, &(aResult->mBytesSent)) ||
        !ReadParam(aMsg, aIter, &(aResult->mComponent)) ||
        !ReadParam(aMsg, aIter, &(aResult->mTransportId)) ||
        !ReadRTCStats(aMsg, aIter, aResult)) {
      return false;
    }

    return true;
  }
};

static void WriteRTCRTPStreamStats(
              Message* aMsg,
              const mozilla::dom::RTCRTPStreamStats& aParam)
{
    WriteParam(aMsg, aParam.mBitrateMean);
    WriteParam(aMsg, aParam.mBitrateStdDev);
    WriteParam(aMsg, aParam.mCodecId);
    WriteParam(aMsg, aParam.mFramerateMean);
    WriteParam(aMsg, aParam.mFramerateStdDev);
    WriteParam(aMsg, aParam.mIsRemote);
    WriteParam(aMsg, aParam.mMediaTrackId);
    WriteParam(aMsg, aParam.mMediaType);
    WriteParam(aMsg, aParam.mRemoteId);
    WriteParam(aMsg, aParam.mSsrc);
    WriteParam(aMsg, aParam.mTransportId);
}

static bool ReadRTCRTPStreamStats(
              const Message* aMsg, void** aIter,
              mozilla::dom::RTCRTPStreamStats* aResult)
{
  if (!ReadParam(aMsg, aIter, &(aResult->mBitrateMean)) ||
      !ReadParam(aMsg, aIter, &(aResult->mBitrateStdDev)) ||
      !ReadParam(aMsg, aIter, &(aResult->mCodecId)) ||
      !ReadParam(aMsg, aIter, &(aResult->mFramerateMean)) ||
      !ReadParam(aMsg, aIter, &(aResult->mFramerateStdDev)) ||
      !ReadParam(aMsg, aIter, &(aResult->mIsRemote)) ||
      !ReadParam(aMsg, aIter, &(aResult->mMediaTrackId)) ||
      !ReadParam(aMsg, aIter, &(aResult->mMediaType)) ||
      !ReadParam(aMsg, aIter, &(aResult->mRemoteId)) ||
      !ReadParam(aMsg, aIter, &(aResult->mSsrc)) ||
      !ReadParam(aMsg, aIter, &(aResult->mTransportId))) {
    return false;
  }

  return true;
}

template<>
struct ParamTraits<mozilla::dom::RTCInboundRTPStreamStats>
{
  typedef mozilla::dom::RTCInboundRTPStreamStats paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mBytesReceived);
    WriteParam(aMsg, aParam.mDiscardedPackets);
    WriteParam(aMsg, aParam.mJitter);
    WriteParam(aMsg, aParam.mMozAvSyncDelay);
    WriteParam(aMsg, aParam.mMozJitterBufferDelay);
    WriteParam(aMsg, aParam.mMozRtt);
    WriteParam(aMsg, aParam.mPacketsLost);
    WriteParam(aMsg, aParam.mPacketsReceived);
    WriteRTCRTPStreamStats(aMsg, aParam);
    WriteRTCStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mBytesReceived)) ||
        !ReadParam(aMsg, aIter, &(aResult->mDiscardedPackets)) ||
        !ReadParam(aMsg, aIter, &(aResult->mJitter)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMozAvSyncDelay)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMozJitterBufferDelay)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMozRtt)) ||
        !ReadParam(aMsg, aIter, &(aResult->mPacketsLost)) ||
        !ReadParam(aMsg, aIter, &(aResult->mPacketsReceived)) ||
        !ReadRTCRTPStreamStats(aMsg, aIter, aResult) ||
        !ReadRTCStats(aMsg, aIter, aResult)) {
      return false;
    }

    return true;
  }
};

template<>
struct ParamTraits<mozilla::dom::RTCOutboundRTPStreamStats>
{
  typedef mozilla::dom::RTCOutboundRTPStreamStats paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mBytesSent);
    WriteParam(aMsg, aParam.mDroppedFrames);
    WriteParam(aMsg, aParam.mPacketsSent);
    WriteParam(aMsg, aParam.mTargetBitrate);
    WriteRTCRTPStreamStats(aMsg, aParam);
    WriteRTCStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mBytesSent)) ||
        !ReadParam(aMsg, aIter, &(aResult->mDroppedFrames)) ||
        !ReadParam(aMsg, aIter, &(aResult->mPacketsSent)) ||
        !ReadParam(aMsg, aIter, &(aResult->mTargetBitrate)) ||
        !ReadRTCRTPStreamStats(aMsg, aIter, aResult) ||
        !ReadRTCStats(aMsg, aIter, aResult)) {
      return false;
    }

    return true;
  }
};

template<>
struct ParamTraits<mozilla::dom::RTCMediaStreamStats>
{
  typedef mozilla::dom::RTCMediaStreamStats paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mStreamIdentifier);
    WriteParam(aMsg, aParam.mTrackIds);
    WriteRTCStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mStreamIdentifier)) ||
        !ReadParam(aMsg, aIter, &(aResult->mTrackIds)) ||
        !ReadRTCStats(aMsg, aIter, aResult)) {
      return false;
    }

    return true;
  }
};

template<>
struct ParamTraits<mozilla::dom::RTCTransportStats>
{
  typedef mozilla::dom::RTCTransportStats paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mBytesReceived);
    WriteParam(aMsg, aParam.mBytesSent);
    WriteRTCStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mBytesReceived)) ||
        !ReadParam(aMsg, aIter, &(aResult->mBytesSent)) ||
        !ReadRTCStats(aMsg, aIter, aResult)) {
      return false;
    }

    return true;
  }
};

template<>
struct ParamTraits<mozilla::dom::RTCMediaStreamTrackStats>
{
  typedef mozilla::dom::RTCMediaStreamTrackStats paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mAudioLevel);
    WriteParam(aMsg, aParam.mEchoReturnLoss);
    WriteParam(aMsg, aParam.mEchoReturnLossEnhancement);
    WriteParam(aMsg, aParam.mFrameHeight);
    WriteParam(aMsg, aParam.mFrameWidth);
    WriteParam(aMsg, aParam.mFramesCorrupted);
    WriteParam(aMsg, aParam.mFramesDecoded);
    WriteParam(aMsg, aParam.mFramesDropped);
    WriteParam(aMsg, aParam.mFramesPerSecond);
    WriteParam(aMsg, aParam.mFramesReceived);
    WriteParam(aMsg, aParam.mFramesSent);
    WriteParam(aMsg, aParam.mRemoteSource);
    WriteParam(aMsg, aParam.mSsrcIds);
    WriteParam(aMsg, aParam.mTrackIdentifier);
    WriteRTCStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mAudioLevel)) ||
        !ReadParam(aMsg, aIter, &(aResult->mEchoReturnLoss)) ||
        !ReadParam(aMsg, aIter, &(aResult->mEchoReturnLossEnhancement)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFrameHeight)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFrameWidth)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFramesCorrupted)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFramesDecoded)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFramesDropped)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFramesPerSecond)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFramesReceived)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFramesSent)) ||
        !ReadParam(aMsg, aIter, &(aResult->mRemoteSource)) ||
        !ReadParam(aMsg, aIter, &(aResult->mSsrcIds)) ||
        !ReadParam(aMsg, aIter, &(aResult->mTrackIdentifier)) ||
        !ReadRTCStats(aMsg, aIter, aResult)) {
      return false;
    }

    return true;
  }
};
} // namespace ipc

#endif  // _WEBRTC_GLOBAL_H_
