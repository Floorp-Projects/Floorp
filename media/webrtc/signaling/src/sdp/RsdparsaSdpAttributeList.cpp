/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCRT.h"

#include "signaling/src/sdp/RsdparsaSdpAttributeList.h"
#include "signaling/src/sdp/RsdparsaSdpInc.h"
#include "signaling/src/sdp/RsdparsaSdpGlue.h"

#include <ostream>
#include "mozilla/Assertions.h"

#include <limits>

namespace mozilla
{

const std::string RsdparsaSdpAttributeList::kEmptyString = "";

RsdparsaSdpAttributeList::~RsdparsaSdpAttributeList()
{
  for (size_t i = 0; i < kNumAttributeTypes; ++i) {
    delete mAttributes[i];
  }
}

bool
RsdparsaSdpAttributeList::HasAttribute(AttributeType type,
                                       bool sessionFallback) const
{
  return !!GetAttribute(type, sessionFallback);
}

const SdpAttribute*
RsdparsaSdpAttributeList::GetAttribute(AttributeType type,
                                       bool sessionFallback) const
{
  const SdpAttribute* value = mAttributes[static_cast<size_t>(type)];
  // Only do fallback when the attribute can appear at both the media and
  // session level
  if (!value && !AtSessionLevel() && sessionFallback &&
      SdpAttribute::IsAllowedAtSessionLevel(type) &&
      SdpAttribute::IsAllowedAtMediaLevel(type)) {
    return mSessionAttributes->GetAttribute(type, false);
  }
  return value;
}

void
RsdparsaSdpAttributeList::RemoveAttribute(AttributeType type)
{
  delete mAttributes[static_cast<size_t>(type)];
  mAttributes[static_cast<size_t>(type)] = nullptr;
}

void
RsdparsaSdpAttributeList::Clear()
{
  for (size_t i = 0; i < kNumAttributeTypes; ++i) {
    RemoveAttribute(static_cast<AttributeType>(i));
  }
}

uint32_t
RsdparsaSdpAttributeList::Count() const
{
  uint32_t count = 0;
  for (size_t i = 0; i < kNumAttributeTypes; ++i) {
    if (mAttributes[i]) {
      count++;
    }
  }
  return count;
}

void
RsdparsaSdpAttributeList::SetAttribute(SdpAttribute* attr)
{
  if (!IsAllowedHere(attr->GetType())) {
    MOZ_ASSERT(false, "This type of attribute is not allowed here");
    delete attr;
    return;
  }
  RemoveAttribute(attr->GetType());
  mAttributes[attr->GetType()] = attr;
}

const std::vector<std::string>&
RsdparsaSdpAttributeList::GetCandidate() const
{
  if (!HasAttribute(SdpAttribute::kCandidateAttribute)) {
    MOZ_CRASH();
  }

  return static_cast<const SdpMultiStringAttribute*>(
             GetAttribute(SdpAttribute::kCandidateAttribute))->mValues;
}

const SdpConnectionAttribute&
RsdparsaSdpAttributeList::GetConnection() const
{
  if (!HasAttribute(SdpAttribute::kConnectionAttribute)) {
    MOZ_CRASH();
  }

  return *static_cast<const SdpConnectionAttribute*>(
             GetAttribute(SdpAttribute::kConnectionAttribute));
}

SdpDirectionAttribute::Direction
RsdparsaSdpAttributeList::GetDirection() const
{
  if (!HasAttribute(SdpAttribute::kDirectionAttribute)) {
    MOZ_CRASH();
  }

  const SdpAttribute* attr = GetAttribute(SdpAttribute::kDirectionAttribute);
  return static_cast<const SdpDirectionAttribute*>(attr)->mValue;
}

const SdpDtlsMessageAttribute&
RsdparsaSdpAttributeList::GetDtlsMessage() const
{
  if (!HasAttribute(SdpAttribute::kDtlsMessageAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kDtlsMessageAttribute);
  return *static_cast<const SdpDtlsMessageAttribute*>(attr);
}

const SdpExtmapAttributeList&
RsdparsaSdpAttributeList::GetExtmap() const
{
  if (!HasAttribute(SdpAttribute::kExtmapAttribute)) {
    MOZ_CRASH();
  }

  return *static_cast<const SdpExtmapAttributeList*>(
             GetAttribute(SdpAttribute::kExtmapAttribute));
}

const SdpFingerprintAttributeList&
RsdparsaSdpAttributeList::GetFingerprint() const
{
  if (!HasAttribute(SdpAttribute::kFingerprintAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kFingerprintAttribute);
  return *static_cast<const SdpFingerprintAttributeList*>(attr);
}

const SdpFmtpAttributeList&
RsdparsaSdpAttributeList::GetFmtp() const
{
  if (!HasAttribute(SdpAttribute::kFmtpAttribute)) {
    MOZ_CRASH();
  }

  return *static_cast<const SdpFmtpAttributeList*>(
             GetAttribute(SdpAttribute::kFmtpAttribute));
}

const SdpGroupAttributeList&
RsdparsaSdpAttributeList::GetGroup() const
{
  if (!HasAttribute(SdpAttribute::kGroupAttribute)) {
    MOZ_CRASH();
  }

  return *static_cast<const SdpGroupAttributeList*>(
             GetAttribute(SdpAttribute::kGroupAttribute));
}

const SdpOptionsAttribute&
RsdparsaSdpAttributeList::GetIceOptions() const
{
  if (!HasAttribute(SdpAttribute::kIceOptionsAttribute)) {
    MOZ_CRASH();
  }

  const SdpAttribute* attr = GetAttribute(SdpAttribute::kIceOptionsAttribute);
  return *static_cast<const SdpOptionsAttribute*>(attr);
}

const std::string&
RsdparsaSdpAttributeList::GetIcePwd() const
{
  if (!HasAttribute(SdpAttribute::kIcePwdAttribute)) {
    return kEmptyString;
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kIcePwdAttribute);
  return static_cast<const SdpStringAttribute*>(attr)->mValue;
}

const std::string&
RsdparsaSdpAttributeList::GetIceUfrag() const
{
  if (!HasAttribute(SdpAttribute::kIceUfragAttribute)) {
    return kEmptyString;
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kIceUfragAttribute);
  return static_cast<const SdpStringAttribute*>(attr)->mValue;
}

const std::string&
RsdparsaSdpAttributeList::GetIdentity() const
{
  if (!HasAttribute(SdpAttribute::kIdentityAttribute)) {
    return kEmptyString;
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kIdentityAttribute);
  return static_cast<const SdpStringAttribute*>(attr)->mValue;
}

const SdpImageattrAttributeList&
RsdparsaSdpAttributeList::GetImageattr() const
{
  if (!HasAttribute(SdpAttribute::kImageattrAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kImageattrAttribute);
  return *static_cast<const SdpImageattrAttributeList*>(attr);
}

const SdpSimulcastAttribute&
RsdparsaSdpAttributeList::GetSimulcast() const
{
  if (!HasAttribute(SdpAttribute::kSimulcastAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kSimulcastAttribute);
  return *static_cast<const SdpSimulcastAttribute*>(attr);
}

const std::string&
RsdparsaSdpAttributeList::GetLabel() const
{
  if (!HasAttribute(SdpAttribute::kLabelAttribute)) {
    return kEmptyString;
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kLabelAttribute);
  return static_cast<const SdpStringAttribute*>(attr)->mValue;
}

uint32_t
RsdparsaSdpAttributeList::GetMaxptime() const
{
  if (!HasAttribute(SdpAttribute::kMaxptimeAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kMaxptimeAttribute);
  return static_cast<const SdpNumberAttribute*>(attr)->mValue;
}

const std::string&
RsdparsaSdpAttributeList::GetMid() const
{
  if (!HasAttribute(SdpAttribute::kMidAttribute)) {
    return kEmptyString;
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kMidAttribute);
  return static_cast<const SdpStringAttribute*>(attr)->mValue;
}

const SdpMsidAttributeList&
RsdparsaSdpAttributeList::GetMsid() const
{
  if (!HasAttribute(SdpAttribute::kMsidAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kMsidAttribute);
  return *static_cast<const SdpMsidAttributeList*>(attr);
}

const SdpMsidSemanticAttributeList&
RsdparsaSdpAttributeList::GetMsidSemantic() const
{
  if (!HasAttribute(SdpAttribute::kMsidSemanticAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kMsidSemanticAttribute);
  return *static_cast<const SdpMsidSemanticAttributeList*>(attr);
}

const SdpRidAttributeList&
RsdparsaSdpAttributeList::GetRid() const
{
  if (!HasAttribute(SdpAttribute::kRidAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kRidAttribute);
  return *static_cast<const SdpRidAttributeList*>(attr);
}

uint32_t
RsdparsaSdpAttributeList::GetPtime() const
{
  if (!HasAttribute(SdpAttribute::kPtimeAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kPtimeAttribute);
  return static_cast<const SdpNumberAttribute*>(attr)->mValue;
}

const SdpRtcpAttribute&
RsdparsaSdpAttributeList::GetRtcp() const
{
  if (!HasAttribute(SdpAttribute::kRtcpAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kRtcpAttribute);
  return *static_cast<const SdpRtcpAttribute*>(attr);
}

const SdpRtcpFbAttributeList&
RsdparsaSdpAttributeList::GetRtcpFb() const
{
  if (!HasAttribute(SdpAttribute::kRtcpFbAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kRtcpFbAttribute);
  return *static_cast<const SdpRtcpFbAttributeList*>(attr);
}

const SdpRemoteCandidatesAttribute&
RsdparsaSdpAttributeList::GetRemoteCandidates() const
{
  MOZ_CRASH("Not yet implemented");
}

const SdpRtpmapAttributeList&
RsdparsaSdpAttributeList::GetRtpmap() const
{
  if (!HasAttribute(SdpAttribute::kRtpmapAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kRtpmapAttribute);
  return *static_cast<const SdpRtpmapAttributeList*>(attr);
}

const SdpSctpmapAttributeList&
RsdparsaSdpAttributeList::GetSctpmap() const
{
  if (!HasAttribute(SdpAttribute::kSctpmapAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kSctpmapAttribute);
  return *static_cast<const SdpSctpmapAttributeList*>(attr);
}

uint32_t
RsdparsaSdpAttributeList::GetSctpPort() const
{
  if (!HasAttribute(SdpAttribute::kSctpPortAttribute)) {
    MOZ_CRASH();
  }

  const SdpAttribute* attr = GetAttribute(SdpAttribute::kSctpPortAttribute);
  return static_cast<const SdpNumberAttribute*>(attr)->mValue;
}

uint32_t
RsdparsaSdpAttributeList::GetMaxMessageSize() const
{
  if (!HasAttribute(SdpAttribute::kMaxMessageSizeAttribute)) {
    MOZ_CRASH();
  }

  const SdpAttribute* attr = GetAttribute(SdpAttribute::kMaxMessageSizeAttribute);
  return static_cast<const SdpNumberAttribute*>(attr)->mValue;
}

const SdpSetupAttribute&
RsdparsaSdpAttributeList::GetSetup() const
{
  if (!HasAttribute(SdpAttribute::kSetupAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kSetupAttribute);
  return *static_cast<const SdpSetupAttribute*>(attr);
}

const SdpSsrcAttributeList&
RsdparsaSdpAttributeList::GetSsrc() const
{
  if (!HasAttribute(SdpAttribute::kSsrcAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kSsrcAttribute);
  return *static_cast<const SdpSsrcAttributeList*>(attr);
}

void
RsdparsaSdpAttributeList::LoadAttribute(RustAttributeList *attributeList,
                                        AttributeType type)
{
  if(!mAttributes[type]) {
    switch(type) {
      case SdpAttribute::kIceUfragAttribute:
        LoadIceUfrag(attributeList);
        return;
      case SdpAttribute::kIcePwdAttribute:
        LoadIcePwd(attributeList);
        return;
      case SdpAttribute::kIceOptionsAttribute:
        LoadIceOptions(attributeList);
        return;
      case SdpAttribute::kDtlsMessageAttribute:
        LoadDtlsMessage(attributeList);
        return;
      case SdpAttribute::kFingerprintAttribute:
        LoadFingerprint(attributeList);
        return;
      case SdpAttribute::kIdentityAttribute:
        LoadIdentity(attributeList);
        return;
      case SdpAttribute::kSetupAttribute:
        LoadSetup(attributeList);
        return;
      case SdpAttribute::kSsrcAttribute:
        LoadSsrc(attributeList);
        return;
      case SdpAttribute::kRtpmapAttribute:
        LoadRtpmap(attributeList);
        return;
      case SdpAttribute::kFmtpAttribute:
        LoadFmtp(attributeList);
        return;
      case SdpAttribute::kPtimeAttribute:
        LoadPtime(attributeList);
        return;
      case SdpAttribute::kIceLiteAttribute:
      case SdpAttribute::kRtcpMuxAttribute:
      case SdpAttribute::kBundleOnlyAttribute:
      case SdpAttribute::kEndOfCandidatesAttribute:
        LoadFlags(attributeList);
        return;
      case SdpAttribute::kMaxMessageSizeAttribute:
        LoadMaxMessageSize(attributeList);
        return ;
      case SdpAttribute::kMidAttribute:
        LoadMid(attributeList);
        return;
      case SdpAttribute::kMsidAttribute:
        LoadMsid(attributeList);
        return;
      case SdpAttribute::kMsidSemanticAttribute:
        LoadMsidSemantics(attributeList);
        return;
      case SdpAttribute::kGroupAttribute:
        LoadGroup(attributeList);
        return;
      case SdpAttribute::kRtcpAttribute:
        LoadRtcp(attributeList);
        return;
      case SdpAttribute::kRtcpFbAttribute:
        LoadRtcpFb(attributeList);
        return;
      case SdpAttribute::kImageattrAttribute:
        LoadImageattr(attributeList);
        return;
      case SdpAttribute::kSctpmapAttribute:
        LoadSctpmaps(attributeList);
        return;
      case SdpAttribute::kDirectionAttribute:
        LoadDirection(attributeList);
        return;
      case SdpAttribute::kRemoteCandidatesAttribute:
        LoadRemoteCandidates(attributeList);
        return;
      case SdpAttribute::kRidAttribute:
        LoadRids(attributeList);
        return;
      case SdpAttribute::kSctpPortAttribute:
        LoadSctpPort(attributeList);
        return ;
      case SdpAttribute::kExtmapAttribute:
        LoadExtmap(attributeList);
        return;
      case SdpAttribute::kSimulcastAttribute:
        LoadSimulcast(attributeList);
        return;
      case SdpAttribute::kMaxptimeAttribute:
        LoadMaxPtime(attributeList);
        return;

      case SdpAttribute::kLabelAttribute:
      case SdpAttribute::kSsrcGroupAttribute:
      case SdpAttribute::kRtcpRsizeAttribute:
      case SdpAttribute::kCandidateAttribute:
      case SdpAttribute::kConnectionAttribute:
      case SdpAttribute::kIceMismatchAttribute:
        // TODO: Not implemented, or not applicable.
        // Sort this out in Bug 1437165.
        return;
    }
  }
}

void
RsdparsaSdpAttributeList::LoadAll(RustAttributeList *attributeList)
{
  for (int i = SdpAttribute::kFirstAttribute; i <= SdpAttribute::kLastAttribute; i++) {
    LoadAttribute(attributeList, static_cast<SdpAttribute::AttributeType>(i));
  }
}

void
RsdparsaSdpAttributeList::LoadIceUfrag(RustAttributeList* attributeList)
{
  StringView ufragStr;
  nsresult nr = sdp_get_iceufrag(attributeList, &ufragStr);
  if (NS_SUCCEEDED(nr)) {
    std::string iceufrag = convertStringView(ufragStr);
    SetAttribute(new SdpStringAttribute(SdpAttribute::kIceUfragAttribute,
                                        iceufrag));
  }
}

void
RsdparsaSdpAttributeList::LoadIcePwd(RustAttributeList* attributeList)
{
  StringView pwdStr;
  nsresult nr = sdp_get_icepwd(attributeList, &pwdStr);
  if (NS_SUCCEEDED(nr)) {
    std::string icePwd = convertStringView(pwdStr);
    SetAttribute(new SdpStringAttribute(SdpAttribute::kIcePwdAttribute,
                                        icePwd));
  }
}

void
RsdparsaSdpAttributeList::LoadIdentity(RustAttributeList* attributeList)
{
  StringView identityStr;
  nsresult nr = sdp_get_identity(attributeList, &identityStr);
  if (NS_SUCCEEDED(nr)) {
    std::string identity = convertStringView(identityStr);
    SetAttribute(new SdpStringAttribute(SdpAttribute::kIdentityAttribute,
                                        identity));
  }
}

void
RsdparsaSdpAttributeList::LoadIceOptions(RustAttributeList* attributeList)
{
  StringVec* options;
  nsresult nr = sdp_get_iceoptions(attributeList, &options);
  if (NS_SUCCEEDED(nr)) {
    std::vector<std::string> optionsVec;
    auto optionsAttr = MakeUnique<SdpOptionsAttribute>(
                                  SdpAttribute::kIceOptionsAttribute);
    for (size_t i = 0; i < string_vec_len(options); i++) {
      StringView optionStr;
      string_vec_get_view(options, i, &optionStr);
      optionsAttr->PushEntry(convertStringView(optionStr));
    }
    SetAttribute(optionsAttr.release());
  }
}

void
RsdparsaSdpAttributeList::LoadFingerprint(RustAttributeList* attributeList)
{
  size_t nFp = sdp_get_fingerprint_count(attributeList);
  if (nFp == 0) {
    return;
  }
  auto rustFingerprints = MakeUnique<RustSdpAttributeFingerprint[]>(nFp);
  sdp_get_fingerprints(attributeList, nFp, rustFingerprints.get());
  auto fingerprints = MakeUnique<SdpFingerprintAttributeList>();
  for(size_t i = 0; i < nFp; i++) {
    const RustSdpAttributeFingerprint& fingerprint = rustFingerprints[i];
    std::string algorithm;
    switch(fingerprint.hashAlgorithm) {
      case RustSdpAttributeFingerprintHashAlgorithm::kSha1:
        algorithm = "sha-1";
        break;
      case RustSdpAttributeFingerprintHashAlgorithm::kSha224:
        algorithm = "sha-224";
        break;
      case RustSdpAttributeFingerprintHashAlgorithm::kSha256:
        algorithm = "sha-256";
        break;
      case RustSdpAttributeFingerprintHashAlgorithm::kSha384:
        algorithm = "sha-384";
        break;
      case RustSdpAttributeFingerprintHashAlgorithm::kSha512:
        algorithm = "sha-512";
        break;
    }

    std::vector<uint8_t> fingerprintBytes =
                                    convertU8Vec(fingerprint.fingerprint);

    fingerprints->PushEntry(algorithm, fingerprintBytes);
  }
  SetAttribute(fingerprints.release());
}

void
RsdparsaSdpAttributeList::LoadDtlsMessage(RustAttributeList* attributeList)
{
  RustSdpAttributeDtlsMessage rustDtlsMessage;
  nsresult nr = sdp_get_dtls_message(attributeList, &rustDtlsMessage);
  if (NS_SUCCEEDED(nr)) {
    SdpDtlsMessageAttribute::Role role;
    if (rustDtlsMessage.role == RustSdpAttributeDtlsMessageType::kClient) {
      role = SdpDtlsMessageAttribute::kClient;
    } else {
      role = SdpDtlsMessageAttribute::kServer;
    }

    std::string value = convertStringView(rustDtlsMessage.value);

    SetAttribute(new SdpDtlsMessageAttribute(role, value));
  }
}

void
RsdparsaSdpAttributeList::LoadSetup(RustAttributeList* attributeList)
{
  RustSdpSetup rustSetup;
  nsresult nr = sdp_get_setup(attributeList, &rustSetup);
  if (NS_SUCCEEDED(nr)) {
    SdpSetupAttribute::Role setupEnum;
    switch(rustSetup) {
      case RustSdpSetup::kRustActive:
        setupEnum = SdpSetupAttribute::kActive;
        break;
      case RustSdpSetup::kRustActpass:
        setupEnum = SdpSetupAttribute::kActpass;
        break;
      case RustSdpSetup::kRustHoldconn:
        setupEnum = SdpSetupAttribute::kHoldconn;
        break;
      case RustSdpSetup::kRustPassive:
        setupEnum = SdpSetupAttribute::kPassive;
        break;
    }
    SetAttribute(new SdpSetupAttribute(setupEnum));
  }
}

void
RsdparsaSdpAttributeList::LoadSsrc(RustAttributeList* attributeList)
{
  size_t numSsrc = sdp_get_ssrc_count(attributeList);
  if (numSsrc == 0) {
    return;
  }
  auto rustSsrcs = MakeUnique<RustSdpAttributeSsrc[]>(numSsrc);
  sdp_get_ssrcs(attributeList, numSsrc, rustSsrcs.get());
  auto ssrcs = MakeUnique<SdpSsrcAttributeList>();
  for(size_t i = 0; i < numSsrc; i++) {
    RustSdpAttributeSsrc& ssrc = rustSsrcs[i];
    std::string attribute = convertStringView(ssrc.attribute);
    std::string value = convertStringView(ssrc.value);
    if (value.length() == 0) {
      ssrcs->PushEntry(ssrc.id, attribute);
    } else {
      ssrcs->PushEntry(ssrc.id, attribute + ":" + value);
    }
  }
  SetAttribute(ssrcs.release());
}

SdpRtpmapAttributeList::CodecType
strToCodecType(const std::string &name)
{
  auto codec = SdpRtpmapAttributeList::kOtherCodec;
  if (!nsCRT::strcasecmp(name.c_str(), "opus")) {
    codec = SdpRtpmapAttributeList::kOpus;
  } else if (!nsCRT::strcasecmp(name.c_str(), "G722")) {
    codec = SdpRtpmapAttributeList::kG722;
  } else if (!nsCRT::strcasecmp(name.c_str(), "PCMU")) {
    codec = SdpRtpmapAttributeList::kPCMU;
  } else if (!nsCRT::strcasecmp(name.c_str(), "PCMA")) {
    codec = SdpRtpmapAttributeList::kPCMA;
  } else if (!nsCRT::strcasecmp(name.c_str(), "VP8")) {
    codec = SdpRtpmapAttributeList::kVP8;
  } else if (!nsCRT::strcasecmp(name.c_str(), "VP9")) {
    codec = SdpRtpmapAttributeList::kVP9;
  } else if (!nsCRT::strcasecmp(name.c_str(), "iLBC")) {
    codec = SdpRtpmapAttributeList::kiLBC;
  } else if(!nsCRT::strcasecmp(name.c_str(), "iSAC")) {
    codec = SdpRtpmapAttributeList::kiSAC;
  } else if (!nsCRT::strcasecmp(name.c_str(), "H264")) {
    codec = SdpRtpmapAttributeList::kH264;
  } else if (!nsCRT::strcasecmp(name.c_str(), "red")) {
    codec = SdpRtpmapAttributeList::kRed;
  } else if (!nsCRT::strcasecmp(name.c_str(), "ulpfec")) {
    codec = SdpRtpmapAttributeList::kUlpfec;
  } else if (!nsCRT::strcasecmp(name.c_str(), "telephone-event")) {
    codec = SdpRtpmapAttributeList::kTelephoneEvent;
  }
  return codec;
}

void
RsdparsaSdpAttributeList::LoadRtpmap(RustAttributeList* attributeList)
{
  size_t numRtpmap = sdp_get_rtpmap_count(attributeList);
  if (numRtpmap == 0) {
    return;
  }
  auto rustRtpmaps = MakeUnique<RustSdpAttributeRtpmap[]>(numRtpmap);
  sdp_get_rtpmaps(attributeList, numRtpmap, rustRtpmaps.get());
  auto rtpmapList = MakeUnique<SdpRtpmapAttributeList>();
  for(size_t i = 0; i < numRtpmap; i++) {
    RustSdpAttributeRtpmap& rtpmap = rustRtpmaps[i];
    std::string payloadType = std::to_string(rtpmap.payloadType);
    std::string name = convertStringView(rtpmap.codecName);
    auto codec = strToCodecType(name);
    uint32_t channels = rtpmap.channels;
    rtpmapList->PushEntry(payloadType, codec, name,
                          rtpmap.frequency, channels);
  }
  SetAttribute(rtpmapList.release());
}

void
RsdparsaSdpAttributeList::LoadFmtp(RustAttributeList* attributeList)
{
  size_t numFmtp = sdp_get_fmtp_count(attributeList);
    if (numFmtp == 0) {
    return;
  }
  auto rustFmtps = MakeUnique<RustSdpAttributeFmtp[]>(numFmtp);
  size_t numValidFmtp = sdp_get_fmtp(attributeList, numFmtp,rustFmtps.get());
  auto fmtpList = MakeUnique<SdpFmtpAttributeList>();
  for(size_t i = 0; i < numValidFmtp; i++) {
    const RustSdpAttributeFmtp& fmtp = rustFmtps[i];
    uint8_t payloadType = fmtp.payloadType;
    std::string codecName = convertStringView(fmtp.codecName);
    const RustSdpAttributeFmtpParameters& rustFmtpParameters = fmtp.parameters;

    UniquePtr<SdpFmtpAttributeList::Parameters> fmtpParameters;

    // use the upper case version of the codec name
    std::transform(codecName.begin(), codecName.end(),
                   codecName.begin(), ::toupper);

    if(codecName == "H264"){
      SdpFmtpAttributeList::H264Parameters h264Parameters;

      h264Parameters.packetization_mode = rustFmtpParameters.packetization_mode;
      h264Parameters.level_asymmetry_allowed =
                                  rustFmtpParameters.level_asymmetry_allowed;
      h264Parameters.profile_level_id = rustFmtpParameters.profile_level_id;
      h264Parameters.max_mbps = rustFmtpParameters.max_mbps;
      h264Parameters.max_fs = rustFmtpParameters.max_fs;
      h264Parameters.max_cpb = rustFmtpParameters.max_cpb;
      h264Parameters.max_dpb = rustFmtpParameters.max_dpb;
      h264Parameters.max_br = rustFmtpParameters.max_br;

      // TODO(bug 1466859): Support sprop-parameter-sets

      fmtpParameters.reset(new SdpFmtpAttributeList::H264Parameters(
                                                    std::move(h264Parameters)));
    } else if(codecName == "OPUS"){
      SdpFmtpAttributeList::OpusParameters opusParameters;

      opusParameters.maxplaybackrate = rustFmtpParameters.maxplaybackrate;
      opusParameters.stereo = rustFmtpParameters.stereo;
      opusParameters.useInBandFec = rustFmtpParameters.useinbandfec;

      fmtpParameters.reset(new SdpFmtpAttributeList::OpusParameters(
                                                    std::move(opusParameters)));
    } else if((codecName == "VP8") || (codecName == "VP9")){
      SdpFmtpAttributeList::VP8Parameters
                                    vp8Parameters(codecName == "VP8" ?
                                                  SdpRtpmapAttributeList::kVP8 :
                                                  SdpRtpmapAttributeList::kVP9);

      vp8Parameters.max_fs = rustFmtpParameters.max_fs;
      vp8Parameters.max_fr = rustFmtpParameters.max_fr;

      fmtpParameters.reset(new SdpFmtpAttributeList::VP8Parameters(
                                                     std::move(vp8Parameters)));
    } else if(codecName == "TELEPHONE-EVENT"){
      SdpFmtpAttributeList::TelephoneEventParameters telephoneEventParameters;

      telephoneEventParameters.dtmfTones =
                              convertStringView(rustFmtpParameters.dtmf_tones);

      fmtpParameters.reset(new SdpFmtpAttributeList::TelephoneEventParameters(
                                          std::move(telephoneEventParameters)));
    } else if(codecName == "RED") {
      SdpFmtpAttributeList::RedParameters redParameters;

      redParameters.encodings = convertU8Vec(rustFmtpParameters.encodings);

      fmtpParameters.reset(new SdpFmtpAttributeList::RedParameters(
                                                     std::move(redParameters)));
    } else{
      // The parameter set is unknown so skip it
      continue;
    }

    fmtpList->PushEntry(std::to_string(payloadType), std::move(fmtpParameters));
  }
  SetAttribute(fmtpList.release());
}

void
RsdparsaSdpAttributeList::LoadPtime(RustAttributeList* attributeList)
{
  int64_t ptime = sdp_get_ptime(attributeList);
  if (ptime >= 0) {
    SetAttribute(new SdpNumberAttribute(SdpAttribute::kPtimeAttribute,
                                        static_cast<uint32_t>(ptime)));
  }
}

void
RsdparsaSdpAttributeList::LoadFlags(RustAttributeList* attributeList)
{
  RustSdpAttributeFlags flags = sdp_get_attribute_flags(attributeList);
  if (flags.iceLite) {
    SetAttribute(new SdpFlagAttribute(SdpAttribute::kIceLiteAttribute));
  }
  if (flags.rtcpMux) {
    SetAttribute(new SdpFlagAttribute(SdpAttribute::kRtcpMuxAttribute));
  }
  if (flags.bundleOnly) {
    SetAttribute(new SdpFlagAttribute(SdpAttribute::kBundleOnlyAttribute));
  }
  if (flags.endOfCandidates) {
    SetAttribute(new SdpFlagAttribute(SdpAttribute::kEndOfCandidatesAttribute));
  }
}

void
RsdparsaSdpAttributeList::LoadMaxMessageSize(RustAttributeList* attributeList)
{
  int64_t max_msg_size = sdp_get_max_msg_size(attributeList);
  if (max_msg_size >= 0) {
    SetAttribute(new SdpNumberAttribute(SdpAttribute::kMaxMessageSizeAttribute,
                                        static_cast<uint32_t>(max_msg_size)));
  }
}

void
RsdparsaSdpAttributeList::LoadMid(RustAttributeList* attributeList)
{
  StringView rustMid;
  if (NS_SUCCEEDED(sdp_get_mid(attributeList, &rustMid))) {
    std::string mid = convertStringView(rustMid);
    SetAttribute(new SdpStringAttribute(SdpAttribute::kMidAttribute, mid));
  }
}

void
RsdparsaSdpAttributeList::LoadMsid(RustAttributeList* attributeList)
{
  size_t numMsid = sdp_get_msid_count(attributeList);
  if (numMsid == 0) {
    return;
  }
  auto rustMsid = MakeUnique<RustSdpAttributeMsid[]>(numMsid);
  sdp_get_msids(attributeList, numMsid, rustMsid.get());
  auto msids = MakeUnique<SdpMsidAttributeList>();
  for(size_t i = 0; i < numMsid; i++) {
    RustSdpAttributeMsid& msid = rustMsid[i];
    std::string id = convertStringView(msid.id);
    std::string appdata = convertStringView(msid.appdata);
    msids->PushEntry(id, appdata);
  }
  SetAttribute(msids.release());
}

void
RsdparsaSdpAttributeList::LoadMsidSemantics(RustAttributeList* attributeList)
{
  size_t numMsidSemantic = sdp_get_msid_semantic_count(attributeList);
  if (numMsidSemantic == 0) {
    return;
  }
  auto rustMsidSemantics = MakeUnique<RustSdpAttributeMsidSemantic[]>(numMsidSemantic);
  sdp_get_msid_semantics(attributeList, numMsidSemantic,
                         rustMsidSemantics.get());
  auto msidSemantics = MakeUnique<SdpMsidSemanticAttributeList>();
  for(size_t i = 0; i < numMsidSemantic; i++) {
    RustSdpAttributeMsidSemantic& rustMsidSemantic = rustMsidSemantics[i];
    std::string semantic = convertStringView(rustMsidSemantic.semantic);
    std::vector<std::string> msids = convertStringVec(rustMsidSemantic.msids);
    msidSemantics->PushEntry(semantic, msids);
  }
  SetAttribute(msidSemantics.release());
}

void
RsdparsaSdpAttributeList::LoadGroup(RustAttributeList* attributeList)
{
  size_t numGroup = sdp_get_group_count(attributeList);
  if (numGroup == 0) {
    return;
  }
  auto rustGroups = MakeUnique<RustSdpAttributeGroup[]>(numGroup);
  sdp_get_groups(attributeList, numGroup, rustGroups.get());
  auto groups = MakeUnique<SdpGroupAttributeList>();
  for(size_t i = 0; i < numGroup; i++) {
    RustSdpAttributeGroup& group = rustGroups[i];
    SdpGroupAttributeList::Semantics semantic;
    switch(group.semantic) {
      case RustSdpAttributeGroupSemantic ::kRustLipSynchronization:
        semantic = SdpGroupAttributeList::kLs;
        break;
      case RustSdpAttributeGroupSemantic ::kRustFlowIdentification:
        semantic = SdpGroupAttributeList::kFid;
        break;
      case RustSdpAttributeGroupSemantic ::kRustSingleReservationFlow:
        semantic = SdpGroupAttributeList::kSrf;
        break;
      case RustSdpAttributeGroupSemantic ::kRustAlternateNetworkAddressType:
        semantic = SdpGroupAttributeList::kAnat;
        break;
      case RustSdpAttributeGroupSemantic ::kRustForwardErrorCorrection:
        semantic = SdpGroupAttributeList::kFec;
        break;
      case RustSdpAttributeGroupSemantic ::kRustDecodingDependency:
        semantic = SdpGroupAttributeList::kDdp;
        break;
      case RustSdpAttributeGroupSemantic ::kRustBundle:
        semantic = SdpGroupAttributeList::kBundle;
        break;
    }
    std::vector<std::string> tags = convertStringVec(group.tags);
    groups->PushEntry(semantic, tags);
  }
  SetAttribute(groups.release());
}

void
RsdparsaSdpAttributeList::LoadRtcp(RustAttributeList* attributeList)
{
  RustSdpAttributeRtcp rtcp;
  if (NS_SUCCEEDED(sdp_get_rtcp(attributeList, &rtcp))) {
    sdp::AddrType addrType = convertAddressType(rtcp.unicastAddr.addrType);
    if (sdp::kAddrTypeNone == addrType) {
      SetAttribute(new SdpRtcpAttribute(rtcp.port));
    } else {
      std::string addr(rtcp.unicastAddr.unicastAddr);
      SetAttribute(new SdpRtcpAttribute(rtcp.port, sdp::kInternet,
                                        addrType, addr));
    }
  }
}

void
RsdparsaSdpAttributeList::LoadRtcpFb(RustAttributeList* attributeList)
{
  auto rtcpfbsCount = sdp_get_rtcpfb_count(attributeList);
  if (!rtcpfbsCount) {
    return;
  }

  auto rustRtcpfbs = MakeUnique<RustSdpAttributeRtcpFb[]>(rtcpfbsCount);
  sdp_get_rtcpfbs(attributeList, rtcpfbsCount, rustRtcpfbs.get());

  auto rtcpfbList = MakeUnique<SdpRtcpFbAttributeList>();
  for(size_t i = 0; i < rtcpfbsCount; i++) {
    RustSdpAttributeRtcpFb& rtcpfb = rustRtcpfbs[i];
    uint32_t payloadTypeU32 = rtcpfb.payloadType;

    std::stringstream ss;
    if(payloadTypeU32 == std::numeric_limits<uint32_t>::max()){
      ss << "*";
    } else {
      ss << payloadTypeU32;
    }

    uint32_t feedbackType = rtcpfb.feedbackType;
    std::string parameter = convertStringView(rtcpfb.parameter);
    std::string extra = convertStringView(rtcpfb.extra);

    rtcpfbList->PushEntry(ss.str(), static_cast<SdpRtcpFbAttributeList::Type>(feedbackType), parameter, extra);
  }

  SetAttribute(rtcpfbList.release());
}

SdpSimulcastAttribute::Versions
LoadSimulcastVersions(const RustSdpAttributeSimulcastVersionVec*
                      rustVersionList)
{
  size_t rustVersionCount = sdp_simulcast_get_version_count(rustVersionList);
  auto rustVersionArray = MakeUnique<RustSdpAttributeSimulcastVersion[]>
                                   (rustVersionCount);
  sdp_simulcast_get_versions(rustVersionList, rustVersionCount,
                             rustVersionArray.get());

  SdpSimulcastAttribute::Versions versions;
  versions.type = SdpSimulcastAttribute::Versions::kRid;

  for(size_t i = 0; i < rustVersionCount; i++) {
    const RustSdpAttributeSimulcastVersion& rustVersion = rustVersionArray[i];
    size_t rustIdCount = sdp_simulcast_get_ids_count(rustVersion.ids);
    if (!rustIdCount) {
      continue;
    }

    SdpSimulcastAttribute::Version version;
    auto rustIdArray = MakeUnique<RustSdpAttributeSimulcastId[]>(rustIdCount);
    sdp_simulcast_get_ids(rustVersion.ids, rustIdCount, rustIdArray.get());

    for(size_t j = 0; j < rustIdCount; j++){
      const RustSdpAttributeSimulcastId& rustId = rustIdArray[j];
      std::string id = convertStringView(rustId.id);
      // TODO: Bug 1225877. Added support for 'paused'-state
      version.choices.push_back(id);
    }

    versions.push_back(version);
  }

  return versions;
}

void
RsdparsaSdpAttributeList::LoadSimulcast(RustAttributeList* attributeList)
{
  RustSdpAttributeSimulcast rustSimulcast;
  if (NS_SUCCEEDED(sdp_get_simulcast(attributeList, &rustSimulcast))) {
    auto simulcast = MakeUnique<SdpSimulcastAttribute>();

    simulcast->sendVersions = LoadSimulcastVersions(rustSimulcast.send);
    simulcast->recvVersions = LoadSimulcastVersions(rustSimulcast.recv);

    SetAttribute(simulcast.release());
  }
}

SdpImageattrAttributeList::XYRange
LoadImageattrXYRange(const RustSdpAttributeImageAttrXYRange& rustXYRange)
{
  SdpImageattrAttributeList::XYRange xyRange;

  if (!rustXYRange.discrete_values) {
    xyRange.min = rustXYRange.min;
    xyRange.max = rustXYRange.max;
    xyRange.step = rustXYRange.step;

  } else {
    xyRange.discreteValues = convertU32Vec(rustXYRange.discrete_values);
  }

  return xyRange;
}

std::vector<SdpImageattrAttributeList::Set>
LoadImageattrSets(const RustSdpAttributeImageAttrSetVec* rustSets)
{
  std::vector<SdpImageattrAttributeList::Set> sets;

  size_t rustSetCount = sdp_imageattr_get_set_count(rustSets);
  if (!rustSetCount) {
    return sets;
  }

  auto rustSetArray = MakeUnique<RustSdpAttributeImageAttrSet[]>(rustSetCount);
  sdp_imageattr_get_sets(rustSets, rustSetCount, rustSetArray.get());

  for(size_t i = 0; i < rustSetCount; i++) {
    const RustSdpAttributeImageAttrSet& rustSet = rustSetArray[i];
    SdpImageattrAttributeList::Set set;

    set.xRange = LoadImageattrXYRange(rustSet.x);
    set.yRange = LoadImageattrXYRange(rustSet.y);

    if (rustSet.has_sar) {
      if (!rustSet.sar.discrete_values) {
        set.sRange.min = rustSet.sar.min;
        set.sRange.max = rustSet.sar.max;
      } else {
        set.sRange.discreteValues = convertF32Vec(rustSet.sar.discrete_values);
      }
    }

    if (rustSet.has_par) {
      set.pRange.min = rustSet.par.min;
      set.pRange.max = rustSet.par.max;
    }

    set.qValue = rustSet.q;

    sets.push_back(set);
  }

  return sets;
}

void
RsdparsaSdpAttributeList::LoadImageattr(RustAttributeList* attributeList)
{
  size_t numImageattrs = sdp_get_imageattr_count(attributeList);
  if (numImageattrs == 0) {
    return;
  }
  auto rustImageattrs = MakeUnique<RustSdpAttributeImageAttr[]>(numImageattrs);
  sdp_get_imageattrs(attributeList, numImageattrs, rustImageattrs.get());
  auto imageattrList = MakeUnique<SdpImageattrAttributeList>();
  for(size_t i = 0; i < numImageattrs; i++) {
    const RustSdpAttributeImageAttr& rustImageAttr = rustImageattrs[i];

    SdpImageattrAttributeList::Imageattr imageAttr;

    if (rustImageAttr.payloadType != std::numeric_limits<uint32_t>::max()) {
      imageAttr.pt = Some(rustImageAttr.payloadType);
    }

    if (rustImageAttr.send.sets) {
      imageAttr.sendSets = LoadImageattrSets(rustImageAttr.send.sets);
    } else {
      imageAttr.sendAll = true;
    }

    if (rustImageAttr.recv.sets) {
      imageAttr.recvSets = LoadImageattrSets(rustImageAttr.recv.sets);
    } else {
      imageAttr.recvAll = true;
    }

    imageattrList->mImageattrs.push_back(imageAttr);
  }
  SetAttribute(imageattrList.release());
}

void
RsdparsaSdpAttributeList::LoadSctpmaps(RustAttributeList* attributeList)
{
  size_t numSctpmaps = sdp_get_sctpmap_count(attributeList);
  if (numSctpmaps == 0) {
    return;
  }
  auto rustSctpmaps = MakeUnique<RustSdpAttributeSctpmap[]>(numSctpmaps);
  sdp_get_sctpmaps(attributeList, numSctpmaps, rustSctpmaps.get());
  auto sctpmapList = MakeUnique<SdpSctpmapAttributeList>();
  for(size_t i = 0; i < numSctpmaps; i++) {
    RustSdpAttributeSctpmap& sctpmap = rustSctpmaps[i];
    sctpmapList->PushEntry(std::to_string(sctpmap.port),
                           "webrtc-datachannel",
                           sctpmap.channels);
  }
  SetAttribute(sctpmapList.release());
}

void
RsdparsaSdpAttributeList::LoadDirection(RustAttributeList* attributeList)
{
  SdpDirectionAttribute::Direction dir;
  RustDirection rustDir = sdp_get_direction(attributeList);
  switch(rustDir) {
    case RustDirection::kRustRecvonly:
      dir = SdpDirectionAttribute::kRecvonly;
      break;
    case RustDirection::kRustSendonly:
      dir = SdpDirectionAttribute::kSendonly;
      break;
    case RustDirection::kRustSendrecv:
      dir = SdpDirectionAttribute::kSendrecv;
      break;
    case RustDirection::kRustInactive:
      dir = SdpDirectionAttribute::kInactive;
      break;
  }
  SetAttribute(new SdpDirectionAttribute(dir));
}

void
RsdparsaSdpAttributeList::LoadRemoteCandidates(RustAttributeList* attributeList)
{
  size_t nC = sdp_get_remote_candidate_count(attributeList);
  if (nC == 0) {
    return;
  }
  auto rustCandidates = MakeUnique<RustSdpAttributeRemoteCandidate[]>(nC);
  sdp_get_remote_candidates(attributeList, nC,
                            rustCandidates.get());
  std::vector<SdpRemoteCandidatesAttribute::Candidate> candidates;
  for(size_t i = 0; i < nC; i++) {
    RustSdpAttributeRemoteCandidate& rustCandidate = rustCandidates[i];
    SdpRemoteCandidatesAttribute::Candidate candidate;
    candidate.port = rustCandidate.port;
    candidate.id = std::to_string(rustCandidate.component);
    candidate.address = std::string(rustCandidate.address.unicastAddr);
    candidates.push_back(candidate);
  }
  SdpRemoteCandidatesAttribute* candidatesList;
  candidatesList = new SdpRemoteCandidatesAttribute(candidates);
  SetAttribute(candidatesList);
}

void
RsdparsaSdpAttributeList::LoadRids(RustAttributeList* attributeList)
{
  size_t numRids = sdp_get_rid_count(attributeList);
  if (numRids == 0) {
    return;
  }

  auto rustRids = MakeUnique<RustSdpAttributeRid[]>(numRids);
  sdp_get_rids(attributeList, numRids, rustRids.get());

  auto ridList = MakeUnique<SdpRidAttributeList>();
  for(size_t i = 0; i < numRids; i++) {
    const RustSdpAttributeRid& rid = rustRids[i];

    std::string id = convertStringView(rid.id);
    auto direction = static_cast<sdp::Direction>(rid.direction);
    std::vector<uint16_t> formats = convertU16Vec(rid.formats);

    EncodingConstraints parameters;
    parameters.maxWidth = rid.params.max_width;
    parameters.maxHeight = rid.params.max_height;
    parameters.maxFps = rid.params.max_fps;
    parameters.maxFs = rid.params.max_fs;
    parameters.maxBr = rid.params.max_br;
    parameters.maxPps = rid.params.max_pps;

    std::vector<std::string> depends = convertStringVec(rid.depends);

    ridList->PushEntry(id, direction, formats, parameters, depends);
  }

  SetAttribute(ridList.release());
}

void
RsdparsaSdpAttributeList::LoadSctpPort(RustAttributeList* attributeList)
{
  int64_t port = sdp_get_sctp_port(attributeList);
  if (port >= 0) {
    SetAttribute(new SdpNumberAttribute(SdpAttribute::kSctpPortAttribute,
                                        static_cast<uint32_t>(port)));
  }
}

void
RsdparsaSdpAttributeList::LoadExtmap(RustAttributeList* attributeList)
{
  size_t numExtmap = sdp_get_extmap_count(attributeList);
  if (numExtmap == 0) {
    return;
  }
  auto rustExtmaps = MakeUnique<RustSdpAttributeExtmap[]>(numExtmap);
  sdp_get_extmaps(attributeList, numExtmap,
                  rustExtmaps.get());
  auto extmaps = MakeUnique<SdpExtmapAttributeList>();
  for(size_t i = 0; i < numExtmap; i++) {
    RustSdpAttributeExtmap& rustExtmap = rustExtmaps[i];
    std::string name = convertStringView(rustExtmap.url);
    SdpDirectionAttribute::Direction direction;
    bool directionSpecified = rustExtmap.direction_specified;
    switch(rustExtmap.direction) {
      case RustDirection::kRustRecvonly:
        direction = SdpDirectionAttribute::kRecvonly;
        break;
      case RustDirection::kRustSendonly:
        direction = SdpDirectionAttribute::kSendonly;
        break;
      case RustDirection::kRustSendrecv:
        direction = SdpDirectionAttribute::kSendrecv;
        break;
      case RustDirection::kRustInactive:
        direction = SdpDirectionAttribute::kInactive;
        break;
    }
    std::string extensionAttributes;
    extensionAttributes = convertStringView(rustExtmap.extensionAttributes);
    extmaps->PushEntry((uint16_t) rustExtmap.id, direction,
                       directionSpecified, name, extensionAttributes);
  }
  SetAttribute(extmaps.release());
}

void
RsdparsaSdpAttributeList::LoadMaxPtime(RustAttributeList* attributeList)
{
  uint64_t maxPtime = 0;
  nsresult nr = sdp_get_maxptime(attributeList, &maxPtime);
  if (NS_SUCCEEDED(nr)) {
    SetAttribute(new SdpNumberAttribute(SdpAttribute::kMaxptimeAttribute,
                                        maxPtime));
  }
}

bool
RsdparsaSdpAttributeList::IsAllowedHere(SdpAttribute::AttributeType type)
{
  if (AtSessionLevel() && !SdpAttribute::IsAllowedAtSessionLevel(type)) {
    return false;
  }

  if (!AtSessionLevel() && !SdpAttribute::IsAllowedAtMediaLevel(type)) {
    return false;
  }

  return true;
}

void
RsdparsaSdpAttributeList::Serialize(std::ostream& os) const
{
  for (size_t i = 0; i < kNumAttributeTypes; ++i) {
    if (mAttributes[i]) {
      os << *mAttributes[i];
    }
  }
}

} // namespace mozilla
