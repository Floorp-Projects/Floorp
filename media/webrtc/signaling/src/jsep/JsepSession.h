/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPSESSION_H_
#define _JSEPSESSION_H_

#include <string>
#include <vector>
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsError.h"

#include "signaling/src/jsep/JsepTransport.h"
#include "signaling/src/sdp/Sdp.h"

#include "signaling/src/jsep/JsepTransceiver.h"

namespace mozilla {

// Forward declarations
class JsepCodecDescription;

enum JsepSignalingState {
  kJsepStateStable,
  kJsepStateHaveLocalOffer,
  kJsepStateHaveRemoteOffer,
  kJsepStateHaveLocalPranswer,
  kJsepStateHaveRemotePranswer,
  kJsepStateClosed
};

enum JsepSdpType {
  kJsepSdpOffer,
  kJsepSdpAnswer,
  kJsepSdpPranswer,
  kJsepSdpRollback
};

enum JsepDescriptionPendingOrCurrent {
  kJsepDescriptionCurrent,
  kJsepDescriptionPending,
  kJsepDescriptionPendingOrCurrent
};

struct JsepOAOptions {};
struct JsepOfferOptions : public JsepOAOptions {
  Maybe<size_t> mOfferToReceiveAudio;
  Maybe<size_t> mOfferToReceiveVideo;
  Maybe<bool> mIceRestart; // currently ignored by JsepSession
};
struct JsepAnswerOptions : public JsepOAOptions {};

enum JsepBundlePolicy {
  kBundleBalanced,
  kBundleMaxCompat,
  kBundleMaxBundle
};

class JsepSession
{
public:
  explicit JsepSession(const std::string& name)
    : mName(name), mState(kJsepStateStable), mNegotiations(0)
  {
  }
  virtual ~JsepSession() {}

  virtual nsresult Init() = 0;

  // Accessors for basic properties.
  virtual const std::string&
  GetName() const
  {
    return mName;
  }
  virtual JsepSignalingState
  GetState() const
  {
    return mState;
  }
  virtual uint32_t
  GetNegotiations() const
  {
    return mNegotiations;
  }

  // Set up the ICE And DTLS data.
  virtual nsresult SetIceCredentials(const std::string& ufrag,
                                     const std::string& pwd) = 0;
  virtual const std::string& GetUfrag() const = 0;
  virtual const std::string& GetPwd() const = 0;
  virtual nsresult SetBundlePolicy(JsepBundlePolicy policy) = 0;
  virtual bool RemoteIsIceLite() const = 0;
  virtual bool RemoteIceIsRestarting() const = 0;
  virtual std::vector<std::string> GetIceOptions() const = 0;

  virtual nsresult AddDtlsFingerprint(const std::string& algorithm,
                                      const std::vector<uint8_t>& value) = 0;

  virtual nsresult AddAudioRtpExtension(const std::string& extensionName,
                                        SdpDirectionAttribute::Direction direction) = 0;
  virtual nsresult AddVideoRtpExtension(const std::string& extensionName,
                                        SdpDirectionAttribute::Direction direction) = 0;

  // Kinda gross to be locking down the data structure type like this, but
  // returning by value is problematic due to the lack of stl move semantics in
  // our build config, since we can't use UniquePtr in the container. The
  // alternative is writing a raft of accessor functions that allow arbitrary
  // manipulation (which will be unwieldy), or allowing functors to be injected
  // that manipulate the data structure (still pretty unwieldy).
  virtual std::vector<JsepCodecDescription*>& Codecs() = 0;

  template <class UnaryFunction>
  void ForEachCodec(UnaryFunction& function)
  {
    std::for_each(Codecs().begin(), Codecs().end(), function);
    for (auto& transceiver : GetTransceivers()) {
      transceiver->mSendTrack.ForEachCodec(function);
      transceiver->mRecvTrack.ForEachCodec(function);
    }
  }

  template <class BinaryPredicate>
  void SortCodecs(BinaryPredicate& sorter)
  {
    std::stable_sort(Codecs().begin(), Codecs().end(), sorter);
    for (auto& transceiver : GetTransceivers()) {
      transceiver->mSendTrack.SortCodecs(sorter);
      transceiver->mRecvTrack.SortCodecs(sorter);
    }
  }

  virtual const std::vector<RefPtr<JsepTransceiver>>&
    GetTransceivers() const = 0;
  virtual std::vector<RefPtr<JsepTransceiver>>& GetTransceivers() = 0;
  virtual nsresult AddTransceiver(RefPtr<JsepTransceiver> transceiver) = 0;

  // Basic JSEP operations.
  virtual nsresult CreateOffer(const JsepOfferOptions& options,
                               std::string* offer) = 0;
  virtual nsresult CreateAnswer(const JsepAnswerOptions& options,
                                std::string* answer) = 0;
  virtual std::string GetLocalDescription(JsepDescriptionPendingOrCurrent type)
                                          const = 0;
  virtual std::string GetRemoteDescription(JsepDescriptionPendingOrCurrent type)
                                           const = 0;
  virtual nsresult SetLocalDescription(JsepSdpType type,
                                       const std::string& sdp) = 0;
  virtual nsresult SetRemoteDescription(JsepSdpType type,
                                        const std::string& sdp) = 0;
  virtual nsresult AddRemoteIceCandidate(const std::string& candidate,
                                         const std::string& mid,
                                         uint16_t level) = 0;
  virtual nsresult AddLocalIceCandidate(const std::string& candidate,
                                        uint16_t level,
                                        std::string* mid,
                                        bool* skipped) = 0;
  virtual nsresult UpdateDefaultCandidate(
      const std::string& defaultCandidateAddr,
      uint16_t defaultCandidatePort,
      const std::string& defaultRtcpCandidateAddr,
      uint16_t defaultRtcpCandidatePort,
      uint16_t level) = 0;
  virtual nsresult EndOfLocalCandidates(uint16_t level) = 0;
  virtual nsresult Close() = 0;

  // ICE controlling or controlled
  virtual bool IsIceControlling() const = 0;
  virtual bool IsOfferer() const = 0;

  virtual const std::string
  GetLastError() const
  {
    return "Error";
  }

  static const char*
  GetStateStr(JsepSignalingState state)
  {
    static const char* states[] = { "stable", "have-local-offer",
                                    "have-remote-offer", "have-local-pranswer",
                                    "have-remote-pranswer", "closed" };

    return states[state];
  }

  virtual bool CheckNegotiationNeeded() const = 0;

  void
  CountTracks(uint16_t (&receiving)[SdpMediaSection::kMediaTypes],
              uint16_t (&sending)[SdpMediaSection::kMediaTypes]) const
  {
    memset(receiving, 0, sizeof(receiving));
    memset(sending, 0, sizeof(sending));

    for (const auto& transceiver : GetTransceivers()) {
      if (!transceiver->mRecvTrack.GetTrackId().empty()) {
        receiving[transceiver->mRecvTrack.GetMediaType()]++;
      }

      if (!transceiver->mSendTrack.GetTrackId().empty()) {
        sending[transceiver->mSendTrack.GetMediaType()]++;
      }
    }
  }

protected:

  const std::string mName;
  JsepSignalingState mState;
  uint32_t mNegotiations;
};

} // namespace mozilla

#endif
