/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SIPCCSDPATTRIBUTELIST_H_
#define _SIPCCSDPATTRIBUTELIST_H_

#include "signaling/src/sdp/SdpAttributeList.h"

extern "C" {
#include "signaling/src/sdp/sipcc/sdp.h"
}

namespace mozilla
{

class SipccSdp;
class SipccSdpMediaSection;
class SdpErrorHolder;

class SipccSdpAttributeList : public SdpAttributeList
{
  friend class SipccSdpMediaSection;
  friend class SipccSdp;

public:
  // Make sure we don't hide the default arg thunks
  using SdpAttributeList::HasAttribute;
  using SdpAttributeList::GetAttribute;

  virtual bool HasAttribute(AttributeType type,
                            bool sessionFallback) const MOZ_OVERRIDE;
  virtual const SdpAttribute* GetAttribute(
      AttributeType type, bool sessionFallback) const MOZ_OVERRIDE;
  virtual void SetAttribute(SdpAttribute* attr) MOZ_OVERRIDE;
  virtual void RemoveAttribute(AttributeType type) MOZ_OVERRIDE;
  virtual void Clear() MOZ_OVERRIDE;

  virtual const SdpConnectionAttribute& GetConnection() const MOZ_OVERRIDE;
  virtual const SdpFingerprintAttributeList& GetFingerprint() const
      MOZ_OVERRIDE;
  virtual const SdpGroupAttributeList& GetGroup() const MOZ_OVERRIDE;
  virtual const SdpOptionsAttribute& GetIceOptions() const MOZ_OVERRIDE;
  virtual const SdpRtcpAttribute& GetRtcp() const MOZ_OVERRIDE;
  virtual const SdpRemoteCandidatesAttribute& GetRemoteCandidates() const
      MOZ_OVERRIDE;
  virtual const SdpSetupAttribute& GetSetup() const MOZ_OVERRIDE;
  virtual const SdpSsrcAttributeList& GetSsrc() const MOZ_OVERRIDE;
  virtual const SdpSsrcGroupAttributeList& GetSsrcGroup() const MOZ_OVERRIDE;

  // These attributes can appear multiple times, so the returned
  // classes actually represent a collection of values.
  virtual const std::vector<std::string>& GetCandidate() const MOZ_OVERRIDE;
  virtual const SdpExtmapAttributeList& GetExtmap() const MOZ_OVERRIDE;
  virtual const SdpFmtpAttributeList& GetFmtp() const MOZ_OVERRIDE;
  virtual const SdpImageattrAttributeList& GetImageattr() const MOZ_OVERRIDE;
  virtual const SdpMsidAttributeList& GetMsid() const MOZ_OVERRIDE;
  virtual const SdpRtcpFbAttributeList& GetRtcpFb() const MOZ_OVERRIDE;
  virtual const SdpRtpmapAttributeList& GetRtpmap() const MOZ_OVERRIDE;
  virtual const SdpSctpmapAttributeList& GetSctpmap() const MOZ_OVERRIDE;

  // These attributes are effectively simple types, so we'll make life
  // easy by just returning their value.
  virtual const std::string& GetIcePwd() const MOZ_OVERRIDE;
  virtual const std::string& GetIceUfrag() const MOZ_OVERRIDE;
  virtual const std::string& GetIdentity() const MOZ_OVERRIDE;
  virtual const std::string& GetLabel() const MOZ_OVERRIDE;
  virtual unsigned int GetMaxptime() const MOZ_OVERRIDE;
  virtual const std::string& GetMid() const MOZ_OVERRIDE;
  virtual const SdpMsidSemanticAttributeList& GetMsidSemantic()
    const MOZ_OVERRIDE;
  virtual unsigned int GetPtime() const MOZ_OVERRIDE;

  virtual SdpDirectionAttribute::Direction GetDirection() const MOZ_OVERRIDE;

  virtual void Serialize(std::ostream&) const MOZ_OVERRIDE;

  virtual ~SipccSdpAttributeList();

private:
  static const std::string kEmptyString;
  static const size_t kNumAttributeTypes = SdpAttribute::kLastAttribute + 1;

  // Pass a session-level attribute list if constructing a media-level one,
  // otherwise pass nullptr
  explicit SipccSdpAttributeList(const SipccSdpAttributeList* sessionLevel);

  bool Load(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  void LoadSimpleStrings(sdp_t* sdp, uint16_t level,
                         SdpErrorHolder& errorHolder);
  void LoadSimpleString(sdp_t* sdp, uint16_t level, sdp_attr_e attr,
                        AttributeType targetType, SdpErrorHolder& errorHolder);
  void LoadSimpleNumbers(sdp_t* sdp, uint16_t level,
                         SdpErrorHolder& errorHolder);
  void LoadSimpleNumber(sdp_t* sdp, uint16_t level, sdp_attr_e attr,
                        AttributeType targetType, SdpErrorHolder& errorHolder);
  void LoadFlags(sdp_t* sdp, uint16_t level);
  void LoadDirection(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  bool LoadRtpmap(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  bool LoadSctpmap(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  void LoadIceAttributes(sdp_t* sdp, uint16_t level);
  bool LoadFingerprint(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  void LoadCandidate(sdp_t* sdp, uint16_t level);
  void LoadSetup(sdp_t* sdp, uint16_t level);
  void LoadSsrc(sdp_t* sdp, uint16_t level);
  bool LoadGroups(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  bool LoadMsidSemantics(sdp_t* sdp,
                         uint16_t level,
                         SdpErrorHolder& errorHolder);
  void LoadFmtp(sdp_t* sdp, uint16_t level);
  void LoadMsids(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  void LoadExtmap(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  void LoadRtcpFb(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  static SdpRtpmapAttributeList::CodecType GetCodecType(rtp_ptype type);

  bool
  AtSessionLevel() const
  {
    return !mSessionLevel;
  }
  bool IsAllowedHere(SdpAttribute::AttributeType type) const;
  void WarnAboutMisplacedAttribute(SdpAttribute::AttributeType type,
                                   uint32_t lineNumber,
                                   SdpErrorHolder& errorHolder);

  const SipccSdpAttributeList* mSessionLevel;

  SdpAttribute* mAttributes[kNumAttributeTypes];

  SipccSdpAttributeList(const SipccSdpAttributeList& orig) = delete;
  SipccSdpAttributeList& operator=(const SipccSdpAttributeList& rhs) = delete;
};

} // namespace mozilla

#endif
