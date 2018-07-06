/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RSDPARSA_SDP_ATTRIBUTE_LIST_H_
#define _RSDPARSA_SDP_ATTRIBUTE_LIST_H_

#include "signaling/src/sdp/RsdparsaSdpGlue.h"
#include "signaling/src/sdp/RsdparsaSdpInc.h"
#include "signaling/src/sdp/SdpAttributeList.h"

namespace mozilla
{

class RsdparsaSdp;
class RsdparsaSdpMediaSection;
class SdpErrorHolder;

class RsdparsaSdpAttributeList : public SdpAttributeList
{
  friend class RsdparsaSdpMediaSection;
  friend class RsdparsaSdp;

public:
  // Make sure we don't hide the default arg thunks
  using SdpAttributeList::HasAttribute;
  using SdpAttributeList::GetAttribute;

  bool HasAttribute(AttributeType type,
                    bool sessionFallback) const override;
  const SdpAttribute* GetAttribute(AttributeType type,
                                   bool sessionFallback) const override;
  void SetAttribute(SdpAttribute* attr) override;
  void RemoveAttribute(AttributeType type) override;
  void Clear() override;
  uint32_t Count() const override;

  const SdpConnectionAttribute& GetConnection() const override;
  const SdpFingerprintAttributeList& GetFingerprint() const override;
  const SdpGroupAttributeList& GetGroup() const override;
  const SdpOptionsAttribute& GetIceOptions() const override;
  const SdpRtcpAttribute& GetRtcp() const override;
  const SdpRemoteCandidatesAttribute& GetRemoteCandidates() const override;
  const SdpSetupAttribute& GetSetup() const override;
  const SdpSsrcAttributeList& GetSsrc() const override;
  const SdpDtlsMessageAttribute& GetDtlsMessage() const override;

  // These attributes can appear multiple times, so the returned
  // classes actually represent a collection of values.
  const std::vector<std::string>& GetCandidate() const override;
  const SdpExtmapAttributeList& GetExtmap() const override;
  const SdpFmtpAttributeList& GetFmtp() const override;
  const SdpImageattrAttributeList& GetImageattr() const override;
  const SdpSimulcastAttribute& GetSimulcast() const override;
  const SdpMsidAttributeList& GetMsid() const override;
  const SdpMsidSemanticAttributeList& GetMsidSemantic() const override;
  const SdpRidAttributeList& GetRid() const override;
  const SdpRtcpFbAttributeList& GetRtcpFb() const override;
  const SdpRtpmapAttributeList& GetRtpmap() const override;
  const SdpSctpmapAttributeList& GetSctpmap() const override;

  // These attributes are effectively simple types, so we'll make life
  // easy by just returning their value.
  uint32_t GetSctpPort() const override;
  uint32_t GetMaxMessageSize() const override;
  const std::string& GetIcePwd() const override;
  const std::string& GetIceUfrag() const override;
  const std::string& GetIdentity() const override;
  const std::string& GetLabel() const override;
  unsigned int GetMaxptime() const override;
  const std::string& GetMid() const override;
  unsigned int GetPtime() const override;

  SdpDirectionAttribute::Direction GetDirection() const override;

  void Serialize(std::ostream&) const override;

  virtual ~RsdparsaSdpAttributeList();

private:
  explicit RsdparsaSdpAttributeList(RsdparsaSessionHandle session)
    : mSession(std::move(session))
    , mSessionAttributes(nullptr)
    , mIsVideo(false)
    , mAttributes()
  {
    RustAttributeList* attributes = get_sdp_session_attributes(mSession.get());
    LoadAll(attributes);
  }

  RsdparsaSdpAttributeList(RsdparsaSessionHandle session,
                           const RustMediaSection* const msection,
                           const RsdparsaSdpAttributeList* sessionAttributes)
    : mSession(std::move(session))
    , mSessionAttributes(sessionAttributes)
    , mAttributes()
  {
    mIsVideo = sdp_rust_get_media_type(msection) == RustSdpMediaValue::kRustVideo;
    RustAttributeList* attributes = sdp_get_media_attribute_list(msection);
    LoadAll(attributes);
  }

  static const std::string kEmptyString;
  static const size_t kNumAttributeTypes = SdpAttribute::kLastAttribute + 1;

  const RsdparsaSessionHandle mSession;
  const RsdparsaSdpAttributeList* mSessionAttributes;
  bool mIsVideo;

  bool
  AtSessionLevel() const
  {
    return !mSessionAttributes;
  }

  bool IsAllowedHere(SdpAttribute::AttributeType type);
  void LoadAll(RustAttributeList* attributeList);
  void LoadAttribute(RustAttributeList* attributeList, AttributeType type);
  void LoadIceUfrag(RustAttributeList* attributeList);
  void LoadIcePwd(RustAttributeList* attributeList);
  void LoadIdentity(RustAttributeList* attributeList);
  void LoadIceOptions(RustAttributeList* attributeList);
  void LoadFingerprint(RustAttributeList* attributeList);
  void LoadSetup(RustAttributeList* attributeList);
  void LoadSsrc(RustAttributeList* attributeList);
  void LoadRtpmap(RustAttributeList* attributeList);
  void LoadFmtp(RustAttributeList* attributeList);
  void LoadPtime(RustAttributeList* attributeList);
  void LoadFlags(RustAttributeList* attributeList);
  void LoadMaxMessageSize(RustAttributeList* attributeList);
  void LoadMid(RustAttributeList* attributeList);
  void LoadMsid(RustAttributeList* attributeList);
  void LoadMsidSemantics(RustAttributeList* attributeList);
  void LoadGroup(RustAttributeList* attributeList);
  void LoadRtcp(RustAttributeList* attributeList);
  void LoadRtcpFb(RustAttributeList* attributeList);
  void LoadSctpPort(RustAttributeList* attributeList);
  void LoadSimulcast(RustAttributeList* attributeList);
  void LoadImageattr(RustAttributeList* attributeList);
  void LoadSctpmaps(RustAttributeList* attributeList);
  void LoadDirection(RustAttributeList* attributeList);
  void LoadRemoteCandidates(RustAttributeList* attributeList);
  void LoadRids(RustAttributeList* attributeList);
  void LoadExtmap(RustAttributeList* attributeList);
  void LoadMaxPtime(RustAttributeList* attributeList);

  void WarnAboutMisplacedAttribute(SdpAttribute::AttributeType type,
                                   uint32_t lineNumber,
                                   SdpErrorHolder& errorHolder);


  SdpAttribute* mAttributes[kNumAttributeTypes];

  RsdparsaSdpAttributeList(const RsdparsaSdpAttributeList& orig) = delete;
  RsdparsaSdpAttributeList& operator=(const RsdparsaSdpAttributeList& rhs) = delete;
};

} // namespace mozilla

#endif
