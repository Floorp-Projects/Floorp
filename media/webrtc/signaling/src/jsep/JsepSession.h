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


namespace mozilla {

// Forward declarations
struct JsepCodecDescription;
class JsepTrack;
struct JsepTrackPair;

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

struct JsepOAOptions {};
struct JsepOfferOptions : public JsepOAOptions {
  Maybe<size_t> mOfferToReceiveAudio;
  Maybe<size_t> mOfferToReceiveVideo;
  Maybe<bool> mDontOfferDataChannel;
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
      : mName(name), mState(kJsepStateStable)
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

  // Set up the ICE And DTLS data.
  virtual nsresult SetIceCredentials(const std::string& ufrag,
                                     const std::string& pwd) = 0;
  virtual nsresult SetBundlePolicy(JsepBundlePolicy policy) = 0;
  virtual bool RemoteIsIceLite() const = 0;
  virtual std::vector<std::string> GetIceOptions() const = 0;

  virtual nsresult AddDtlsFingerprint(const std::string& algorithm,
                                      const std::vector<uint8_t>& value) = 0;

  virtual nsresult AddAudioRtpExtension(const std::string& extensionName) = 0;
  virtual nsresult AddVideoRtpExtension(const std::string& extensionName) = 0;

  // Kinda gross to be locking down the data structure type like this, but
  // returning by value is problematic due to the lack of stl move semantics in
  // our build config, since we can't use UniquePtr in the container. The
  // alternative is writing a raft of accessor functions that allow arbitrary
  // manipulation (which will be unwieldy), or allowing functors to be injected
  // that manipulate the data structure (still pretty unwieldy).
  virtual std::vector<JsepCodecDescription*>& Codecs() = 0;

  // Manage tracks. We take shared ownership of any track.
  virtual nsresult AddTrack(const RefPtr<JsepTrack>& track) = 0;
  virtual nsresult RemoveTrack(const std::string& streamId,
                               const std::string& trackId) = 0;
  virtual nsresult ReplaceTrack(const std::string& oldStreamId,
                                const std::string& oldTrackId,
                                const std::string& newStreamId,
                                const std::string& newTrackId) = 0;

  virtual std::vector<RefPtr<JsepTrack>> GetLocalTracks() const = 0;

  virtual std::vector<RefPtr<JsepTrack>> GetRemoteTracks() const = 0;

  virtual std::vector<RefPtr<JsepTrack>> GetRemoteTracksAdded() const = 0;

  virtual std::vector<RefPtr<JsepTrack>> GetRemoteTracksRemoved() const = 0;

  // Access the negotiated track pairs.
  virtual std::vector<JsepTrackPair> GetNegotiatedTrackPairs() const = 0;

  // Access transports.
  virtual std::vector<RefPtr<JsepTransport>> GetTransports() const = 0;

  // Basic JSEP operations.
  virtual nsresult CreateOffer(const JsepOfferOptions& options,
                               std::string* offer) = 0;
  virtual nsresult CreateAnswer(const JsepAnswerOptions& options,
                                std::string* answer) = 0;
  virtual std::string GetLocalDescription() const = 0;
  virtual std::string GetRemoteDescription() const = 0;
  virtual nsresult SetLocalDescription(JsepSdpType type,
                                       const std::string& sdp) = 0;
  virtual nsresult SetRemoteDescription(JsepSdpType type,
                                        const std::string& sdp) = 0;
  virtual nsresult AddRemoteIceCandidate(const std::string& candidate,
                                         const std::string& mid,
                                         uint16_t level) = 0;
  virtual nsresult AddLocalIceCandidate(const std::string& candidate,
                                        const std::string& mid,
                                        uint16_t level,
                                        bool* skipped) = 0;
  virtual nsresult EndOfLocalCandidates(
      const std::string& defaultCandidateAddr,
      uint16_t defaultCandidatePort,
      const std::string& defaultRtcpCandidateAddr,
      uint16_t defaultRtcpCandidatePort,
      uint16_t level) = 0;
  virtual nsresult Close() = 0;

  // ICE controlling or controlled
  virtual bool IsIceControlling() const = 0;

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

  virtual bool AllLocalTracksAreAssigned() const = 0;

protected:
  const std::string mName;
  JsepSignalingState mState;
};

} // namespace mozilla

#endif
