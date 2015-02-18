/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPSESSIONIMPL_H_
#define _JSEPSESSIONIMPL_H_

#include <set>
#include <string>
#include <vector>

#include "signaling/src/jsep/JsepCodecDescription.h"
#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepSession.h"
#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepTrackImpl.h"
#include "signaling/src/sdp/SipccSdpParser.h"

namespace mozilla {

class JsepUuidGenerator
{
public:
  virtual ~JsepUuidGenerator() {}
  virtual bool Generate(std::string* id) = 0;
};

class JsepSessionImpl : public JsepSession
{
public:
  JsepSessionImpl(const std::string& name, UniquePtr<JsepUuidGenerator> uuidgen)
      : JsepSession(name),
        mIsOfferer(false),
        mIceControlling(false),
        mRemoteIsIceLite(false),
        mSessionId(0),
        mSessionVersion(0),
        mUuidGen(Move(uuidgen))
  {
  }

  virtual ~JsepSessionImpl();

  // Implement JsepSession methods.
  virtual nsresult Init() MOZ_OVERRIDE;

  virtual nsresult AddTrack(const RefPtr<JsepTrack>& track) MOZ_OVERRIDE;

  virtual nsresult RemoveTrack(const std::string& streamId,
                               const std::string& trackId) MOZ_OVERRIDE;

  virtual nsresult SetIceCredentials(const std::string& ufrag,
                                     const std::string& pwd) MOZ_OVERRIDE;

  virtual bool
  RemoteIsIceLite() const MOZ_OVERRIDE
  {
    return mRemoteIsIceLite;
  }

  virtual std::vector<std::string>
  GetIceOptions() const MOZ_OVERRIDE
  {
    return mIceOptions;
  }

  virtual nsresult AddDtlsFingerprint(const std::string& algorithm,
                                      const std::vector<uint8_t>& value) MOZ_OVERRIDE;

  virtual nsresult AddAudioRtpExtension(
      const std::string& extensionName) MOZ_OVERRIDE;

  virtual nsresult AddVideoRtpExtension(
      const std::string& extensionName) MOZ_OVERRIDE;

  virtual std::vector<JsepCodecDescription*>&
  Codecs() MOZ_OVERRIDE
  {
    return mCodecs;
  }

  virtual nsresult
  ReplaceTrack(size_t trackIndex, const RefPtr<JsepTrack>& track) MOZ_OVERRIDE
  {
    mLastError.clear();
    MOZ_CRASH(); // Stub
  }

  virtual std::vector<RefPtr<JsepTrack>> GetLocalTracks() const MOZ_OVERRIDE;

  virtual std::vector<RefPtr<JsepTrack>> GetRemoteTracks() const MOZ_OVERRIDE;

  virtual std::vector<RefPtr<JsepTrack>>
    GetRemoteTracksAdded() const MOZ_OVERRIDE;

  virtual std::vector<RefPtr<JsepTrack>>
    GetRemoteTracksRemoved() const MOZ_OVERRIDE;

  virtual nsresult CreateOffer(const JsepOfferOptions& options,
                               std::string* offer) MOZ_OVERRIDE;

  virtual nsresult CreateAnswer(const JsepAnswerOptions& options,
                                std::string* answer) MOZ_OVERRIDE;

  virtual std::string GetLocalDescription() const MOZ_OVERRIDE;

  virtual std::string GetRemoteDescription() const MOZ_OVERRIDE;

  virtual nsresult SetLocalDescription(JsepSdpType type,
                                       const std::string& sdp) MOZ_OVERRIDE;

  virtual nsresult SetRemoteDescription(JsepSdpType type,
                                        const std::string& sdp) MOZ_OVERRIDE;

  virtual nsresult AddRemoteIceCandidate(const std::string& candidate,
                                         const std::string& mid,
                                         uint16_t level) MOZ_OVERRIDE;

  virtual nsresult AddLocalIceCandidate(const std::string& candidate,
                                        const std::string& mid,
                                        uint16_t level,
                                        bool* skipped) MOZ_OVERRIDE;

  virtual nsresult EndOfLocalCandidates(const std::string& defaultCandidateAddr,
                                        uint16_t defaultCandidatePort,
                                        uint16_t level) MOZ_OVERRIDE;

  virtual nsresult Close() MOZ_OVERRIDE;

  virtual const std::string GetLastError() const MOZ_OVERRIDE;

  virtual bool
  IsIceControlling() const MOZ_OVERRIDE
  {
    return mIceControlling;
  }

  virtual bool
  IsOfferer() const
  {
    return mIsOfferer;
  }

  // Access transports.
  virtual std::vector<RefPtr<JsepTransport>>
  GetTransports() const MOZ_OVERRIDE
  {
    return mTransports;
  }

  virtual std::vector<JsepTrackPair>
  GetNegotiatedTrackPairs() const MOZ_OVERRIDE
  {
    return mNegotiatedTrackPairs;
  }

  virtual bool AllLocalTracksAreAssigned() const MOZ_OVERRIDE;

private:
  struct JsepDtlsFingerprint {
    std::string mAlgorithm;
    std::vector<uint8_t> mValue;
  };

  struct JsepSendingTrack {
    RefPtr<JsepTrack> mTrack;
    Maybe<size_t> mAssignedMLine;
    bool mSetInLocalDescription;
  };

  struct JsepReceivingTrack {
    RefPtr<JsepTrack> mTrack;
    Maybe<size_t> mAssignedMLine;
  };

  // Non-const so it can set mLastError
  nsresult CreateGenericSDP(UniquePtr<Sdp>* sdp);
  void AddCodecs(SdpMediaSection* msection) const;
  void AddExtmap(SdpMediaSection* msection) const;
  void AddMid(const std::string& mid, SdpMediaSection* msection) const;
  void AddLocalSsrcs(const JsepTrack& track, SdpMediaSection* msection) const;
  void AddLocalIds(const JsepTrack& track, SdpMediaSection* msection) const;
  JsepCodecDescription* FindMatchingCodec(
      const std::string& pt,
      const SdpMediaSection& msection) const;
  const std::vector<SdpExtmapAttributeList::Extmap>* GetRtpExtensions(
      SdpMediaSection::MediaType type) const;
  void AddCommonCodecs(const SdpMediaSection& remoteMsection,
                       SdpMediaSection* msection);
  void AddCommonExtmaps(const SdpMediaSection& remoteMsection,
                        SdpMediaSection* msection);
  void SetupDefaultCodecs();
  void SetupDefaultRtpExtensions();
  void SetState(JsepSignalingState state);
  // Non-const so it can set mLastError
  nsresult ParseSdp(const std::string& sdp, UniquePtr<Sdp>* parsedp);
  nsresult SetLocalDescriptionOffer(UniquePtr<Sdp> offer);
  nsresult SetLocalDescriptionAnswer(JsepSdpType type, UniquePtr<Sdp> answer);
  nsresult SetRemoteDescriptionOffer(UniquePtr<Sdp> offer);
  nsresult SetRemoteDescriptionAnswer(JsepSdpType type, UniquePtr<Sdp> answer);
  nsresult ValidateLocalDescription(const Sdp& description);
  nsresult ValidateRemoteDescription(const Sdp& description);
  nsresult SetRemoteTracksFromDescription(const Sdp& remoteDescription);
  // Non-const because we use our Uuid generator
  nsresult CreateReceivingTrack(size_t mline,
                                const Sdp& sdp,
                                const SdpMediaSection& msection,
                                RefPtr<JsepTrack>* track);
  nsresult HandleNegotiatedSession(const UniquePtr<Sdp>& local,
                                   const UniquePtr<Sdp>& remote);
  nsresult AddTransportAttributes(SdpMediaSection* msection,
                                  SdpSetupAttribute::Role dtlsRole);
  // Non-const so it can assign m-line index to tracks
  nsresult AddOfferMSectionsByType(SdpMediaSection::MediaType type,
                                   Maybe<size_t> offerToReceive,
                                   Sdp* sdp);
  nsresult BindTrackToMsection(JsepSendingTrack* track,
                               SdpMediaSection* msection);
  nsresult CreateReoffer(const Sdp& oldLocalSdp,
                         const Sdp& oldAnswer,
                         Sdp* newSdp);
  void SetupBundle(Sdp* sdp) const;
  void SetupMsidSemantic(const std::vector<std::string>& msids, Sdp* sdp) const;
  nsresult GetIdsFromMsid(const Sdp& sdp,
                          const SdpMediaSection& msection,
                          std::string* streamId,
                          std::string* trackId);
  nsresult GetRemoteIds(const Sdp& sdp,
                        const SdpMediaSection& msection,
                        std::string* streamId,
                        std::string* trackId);
  nsresult GetMsids(const SdpMediaSection& msection,
                    std::vector<SdpMsidAttributeList::Msid>* msids);
  nsresult CreateOfferMSection(SdpMediaSection::MediaType type,
                               SdpDirectionAttribute::Direction direction,
                               Sdp* sdp);
  nsresult GetFreeMsectionForSend(SdpMediaSection::MediaType type,
                                  Sdp* sdp,
                                  SdpMediaSection** msection);
  nsresult CreateAnswerMSection(const JsepAnswerOptions& options,
                                size_t mlineIndex,
                                const SdpMediaSection& remoteMsection,
                                SdpMediaSection* msection,
                                Sdp* sdp);
  nsresult DetermineAnswererSetupRole(const SdpMediaSection& remoteMsection,
                                      SdpSetupAttribute::Role* rolep);
  nsresult NegotiateTrack(const SdpMediaSection& remoteMsection,
                          const SdpMediaSection& localMsection,
                          JsepTrack::Direction,
                          RefPtr<JsepTrack>* track);

  nsresult CreateTransport(const SdpMediaSection& msection,
                           RefPtr<JsepTransport>* transport);

  nsresult SetupTransport(const SdpAttributeList& remote,
                          const SdpAttributeList& answer,
                          const RefPtr<JsepTransport>& transport);

  nsresult AddCandidateToSdp(Sdp* sdp,
                             const std::string& candidate,
                             const std::string& mid,
                             uint16_t level);

  SdpMediaSection* FindMsectionByMid(Sdp& sdp,
                                     const std::string& mid) const;

  const SdpMediaSection* FindMsectionByMid(const Sdp& sdp,
                                           const std::string& mid) const;

  const SdpGroupAttributeList::Group* FindBundleGroup(const Sdp& sdp) const;

  nsresult GetNegotiatedBundleInfo(std::set<std::string>* bundleMids,
                                   const SdpMediaSection** bundleMsection);

  nsresult GetBundleInfo(const Sdp& sdp,
                         std::set<std::string>* bundleMids,
                         const SdpMediaSection** bundleMsection);

  bool IsBundleSlave(const Sdp& localSdp, uint16_t level);

  void DisableMsection(Sdp* sdp, SdpMediaSection* msection) const;
  nsresult EnableMsection(SdpMediaSection* msection);

  nsresult SetUniquePayloadTypes();
  nsresult GetAllPayloadTypes(const JsepTrackNegotiatedDetails& trackDetails,
                              std::vector<uint8_t>* payloadTypesOut);
  std::string GetCNAME(const SdpMediaSection& msection) const;
  bool MsectionIsDisabled(const SdpMediaSection& msection) const;

  std::vector<JsepSendingTrack> mLocalTracks;
  std::vector<JsepReceivingTrack> mRemoteTracks;
  // By the most recent SetRemoteDescription
  std::vector<JsepReceivingTrack> mRemoteTracksAdded;
  std::vector<JsepReceivingTrack> mRemoteTracksRemoved;
  std::vector<RefPtr<JsepTransport> > mTransports;
  std::vector<JsepTrackPair> mNegotiatedTrackPairs;

  bool mIsOfferer;
  bool mIceControlling;
  std::string mIceUfrag;
  std::string mIcePwd;
  bool mRemoteIsIceLite;
  std::vector<std::string> mIceOptions;
  std::vector<JsepDtlsFingerprint> mDtlsFingerprints;
  uint64_t mSessionId;
  uint64_t mSessionVersion;
  std::vector<SdpExtmapAttributeList::Extmap> mAudioRtpExtensions;
  std::vector<SdpExtmapAttributeList::Extmap> mVideoRtpExtensions;
  UniquePtr<JsepUuidGenerator> mUuidGen;
  std::string mDefaultRemoteStreamId;
  std::map<size_t, std::string> mDefaultRemoteTrackIdsByLevel;
  std::string mCNAME;
  UniquePtr<Sdp> mGeneratedLocalDescription; // Created but not set.
  UniquePtr<Sdp> mCurrentLocalDescription;
  UniquePtr<Sdp> mCurrentRemoteDescription;
  UniquePtr<Sdp> mPendingLocalDescription;
  UniquePtr<Sdp> mPendingRemoteDescription;
  std::vector<JsepCodecDescription*> mCodecs;
  std::string mLastError;
  SipccSdpParser mParser;
};

} // namespace mozilla

#endif
