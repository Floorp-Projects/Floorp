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
#include "signaling/src/sdp/SipccSdpParser.h"
#include "signaling/src/sdp/SdpHelper.h"
#include "signaling/src/common/PtrVector.h"

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
        mWasOffererLastTime(false),
        mIceControlling(false),
        mRemoteIsIceLite(false),
        mRemoteIceIsRestarting(false),
        mBundlePolicy(kBundleBalanced),
        mSessionId(0),
        mSessionVersion(0),
        mUuidGen(Move(uuidgen)),
        mSdpHelper(&mLastError)
  {
  }

  // Implement JsepSession methods.
  virtual nsresult Init() override;

  virtual nsresult AddTrack(const RefPtr<JsepTrack>& track) override;

  virtual nsresult RemoveTrack(const std::string& streamId,
                               const std::string& trackId) override;

  virtual nsresult SetIceCredentials(const std::string& ufrag,
                                     const std::string& pwd) override;
  virtual const std::string& GetUfrag() const override { return mIceUfrag; }
  virtual const std::string& GetPwd() const override { return mIcePwd; }
  nsresult SetBundlePolicy(JsepBundlePolicy policy) override;

  virtual bool
  RemoteIsIceLite() const override
  {
    return mRemoteIsIceLite;
  }

  virtual bool
  RemoteIceIsRestarting() const override
  {
    return mRemoteIceIsRestarting;
  }

  virtual std::vector<std::string>
  GetIceOptions() const override
  {
    return mIceOptions;
  }

  virtual nsresult AddDtlsFingerprint(const std::string& algorithm,
                                      const std::vector<uint8_t>& value) override;

  nsresult AddRtpExtension(std::vector<SdpExtmapAttributeList::Extmap>& extensions,
                           const std::string& extensionName,
                           SdpDirectionAttribute::Direction direction);
  virtual nsresult AddAudioRtpExtension(
      const std::string& extensionName,
      SdpDirectionAttribute::Direction direction =
      SdpDirectionAttribute::Direction::kSendrecv) override;

  virtual nsresult AddVideoRtpExtension(
      const std::string& extensionName,
      SdpDirectionAttribute::Direction direction =
      SdpDirectionAttribute::Direction::kSendrecv) override;

  virtual std::vector<JsepCodecDescription*>&
  Codecs() override
  {
    return mSupportedCodecs.values;
  }

  virtual nsresult ReplaceTrack(const std::string& oldStreamId,
                                const std::string& oldTrackId,
                                const std::string& newStreamId,
                                const std::string& newTrackId) override;

  virtual nsresult SetParameters(
      const std::string& streamId,
      const std::string& trackId,
      const std::vector<JsepTrack::JsConstraints>& constraints) override;

  virtual nsresult GetParameters(
      const std::string& streamId,
      const std::string& trackId,
      std::vector<JsepTrack::JsConstraints>* outConstraints) override;

  virtual std::vector<RefPtr<JsepTrack>> GetLocalTracks() const override;

  virtual std::vector<RefPtr<JsepTrack>> GetRemoteTracks() const override;

  virtual std::vector<RefPtr<JsepTrack>>
    GetRemoteTracksAdded() const override;

  virtual std::vector<RefPtr<JsepTrack>>
    GetRemoteTracksRemoved() const override;

  virtual nsresult CreateOffer(const JsepOfferOptions& options,
                               std::string* offer) override;

  virtual nsresult CreateAnswer(const JsepAnswerOptions& options,
                                std::string* answer) override;

  virtual std::string GetLocalDescription(JsepDescriptionPendingOrCurrent type)
                                          const override;

  virtual std::string GetRemoteDescription(JsepDescriptionPendingOrCurrent type)
                                           const override;

  virtual nsresult SetLocalDescription(JsepSdpType type,
                                       const std::string& sdp) override;

  virtual nsresult SetRemoteDescription(JsepSdpType type,
                                        const std::string& sdp) override;

  virtual nsresult AddRemoteIceCandidate(const std::string& candidate,
                                         const std::string& mid,
                                         uint16_t level) override;

  virtual nsresult AddLocalIceCandidate(const std::string& candidate,
                                        uint16_t level,
                                        std::string* mid,
                                        bool* skipped) override;

  virtual nsresult UpdateDefaultCandidate(
      const std::string& defaultCandidateAddr,
      uint16_t defaultCandidatePort,
      const std::string& defaultRtcpCandidateAddr,
      uint16_t defaultRtcpCandidatePort,
      uint16_t level) override;

  virtual nsresult EndOfLocalCandidates(uint16_t level) override;

  virtual nsresult Close() override;

  virtual const std::string GetLastError() const override;

  virtual bool
  IsIceControlling() const override
  {
    return mIceControlling;
  }

  virtual bool
  IsOfferer() const override
  {
    return mIsOfferer;
  }

  // Access transports.
  virtual std::vector<RefPtr<JsepTransport>>
  GetTransports() const override
  {
    return mTransports;
  }

  virtual std::vector<JsepTrackPair>
  GetNegotiatedTrackPairs() const override
  {
    return mNegotiatedTrackPairs;
  }

  virtual bool AllLocalTracksAreAssigned() const override;

private:
  struct JsepDtlsFingerprint {
    std::string mAlgorithm;
    std::vector<uint8_t> mValue;
  };

  struct JsepSendingTrack {
    RefPtr<JsepTrack> mTrack;
    Maybe<size_t> mAssignedMLine;
  };

  struct JsepReceivingTrack {
    RefPtr<JsepTrack> mTrack;
    Maybe<size_t> mAssignedMLine;
  };

  // Non-const so it can set mLastError
  nsresult CreateGenericSDP(UniquePtr<Sdp>* sdp);
  void AddExtmap(SdpMediaSection* msection) const;
  void AddMid(const std::string& mid, SdpMediaSection* msection) const;
  const std::vector<SdpExtmapAttributeList::Extmap>* GetRtpExtensions(
      SdpMediaSection::MediaType type) const;

  void AddCommonExtmaps(const SdpMediaSection& remoteMsection,
                        SdpMediaSection* msection);
  nsresult SetupIds();
  nsresult CreateSsrc(uint32_t* ssrc);
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
  nsresult ValidateOffer(const Sdp& offer);
  nsresult ValidateAnswer(const Sdp& offer, const Sdp& answer);
  nsresult SetRemoteTracksFromDescription(const Sdp* remoteDescription);
  // Non-const because we use our Uuid generator
  nsresult CreateReceivingTrack(size_t mline,
                                const Sdp& sdp,
                                const SdpMediaSection& msection,
                                RefPtr<JsepTrack>* track);
  nsresult HandleNegotiatedSession(const UniquePtr<Sdp>& local,
                                   const UniquePtr<Sdp>& remote);
  nsresult AddTransportAttributes(SdpMediaSection* msection,
                                  SdpSetupAttribute::Role dtlsRole);
  nsresult CopyPreviousTransportParams(const Sdp& oldAnswer,
                                       const Sdp& offerersPreviousSdp,
                                       const Sdp& newOffer,
                                       Sdp* newLocal);
  nsresult SetupOfferMSections(const JsepOfferOptions& options, Sdp* sdp);
  // Non-const so it can assign m-line index to tracks
  nsresult SetupOfferMSectionsByType(SdpMediaSection::MediaType type,
                                     const Maybe<size_t>& offerToReceive,
                                     Sdp* sdp);
  nsresult BindLocalTracks(SdpMediaSection::MediaType mediatype,
                           Sdp* sdp);
  nsresult BindRemoteTracks(SdpMediaSection::MediaType mediatype,
                            Sdp* sdp,
                            size_t* offerToReceive);
  nsresult SetRecvAsNeededOrDisable(SdpMediaSection::MediaType mediatype,
                                    Sdp* sdp,
                                    size_t* offerToRecv);
  void SetupOfferToReceiveMsection(SdpMediaSection* offer);
  nsresult AddRecvonlyMsections(SdpMediaSection::MediaType mediatype,
                                size_t count,
                                Sdp* sdp);
  nsresult AddReofferMsections(const Sdp& oldLocalSdp,
                               const Sdp& oldAnswer,
                               Sdp* newSdp);
  void SetupBundle(Sdp* sdp) const;
  nsresult GetRemoteIds(const Sdp& sdp,
                        const SdpMediaSection& msection,
                        std::string* streamId,
                        std::string* trackId);
  nsresult CreateOfferMSection(SdpMediaSection::MediaType type,
                               SdpMediaSection::Protocol proto,
                               SdpDirectionAttribute::Direction direction,
                               Sdp* sdp);
  nsresult GetFreeMsectionForSend(SdpMediaSection::MediaType type,
                                  Sdp* sdp,
                                  SdpMediaSection** msection);
  nsresult CreateAnswerMSection(const JsepAnswerOptions& options,
                                size_t mlineIndex,
                                const SdpMediaSection& remoteMsection,
                                Sdp* sdp);
  nsresult SetRecvonlySsrc(SdpMediaSection* msection);
  nsresult BindMatchingLocalTrackToAnswer(SdpMediaSection* msection);
  nsresult BindMatchingRemoteTrackToAnswer(SdpMediaSection* msection);
  nsresult DetermineAnswererSetupRole(const SdpMediaSection& remoteMsection,
                                      SdpSetupAttribute::Role* rolep);
  nsresult MakeNegotiatedTrackPair(const SdpMediaSection& remote,
                                   const SdpMediaSection& local,
                                   const RefPtr<JsepTransport>& transport,
                                   bool usingBundle,
                                   size_t transportLevel,
                                   JsepTrackPair* trackPairOut);
  void InitTransport(const SdpMediaSection& msection, JsepTransport* transport);

  nsresult FinalizeTransport(const SdpAttributeList& remote,
                             const SdpAttributeList& answer,
                             const RefPtr<JsepTransport>& transport);

  nsresult GetNegotiatedBundledMids(SdpHelper::BundledMids* bundledMids);

  nsresult EnableOfferMsection(SdpMediaSection* msection);

  mozilla::Sdp* GetParsedLocalDescription(JsepDescriptionPendingOrCurrent type)
                                          const;
  mozilla::Sdp* GetParsedRemoteDescription(JsepDescriptionPendingOrCurrent type)
                                           const;
  const Sdp* GetAnswer() const;

  std::vector<JsepSendingTrack> mLocalTracks;
  std::vector<JsepReceivingTrack> mRemoteTracks;
  // By the most recent SetRemoteDescription
  std::vector<JsepReceivingTrack> mRemoteTracksAdded;
  std::vector<JsepReceivingTrack> mRemoteTracksRemoved;
  std::vector<RefPtr<JsepTransport> > mTransports;
  // So we can rollback
  std::vector<RefPtr<JsepTransport> > mOldTransports;
  std::vector<JsepTrackPair> mNegotiatedTrackPairs;

  bool mIsOfferer;
  bool mWasOffererLastTime;
  bool mIceControlling;
  std::string mIceUfrag;
  std::string mIcePwd;
  bool mRemoteIsIceLite;
  bool mRemoteIceIsRestarting;
  std::vector<std::string> mIceOptions;
  JsepBundlePolicy mBundlePolicy;
  std::vector<JsepDtlsFingerprint> mDtlsFingerprints;
  uint64_t mSessionId;
  uint64_t mSessionVersion;
  std::vector<SdpExtmapAttributeList::Extmap> mAudioRtpExtensions;
  std::vector<SdpExtmapAttributeList::Extmap> mVideoRtpExtensions;
  UniquePtr<JsepUuidGenerator> mUuidGen;
  std::string mDefaultRemoteStreamId;
  std::map<size_t, std::string> mDefaultRemoteTrackIdsByLevel;
  std::string mCNAME;
  // Used to prevent duplicate local SSRCs. Not used to prevent local/remote or
  // remote-only duplication, which will be important for EKT but not now.
  std::set<uint32_t> mSsrcs;
  // When an m-section doesn't have a local track, it still needs an ssrc, which
  // is stored here.
  std::vector<uint32_t> mRecvonlySsrcs;
  UniquePtr<Sdp> mGeneratedLocalDescription; // Created but not set.
  UniquePtr<Sdp> mCurrentLocalDescription;
  UniquePtr<Sdp> mCurrentRemoteDescription;
  UniquePtr<Sdp> mPendingLocalDescription;
  UniquePtr<Sdp> mPendingRemoteDescription;
  PtrVector<JsepCodecDescription> mSupportedCodecs;
  std::string mLastError;
  SipccSdpParser mParser;
  SdpHelper mSdpHelper;
};

} // namespace mozilla

#endif
