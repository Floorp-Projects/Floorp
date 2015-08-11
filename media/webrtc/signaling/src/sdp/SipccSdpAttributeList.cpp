/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/sdp/SipccSdpAttributeList.h"

#include <ostream>
#include "mozilla/Assertions.h"
#include "signaling/src/sdp/SdpErrorHolder.h"

extern "C" {
#include "signaling/src/sdp/sipcc/sdp_private.h"
}

namespace mozilla
{

/* static */ const std::string SipccSdpAttributeList::kEmptyString = "";

SipccSdpAttributeList::SipccSdpAttributeList(
    const SipccSdpAttributeList* sessionLevel)
    : mSessionLevel(sessionLevel)
{
  memset(&mAttributes, 0, sizeof(mAttributes));
}

SipccSdpAttributeList::~SipccSdpAttributeList()
{
  for (size_t i = 0; i < kNumAttributeTypes; ++i) {
    delete mAttributes[i];
  }
}

bool
SipccSdpAttributeList::HasAttribute(AttributeType type,
                                    bool sessionFallback) const
{
  return !!GetAttribute(type, sessionFallback);
}

const SdpAttribute*
SipccSdpAttributeList::GetAttribute(AttributeType type,
                                    bool sessionFallback) const
{
  const SdpAttribute* value = mAttributes[static_cast<size_t>(type)];
  // Only do fallback when the attribute can appear at both the media and
  // session level
  if (!value && !AtSessionLevel() && sessionFallback &&
      SdpAttribute::IsAllowedAtSessionLevel(type) &&
      SdpAttribute::IsAllowedAtMediaLevel(type)) {
    return mSessionLevel->GetAttribute(type, false);
  }
  return value;
}

void
SipccSdpAttributeList::RemoveAttribute(AttributeType type)
{
  delete mAttributes[static_cast<size_t>(type)];
  mAttributes[static_cast<size_t>(type)] = nullptr;
}

void
SipccSdpAttributeList::Clear()
{
  for (size_t i = 0; i < kNumAttributeTypes; ++i) {
    RemoveAttribute(static_cast<AttributeType>(i));
  }
}

void
SipccSdpAttributeList::SetAttribute(SdpAttribute* attr)
{
  if (!IsAllowedHere(attr->GetType())) {
    MOZ_ASSERT(false, "This type of attribute is not allowed here");
    return;
  }
  RemoveAttribute(attr->GetType());
  mAttributes[attr->GetType()] = attr;
}

void
SipccSdpAttributeList::LoadSimpleString(sdp_t* sdp, uint16_t level,
                                        sdp_attr_e attr,
                                        AttributeType targetType,
                                        SdpErrorHolder& errorHolder)
{
  const char* value = sdp_attr_get_simple_string(sdp, attr, level, 0, 1);
  if (value) {
    if (!IsAllowedHere(targetType)) {
      uint32_t lineNumber = sdp_attr_line_number(sdp, attr, level, 0, 1);
      WarnAboutMisplacedAttribute(targetType, lineNumber, errorHolder);
    } else {
      SetAttribute(new SdpStringAttribute(targetType, std::string(value)));
    }
  }
}

void
SipccSdpAttributeList::LoadSimpleStrings(sdp_t* sdp, uint16_t level,
                                         SdpErrorHolder& errorHolder)
{
  LoadSimpleString(sdp, level, SDP_ATTR_MID, SdpAttribute::kMidAttribute,
                   errorHolder);
  LoadSimpleString(sdp, level, SDP_ATTR_LABEL, SdpAttribute::kLabelAttribute,
                   errorHolder);
  LoadSimpleString(sdp, level, SDP_ATTR_IDENTITY,
                   SdpAttribute::kIdentityAttribute, errorHolder);
}

void
SipccSdpAttributeList::LoadSimpleNumber(sdp_t* sdp, uint16_t level,
                                        sdp_attr_e attr,
                                        AttributeType targetType,
                                        SdpErrorHolder& errorHolder)
{
  if (sdp_attr_valid(sdp, attr, level, 0, 1)) {
    if (!IsAllowedHere(targetType)) {
      uint32_t lineNumber = sdp_attr_line_number(sdp, attr, level, 0, 1);
      WarnAboutMisplacedAttribute(targetType, lineNumber, errorHolder);
    } else {
      uint32_t value = sdp_attr_get_simple_u32(sdp, attr, level, 0, 1);
      SetAttribute(new SdpNumberAttribute(targetType, value));
    }
  }
}

void
SipccSdpAttributeList::LoadSimpleNumbers(sdp_t* sdp, uint16_t level,
                                         SdpErrorHolder& errorHolder)
{
  LoadSimpleNumber(sdp, level, SDP_ATTR_PTIME, SdpAttribute::kPtimeAttribute,
                   errorHolder);
  LoadSimpleNumber(sdp, level, SDP_ATTR_MAXPTIME,
                   SdpAttribute::kMaxptimeAttribute, errorHolder);
}

void
SipccSdpAttributeList::LoadFlags(sdp_t* sdp, uint16_t level)
{
  if (AtSessionLevel()) {
    if (sdp_attr_valid(sdp, SDP_ATTR_ICE_LITE, level, 0, 1)) {
      SetAttribute(new SdpFlagAttribute(SdpAttribute::kIceLiteAttribute));
    }
  } else { // media-level
    if (sdp_attr_valid(sdp, SDP_ATTR_RTCP_MUX, level, 0, 1)) {
      SetAttribute(new SdpFlagAttribute(SdpAttribute::kRtcpMuxAttribute));
    }
    if (sdp_attr_valid(sdp, SDP_ATTR_END_OF_CANDIDATES, level, 0, 1)) {
      SetAttribute(
          new SdpFlagAttribute(SdpAttribute::kEndOfCandidatesAttribute));
    }
    if (sdp_attr_valid(sdp, SDP_ATTR_BUNDLE_ONLY, level, 0, 1)) {
      SetAttribute(new SdpFlagAttribute(SdpAttribute::kBundleOnlyAttribute));
    }
  }
}

static void
ConvertDirection(sdp_direction_e sipcc_direction,
                 SdpDirectionAttribute::Direction* dir_outparam)
{
  switch (sipcc_direction) {
    case SDP_DIRECTION_SENDRECV:
      *dir_outparam = SdpDirectionAttribute::kSendrecv;
      return;
    case SDP_DIRECTION_SENDONLY:
      *dir_outparam = SdpDirectionAttribute::kSendonly;
      return;
    case SDP_DIRECTION_RECVONLY:
      *dir_outparam = SdpDirectionAttribute::kRecvonly;
      return;
    case SDP_DIRECTION_INACTIVE:
      *dir_outparam = SdpDirectionAttribute::kInactive;
      return;
    case SDP_MAX_QOS_DIRECTIONS:
      // Nothing actually sets this value.
      // Fall through to MOZ_CRASH below.
      {
      }
  }

  MOZ_CRASH("Invalid direction from sipcc; this is probably corruption");
}

void
SipccSdpAttributeList::LoadDirection(sdp_t* sdp, uint16_t level,
                                     SdpErrorHolder& errorHolder)
{
  SdpDirectionAttribute::Direction dir;
  ConvertDirection(sdp_get_media_direction(sdp, level, 0), &dir);
  SetAttribute(new SdpDirectionAttribute(dir));
}

void
SipccSdpAttributeList::LoadIceAttributes(sdp_t* sdp, uint16_t level)
{
  char* value;
  sdp_result_e sdpres =
      sdp_attr_get_ice_attribute(sdp, level, 0, SDP_ATTR_ICE_UFRAG, 1, &value);
  if (sdpres == SDP_SUCCESS) {
    SetAttribute(new SdpStringAttribute(SdpAttribute::kIceUfragAttribute,
                                        std::string(value)));
  }
  sdpres =
      sdp_attr_get_ice_attribute(sdp, level, 0, SDP_ATTR_ICE_PWD, 1, &value);
  if (sdpres == SDP_SUCCESS) {
    SetAttribute(new SdpStringAttribute(SdpAttribute::kIcePwdAttribute,
                                        std::string(value)));
  }

  const char* iceOptVal =
      sdp_attr_get_simple_string(sdp, SDP_ATTR_ICE_OPTIONS, level, 0, 1);
  if (iceOptVal) {
    auto* iceOptions =
        new SdpOptionsAttribute(SdpAttribute::kIceOptionsAttribute);
    iceOptions->Load(iceOptVal);
    SetAttribute(iceOptions);
  }
}

bool
SipccSdpAttributeList::LoadFingerprint(sdp_t* sdp, uint16_t level,
                                       SdpErrorHolder& errorHolder)
{
  char* value;
  UniquePtr<SdpFingerprintAttributeList> fingerprintAttrs;

  for (uint16_t i = 1; i < UINT16_MAX; ++i) {
    sdp_result_e result = sdp_attr_get_dtls_fingerprint_attribute(
        sdp, level, 0, SDP_ATTR_DTLS_FINGERPRINT, i, &value);

    if (result != SDP_SUCCESS) {
      break;
    }

    std::string fingerprintAttr(value);
    uint32_t lineNumber =
        sdp_attr_line_number(sdp, SDP_ATTR_DTLS_FINGERPRINT, level, 0, i);

    // sipcc does not expose parse code for this
    size_t start = fingerprintAttr.find_first_not_of(" \t");
    if (start == std::string::npos) {
      errorHolder.AddParseError(lineNumber, "Empty fingerprint attribute");
      return false;
    }

    size_t end = fingerprintAttr.find_first_of(" \t", start);
    if (end == std::string::npos) {
      // One token, no trailing ws
      errorHolder.AddParseError(lineNumber,
                                "Only one token in fingerprint attribute");
      return false;
    }

    std::string algorithmToken(fingerprintAttr.substr(start, end - start));

    start = fingerprintAttr.find_first_not_of(" \t", end);
    if (start == std::string::npos) {
      // One token, trailing ws
      errorHolder.AddParseError(lineNumber,
                                "Only one token in fingerprint attribute");
      return false;
    }

    std::string fingerprintToken(fingerprintAttr.substr(start));

    std::vector<uint8_t> fingerprint =
        SdpFingerprintAttributeList::ParseFingerprint(fingerprintToken);
    if (fingerprint.size() == 0) {
      errorHolder.AddParseError(lineNumber, "Malformed fingerprint token");
      return false;
    }

    if (!fingerprintAttrs) {
      fingerprintAttrs.reset(new SdpFingerprintAttributeList);
    }

    // Don't assert on unknown algorithm, just skip
    fingerprintAttrs->PushEntry(algorithmToken, fingerprint, false);
  }

  if (fingerprintAttrs) {
    SetAttribute(fingerprintAttrs.release());
  }

  return true;
}

void
SipccSdpAttributeList::LoadCandidate(sdp_t* sdp, uint16_t level)
{
  char* value;
  auto candidates =
      MakeUnique<SdpMultiStringAttribute>(SdpAttribute::kCandidateAttribute);

  for (uint16_t i = 1; i < UINT16_MAX; ++i) {
    sdp_result_e result = sdp_attr_get_ice_attribute(
        sdp, level, 0, SDP_ATTR_ICE_CANDIDATE, i, &value);

    if (result != SDP_SUCCESS) {
      break;
    }

    candidates->mValues.push_back(value);
  }

  if (!candidates->mValues.empty()) {
    SetAttribute(candidates.release());
  }
}

bool
SipccSdpAttributeList::LoadSctpmap(sdp_t* sdp, uint16_t level,
                                   SdpErrorHolder& errorHolder)
{
  auto sctpmap = MakeUnique<SdpSctpmapAttributeList>();
  for (uint16_t i = 0; i < UINT16_MAX; ++i) {
    sdp_attr_t* attr = sdp_find_attr(sdp, level, 0, SDP_ATTR_SCTPMAP, i + 1);

    if (!attr) {
      break;
    }

    // Yeah, this is a little weird, but for now we'll just store this as a
    // payload type.
    uint16_t payloadType = attr->attr.sctpmap.port;
    uint16_t streams = attr->attr.sctpmap.streams;
    const char* name = attr->attr.sctpmap.protocol;

    std::ostringstream osPayloadType;
    osPayloadType << payloadType;
    sctpmap->PushEntry(osPayloadType.str(), name, streams);
  }

  if (!sctpmap->mSctpmaps.empty()) {
    SetAttribute(sctpmap.release());
  }

  return true;
}

SdpRtpmapAttributeList::CodecType
SipccSdpAttributeList::GetCodecType(rtp_ptype type)
{
  switch (type) {
    case RTP_PCMU:
      return SdpRtpmapAttributeList::kPCMU;
    case RTP_PCMA:
      return SdpRtpmapAttributeList::kPCMA;
    case RTP_G722:
      return SdpRtpmapAttributeList::kG722;
    case RTP_H264_P0:
    case RTP_H264_P1:
      return SdpRtpmapAttributeList::kH264;
    case RTP_OPUS:
      return SdpRtpmapAttributeList::kOpus;
    case RTP_VP8:
      return SdpRtpmapAttributeList::kVP8;
    case RTP_VP9:
      return SdpRtpmapAttributeList::kVP9;
    case RTP_NONE:
    // Happens when sipcc doesn't know how to translate to the enum
    case RTP_CELP:
    case RTP_G726:
    case RTP_GSM:
    case RTP_G723:
    case RTP_DVI4:
    case RTP_DVI4_II:
    case RTP_LPC:
    case RTP_G728:
    case RTP_G729:
    case RTP_JPEG:
    case RTP_NV:
    case RTP_H261:
    case RTP_AVT:
    case RTP_L16:
    case RTP_H263:
    case RTP_ILBC:
    case RTP_I420:
      return SdpRtpmapAttributeList::kOtherCodec;
  }
  MOZ_CRASH("Invalid codec type from sipcc. Probably corruption.");
}

bool
SipccSdpAttributeList::LoadRtpmap(sdp_t* sdp, uint16_t level,
                                  SdpErrorHolder& errorHolder)
{
  auto rtpmap = MakeUnique<SdpRtpmapAttributeList>();
  uint16_t count;
  sdp_result_e result =
      sdp_attr_num_instances(sdp, level, 0, SDP_ATTR_RTPMAP, &count);
  if (result != SDP_SUCCESS) {
    MOZ_ASSERT(false, "Unable to get rtpmap size");
    errorHolder.AddParseError(sdp_get_media_line_number(sdp, level),
                              "Unable to get rtpmap size");
    return false;
  }
  for (uint16_t i = 0; i < count; ++i) {
    uint16_t pt = sdp_attr_get_rtpmap_payload_type(sdp, level, 0, i + 1);
    const char* ccName = sdp_attr_get_rtpmap_encname(sdp, level, 0, i + 1);

    if (!ccName) {
      // Probably no rtpmap attribute for a pt in an m-line
      errorHolder.AddParseError(sdp_get_media_line_number(sdp, level),
                                "No rtpmap attribute for payload type");
      continue;
    }

    std::string name(ccName);

    SdpRtpmapAttributeList::CodecType codec =
        GetCodecType(sdp_get_known_payload_type(sdp, level, pt));

    uint32_t clock = sdp_attr_get_rtpmap_clockrate(sdp, level, 0, i + 1);
    uint16_t channels = 0;

    // sipcc gives us a channels value of "1" for video
    if (sdp_get_media_type(sdp, level) == SDP_MEDIA_AUDIO) {
      channels = sdp_attr_get_rtpmap_num_chan(sdp, level, 0, i + 1);
    }

    std::ostringstream osPayloadType;
    osPayloadType << pt;
    rtpmap->PushEntry(osPayloadType.str(), codec, name, clock, channels);
  }

  if (!rtpmap->mRtpmaps.empty()) {
    SetAttribute(rtpmap.release());
  }

  return true;
}

void
SipccSdpAttributeList::LoadSetup(sdp_t* sdp, uint16_t level)
{
  sdp_setup_type_e setupType;
  auto sdpres = sdp_attr_get_setup_attribute(sdp, level, 0, 1, &setupType);

  if (sdpres != SDP_SUCCESS) {
    return;
  }

  switch (setupType) {
    case SDP_SETUP_ACTIVE:
      SetAttribute(new SdpSetupAttribute(SdpSetupAttribute::kActive));
      return;
    case SDP_SETUP_PASSIVE:
      SetAttribute(new SdpSetupAttribute(SdpSetupAttribute::kPassive));
      return;
    case SDP_SETUP_ACTPASS:
      SetAttribute(new SdpSetupAttribute(SdpSetupAttribute::kActpass));
      return;
    case SDP_SETUP_HOLDCONN:
      SetAttribute(new SdpSetupAttribute(SdpSetupAttribute::kHoldconn));
      return;
    case SDP_SETUP_UNKNOWN:
      return;
    case SDP_SETUP_NOT_FOUND:
    case SDP_MAX_SETUP:
      // There is no code that will set these.
      // Fall through to MOZ_CRASH() below.
      {
      }
  }

  MOZ_CRASH("Invalid setup type from sipcc. This is probably corruption.");
}

void
SipccSdpAttributeList::LoadSsrc(sdp_t* sdp, uint16_t level)
{
  auto ssrcs = MakeUnique<SdpSsrcAttributeList>();

  for (uint16_t i = 1; i < UINT16_MAX; ++i) {
    sdp_attr_t* attr = sdp_find_attr(sdp, level, 0, SDP_ATTR_SSRC, i);

    if (!attr) {
      break;
    }

    sdp_ssrc_t* ssrc = &(attr->attr.ssrc);
    ssrcs->PushEntry(ssrc->ssrc, ssrc->attribute);
  }

  if (!ssrcs->mSsrcs.empty()) {
    SetAttribute(ssrcs.release());
  }
}

bool
SipccSdpAttributeList::LoadImageattr(sdp_t* sdp,
                                     uint16_t level,
                                     SdpErrorHolder& errorHolder)
{
  UniquePtr<SdpImageattrAttributeList> imageattrs(
      new SdpImageattrAttributeList);

  for (uint16_t i = 1; i < UINT16_MAX; ++i) {
    const char* imageattrRaw = sdp_attr_get_simple_string(sdp,
                                                          SDP_ATTR_IMAGEATTR,
                                                          level,
                                                          0,
                                                          i);
    if (!imageattrRaw) {
      break;
    }

    std::string error;
    size_t errorPos;
    if (!imageattrs->PushEntry(imageattrRaw, &error, &errorPos)) {
      std::ostringstream fullError(error + " at column ");
      fullError << errorPos;
      errorHolder.AddParseError(
        sdp_attr_line_number(sdp, SDP_ATTR_IMAGEATTR, level, 0, i),
        fullError.str());
      return false;
    }
  }

  if (!imageattrs->mImageattrs.empty()) {
    SetAttribute(imageattrs.release());
  }
  return true;
}

bool
SipccSdpAttributeList::LoadSimulcast(sdp_t* sdp,
                                     uint16_t level,
                                     SdpErrorHolder& errorHolder)
{
  const char* simulcastRaw = sdp_attr_get_simple_string(sdp,
                                                        SDP_ATTR_SIMULCAST,
                                                        level,
                                                        0,
                                                        1);
  if (!simulcastRaw) {
    return true;
  }

  UniquePtr<SdpSimulcastAttribute> simulcast(
      new SdpSimulcastAttribute);

  std::istringstream is(simulcastRaw);
  std::string error;
  if (!simulcast->Parse(is, &error)) {
    is.clear();
    std::ostringstream fullError(error + " at column ");
    fullError << is.tellg();
    errorHolder.AddParseError(
      sdp_attr_line_number(sdp, SDP_ATTR_SIMULCAST, level, 0, 1),
      fullError.str());
    return false;
  }

  SetAttribute(simulcast.release());
  return true;
}

bool
SipccSdpAttributeList::LoadGroups(sdp_t* sdp, uint16_t level,
                                  SdpErrorHolder& errorHolder)
{
  uint16_t attrCount = 0;
  if (sdp_attr_num_instances(sdp, level, 0, SDP_ATTR_GROUP, &attrCount) !=
      SDP_SUCCESS) {
    MOZ_ASSERT(false, "Could not get count of group attributes");
    errorHolder.AddParseError(0, "Could not get count of group attributes");
    return false;
  }

  UniquePtr<SdpGroupAttributeList> groups = MakeUnique<SdpGroupAttributeList>();
  for (uint16_t attr = 1; attr <= attrCount; ++attr) {
    SdpGroupAttributeList::Semantics semantics;
    std::vector<std::string> tags;

    switch (sdp_get_group_attr(sdp, level, 0, attr)) {
      case SDP_GROUP_ATTR_FID:
        semantics = SdpGroupAttributeList::kFid;
        break;
      case SDP_GROUP_ATTR_LS:
        semantics = SdpGroupAttributeList::kLs;
        break;
      case SDP_GROUP_ATTR_ANAT:
        semantics = SdpGroupAttributeList::kAnat;
        break;
      case SDP_GROUP_ATTR_BUNDLE:
        semantics = SdpGroupAttributeList::kBundle;
        break;
      default:
        continue;
    }

    uint16_t idCount = sdp_get_group_num_id(sdp, level, 0, attr);
    for (uint16_t id = 1; id <= idCount; ++id) {
      const char* idStr = sdp_get_group_id(sdp, level, 0, attr, id);
      if (!idStr) {
        std::ostringstream os;
        os << "bad a=group identifier at " << (attr - 1) << ", " << (id - 1);
        errorHolder.AddParseError(0, os.str());
        return false;
      }
      tags.push_back(std::string(idStr));
    }
    groups->PushEntry(semantics, tags);
  }

  if (!groups->mGroups.empty()) {
    SetAttribute(groups.release());
  }

  return true;
}

bool
SipccSdpAttributeList::LoadMsidSemantics(sdp_t* sdp, uint16_t level,
                                         SdpErrorHolder& errorHolder)
{
  auto msidSemantics = MakeUnique<SdpMsidSemanticAttributeList>();

  for (uint16_t i = 1; i < UINT16_MAX; ++i) {
    sdp_attr_t* attr = sdp_find_attr(sdp, level, 0, SDP_ATTR_MSID_SEMANTIC, i);

    if (!attr) {
      break;
    }

    sdp_msid_semantic_t* msid_semantic = &(attr->attr.msid_semantic);
    std::vector<std::string> msids;
    for (size_t i = 0; i < SDP_MAX_MEDIA_STREAMS; ++i) {
      if (!msid_semantic->msids[i]) {
        break;
      }

      msids.push_back(msid_semantic->msids[i]);
    }

    msidSemantics->PushEntry(msid_semantic->semantic, msids);
  }

  if (!msidSemantics->mMsidSemantics.empty()) {
    SetAttribute(msidSemantics.release());
  }
  return true;
}

void
SipccSdpAttributeList::LoadFmtp(sdp_t* sdp, uint16_t level)
{
  auto fmtps = MakeUnique<SdpFmtpAttributeList>();

  for (uint16_t i = 1; i < UINT16_MAX; ++i) {
    sdp_attr_t* attr = sdp_find_attr(sdp, level, 0, SDP_ATTR_FMTP, i);

    if (!attr) {
      break;
    }

    sdp_fmtp_t* fmtp = &(attr->attr.fmtp);

    // Get the payload type
    std::stringstream osPayloadType;
    // payload_num is the number in the fmtp attribute, verbatim
    osPayloadType << fmtp->payload_num;

    // Get the serialized form of the parameters
    flex_string fs;
    flex_string_init(&fs);

    // Very lame, but we need direct access so we can get the serialized form
    sdp_result_e sdpres = sdp_build_attr_fmtp_params(sdp, fmtp, &fs);

    if (sdpres != SDP_SUCCESS) {
      flex_string_free(&fs);
      continue;
    }

    std::string paramsString(fs.buffer);
    flex_string_free(&fs);

    // Get parsed form of parameters, if supported
    UniquePtr<SdpFmtpAttributeList::Parameters> parameters;

    rtp_ptype codec = sdp_get_known_payload_type(sdp, level, fmtp->payload_num);

    switch (codec) {
      case RTP_H264_P0:
      case RTP_H264_P1: {
        SdpFmtpAttributeList::H264Parameters* h264Parameters(
            new SdpFmtpAttributeList::H264Parameters);

        sstrncpy(h264Parameters->sprop_parameter_sets, fmtp->parameter_sets,
                 sizeof(h264Parameters->sprop_parameter_sets));

        h264Parameters->level_asymmetry_allowed =
            !!(fmtp->level_asymmetry_allowed);

        h264Parameters->packetization_mode = fmtp->packetization_mode;
// Copied from VcmSIPCCBinding
#ifdef _WIN32
        sscanf_s(fmtp->profile_level_id, "%x",
                 &h264Parameters->profile_level_id, sizeof(unsigned*));
#else
        sscanf(fmtp->profile_level_id, "%xu",
               &h264Parameters->profile_level_id);
#endif
        h264Parameters->max_mbps = fmtp->max_mbps;
        h264Parameters->max_fs = fmtp->max_fs;
        h264Parameters->max_cpb = fmtp->max_cpb;
        h264Parameters->max_dpb = fmtp->max_dpb;
        h264Parameters->max_br = fmtp->max_br;

        parameters.reset(h264Parameters);
      } break;
      case RTP_VP9: {
        SdpFmtpAttributeList::VP8Parameters* vp9Parameters(
            new SdpFmtpAttributeList::VP8Parameters(
              SdpRtpmapAttributeList::kVP9));

        vp9Parameters->max_fs = fmtp->max_fs;
        vp9Parameters->max_fr = fmtp->max_fr;

        parameters.reset(vp9Parameters);
      } break;
      case RTP_VP8: {
        SdpFmtpAttributeList::VP8Parameters* vp8Parameters(
            new SdpFmtpAttributeList::VP8Parameters(
              SdpRtpmapAttributeList::kVP8));

        vp8Parameters->max_fs = fmtp->max_fs;
        vp8Parameters->max_fr = fmtp->max_fr;

        parameters.reset(vp8Parameters);
      } break;
      default: {
      }
    }

    fmtps->PushEntry(osPayloadType.str(), paramsString, Move(parameters));
  }

  if (!fmtps->mFmtps.empty()) {
    SetAttribute(fmtps.release());
  }
}

void
SipccSdpAttributeList::LoadMsids(sdp_t* sdp, uint16_t level,
                                 SdpErrorHolder& errorHolder)
{
  uint16_t attrCount = 0;
  if (sdp_attr_num_instances(sdp, level, 0, SDP_ATTR_MSID, &attrCount) !=
      SDP_SUCCESS) {
    MOZ_ASSERT(false, "Unable to get count of msid attributes");
    errorHolder.AddParseError(0, "Unable to get count of msid attributes");
    return;
  }
  auto msids = MakeUnique<SdpMsidAttributeList>();
  for (uint16_t i = 1; i <= attrCount; ++i) {
    uint32_t lineNumber = sdp_attr_line_number(sdp, SDP_ATTR_MSID, level, 0, i);

    const char* identifier = sdp_attr_get_msid_identifier(sdp, level, 0, i);
    if (!identifier) {
      errorHolder.AddParseError(lineNumber, "msid attribute with bad identity");
      continue;
    }

    const char* appdata = sdp_attr_get_msid_appdata(sdp, level, 0, i);
    if (!appdata) {
      errorHolder.AddParseError(lineNumber, "msid attribute with bad appdata");
      continue;
    }

    msids->PushEntry(identifier, appdata);
  }

  if (!msids->mMsids.empty()) {
    SetAttribute(msids.release());
  }
}

void
SipccSdpAttributeList::LoadExtmap(sdp_t* sdp, uint16_t level,
                                  SdpErrorHolder& errorHolder)
{
  auto extmaps = MakeUnique<SdpExtmapAttributeList>();

  for (uint16_t i = 1; i < UINT16_MAX; ++i) {
    sdp_attr_t* attr = sdp_find_attr(sdp, level, 0, SDP_ATTR_EXTMAP, i);

    if (!attr) {
      break;
    }

    sdp_extmap_t* extmap = &(attr->attr.extmap);

    SdpDirectionAttribute::Direction dir = SdpDirectionAttribute::kSendrecv;

    if (extmap->media_direction_specified) {
      ConvertDirection(extmap->media_direction, &dir);
    }

    extmaps->PushEntry(extmap->id, dir, extmap->media_direction_specified,
                       extmap->uri, extmap->extension_attributes);
  }

  if (!extmaps->mExtmaps.empty()) {
    if (!AtSessionLevel() &&
        mSessionLevel->HasAttribute(SdpAttribute::kExtmapAttribute)) {
      uint32_t lineNumber =
          sdp_attr_line_number(sdp, SDP_ATTR_EXTMAP, level, 0, 1);
      errorHolder.AddParseError(
          lineNumber, "extmap attributes in both session and media level");
    }
    SetAttribute(extmaps.release());
  }
}

void
SipccSdpAttributeList::LoadRtcpFb(sdp_t* sdp, uint16_t level,
                                  SdpErrorHolder& errorHolder)
{
  auto rtcpfbs = MakeUnique<SdpRtcpFbAttributeList>();

  for (uint16_t i = 1; i < UINT16_MAX; ++i) {
    sdp_attr_t* attr = sdp_find_attr(sdp, level, 0, SDP_ATTR_RTCP_FB, i);

    if (!attr) {
      break;
    }

    sdp_fmtp_fb_t* rtcpfb = &attr->attr.rtcp_fb;

    SdpRtcpFbAttributeList::Type type;
    std::string parameter;

    // Set type and parameter
    switch (rtcpfb->feedback_type) {
      case SDP_RTCP_FB_ACK:
        type = SdpRtcpFbAttributeList::kAck;
        switch (rtcpfb->param.ack) {
          // TODO: sipcc doesn't seem to support ack with no following token.
          // Issue 189.
          case SDP_RTCP_FB_ACK_RPSI:
            parameter = SdpRtcpFbAttributeList::rpsi;
            break;
          case SDP_RTCP_FB_ACK_APP:
            parameter = SdpRtcpFbAttributeList::app;
            break;
          default:
            // Type we don't care about, ignore.
            continue;
        }
        break;
      case SDP_RTCP_FB_CCM:
        type = SdpRtcpFbAttributeList::kCcm;
        switch (rtcpfb->param.ccm) {
          case SDP_RTCP_FB_CCM_FIR:
            parameter = SdpRtcpFbAttributeList::fir;
            break;
          case SDP_RTCP_FB_CCM_TMMBR:
            parameter = SdpRtcpFbAttributeList::tmmbr;
            break;
          case SDP_RTCP_FB_CCM_TSTR:
            parameter = SdpRtcpFbAttributeList::tstr;
            break;
          case SDP_RTCP_FB_CCM_VBCM:
            parameter = SdpRtcpFbAttributeList::vbcm;
            break;
          default:
            // Type we don't care about, ignore.
            continue;
        }
        break;
      case SDP_RTCP_FB_NACK:
        type = SdpRtcpFbAttributeList::kNack;
        switch (rtcpfb->param.nack) {
          case SDP_RTCP_FB_NACK_BASIC:
            break;
          case SDP_RTCP_FB_NACK_SLI:
            parameter = SdpRtcpFbAttributeList::sli;
            break;
          case SDP_RTCP_FB_NACK_PLI:
            parameter = SdpRtcpFbAttributeList::pli;
            break;
          case SDP_RTCP_FB_NACK_RPSI:
            parameter = SdpRtcpFbAttributeList::rpsi;
            break;
          case SDP_RTCP_FB_NACK_APP:
            parameter = SdpRtcpFbAttributeList::app;
            break;
          default:
            // Type we don't care about, ignore.
            continue;
        }
        break;
      case SDP_RTCP_FB_TRR_INT: {
        type = SdpRtcpFbAttributeList::kTrrInt;
        std::ostringstream os;
        os << rtcpfb->param.trr_int;
        parameter = os.str();
      } break;
      default:
        // Type we don't care about, ignore.
        continue;
    }

    std::stringstream osPayloadType;
    if (rtcpfb->payload_num == UINT16_MAX) {
      osPayloadType << "*";
    } else {
      osPayloadType << rtcpfb->payload_num;
    }

    std::string pt(osPayloadType.str());
    std::string extra(rtcpfb->extra);

    rtcpfbs->PushEntry(pt, type, parameter, extra);
  }

  if (!rtcpfbs->mFeedbacks.empty()) {
    SetAttribute(rtcpfbs.release());
  }
}

void
SipccSdpAttributeList::LoadRtcp(sdp_t* sdp, uint16_t level,
                                SdpErrorHolder& errorHolder)
{
  sdp_attr_t* attr = sdp_find_attr(sdp, level, 0, SDP_ATTR_RTCP, 1);

  if (!attr) {
    return;
  }

  sdp_rtcp_t* rtcp = &attr->attr.rtcp;

  if (rtcp->nettype != SDP_NT_INTERNET) {
    return;
  }

  if (rtcp->addrtype != SDP_AT_IP4 && rtcp->addrtype != SDP_AT_IP6) {
    return;
  }

  if (!strlen(rtcp->addr)) {
    SetAttribute(new SdpRtcpAttribute(rtcp->port));
  } else {
    SetAttribute(
        new SdpRtcpAttribute(
          rtcp->port,
          sdp::kInternet,
          rtcp->addrtype == SDP_AT_IP4 ? sdp::kIPv4 : sdp::kIPv6,
          rtcp->addr));
  }
}

bool
SipccSdpAttributeList::Load(sdp_t* sdp, uint16_t level,
                            SdpErrorHolder& errorHolder)
{

  LoadSimpleStrings(sdp, level, errorHolder);
  LoadSimpleNumbers(sdp, level, errorHolder);
  LoadFlags(sdp, level);
  LoadDirection(sdp, level, errorHolder);

  if (AtSessionLevel()) {
    if (!LoadGroups(sdp, level, errorHolder)) {
      return false;
    }

    if (!LoadMsidSemantics(sdp, level, errorHolder)) {
      return false;
    }
  } else {
    sdp_media_e mtype = sdp_get_media_type(sdp, level);
    if (mtype == SDP_MEDIA_APPLICATION) {
      if (!LoadSctpmap(sdp, level, errorHolder)) {
        return false;
      }
    } else {
      if (!LoadRtpmap(sdp, level, errorHolder)) {
        return false;
      }
    }
    LoadCandidate(sdp, level);
    LoadFmtp(sdp, level);
    LoadMsids(sdp, level, errorHolder);
    LoadRtcpFb(sdp, level, errorHolder);
    LoadRtcp(sdp, level, errorHolder);
    LoadSsrc(sdp, level);
    if (!LoadImageattr(sdp, level, errorHolder)) {
      return false;
    }
    if (!LoadSimulcast(sdp, level, errorHolder)) {
      return false;
    }
  }

  LoadIceAttributes(sdp, level);
  if (!LoadFingerprint(sdp, level, errorHolder)) {
    return false;
  }
  LoadSetup(sdp, level);
  LoadExtmap(sdp, level, errorHolder);

  return true;
}

bool
SipccSdpAttributeList::IsAllowedHere(SdpAttribute::AttributeType type) const
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
SipccSdpAttributeList::WarnAboutMisplacedAttribute(
    SdpAttribute::AttributeType type, uint32_t lineNumber,
    SdpErrorHolder& errorHolder)
{
  std::string warning = SdpAttribute::GetAttributeTypeString(type) +
                        (AtSessionLevel() ? " at session level. Ignoring."
                                          : " at media level. Ignoring.");
  errorHolder.AddParseError(lineNumber, warning);
}

const std::vector<std::string>&
SipccSdpAttributeList::GetCandidate() const
{
  if (!HasAttribute(SdpAttribute::kCandidateAttribute)) {
    MOZ_CRASH();
  }

  return static_cast<const SdpMultiStringAttribute*>(
             GetAttribute(SdpAttribute::kCandidateAttribute))->mValues;
}

const SdpConnectionAttribute&
SipccSdpAttributeList::GetConnection() const
{
  if (!HasAttribute(SdpAttribute::kConnectionAttribute)) {
    MOZ_CRASH();
  }

  return *static_cast<const SdpConnectionAttribute*>(
             GetAttribute(SdpAttribute::kConnectionAttribute));
}

SdpDirectionAttribute::Direction
SipccSdpAttributeList::GetDirection() const
{
  if (!HasAttribute(SdpAttribute::kDirectionAttribute)) {
    MOZ_CRASH();
  }

  const SdpAttribute* attr = GetAttribute(SdpAttribute::kDirectionAttribute);
  return static_cast<const SdpDirectionAttribute*>(attr)->mValue;
}

const SdpExtmapAttributeList&
SipccSdpAttributeList::GetExtmap() const
{
  if (!HasAttribute(SdpAttribute::kExtmapAttribute)) {
    MOZ_CRASH();
  }

  return *static_cast<const SdpExtmapAttributeList*>(
             GetAttribute(SdpAttribute::kExtmapAttribute));
}

const SdpFingerprintAttributeList&
SipccSdpAttributeList::GetFingerprint() const
{
  if (!HasAttribute(SdpAttribute::kFingerprintAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kFingerprintAttribute);
  return *static_cast<const SdpFingerprintAttributeList*>(attr);
}

const SdpFmtpAttributeList&
SipccSdpAttributeList::GetFmtp() const
{
  if (!HasAttribute(SdpAttribute::kFmtpAttribute)) {
    MOZ_CRASH();
  }

  return *static_cast<const SdpFmtpAttributeList*>(
             GetAttribute(SdpAttribute::kFmtpAttribute));
}

const SdpGroupAttributeList&
SipccSdpAttributeList::GetGroup() const
{
  if (!HasAttribute(SdpAttribute::kGroupAttribute)) {
    MOZ_CRASH();
  }

  return *static_cast<const SdpGroupAttributeList*>(
             GetAttribute(SdpAttribute::kGroupAttribute));
}

const SdpOptionsAttribute&
SipccSdpAttributeList::GetIceOptions() const
{
  if (!HasAttribute(SdpAttribute::kIceOptionsAttribute)) {
    MOZ_CRASH();
  }

  const SdpAttribute* attr = GetAttribute(SdpAttribute::kIceOptionsAttribute);
  return *static_cast<const SdpOptionsAttribute*>(attr);
}

const std::string&
SipccSdpAttributeList::GetIcePwd() const
{
  if (!HasAttribute(SdpAttribute::kIcePwdAttribute)) {
    return kEmptyString;
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kIcePwdAttribute);
  return static_cast<const SdpStringAttribute*>(attr)->mValue;
}

const std::string&
SipccSdpAttributeList::GetIceUfrag() const
{
  if (!HasAttribute(SdpAttribute::kIceUfragAttribute)) {
    return kEmptyString;
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kIceUfragAttribute);
  return static_cast<const SdpStringAttribute*>(attr)->mValue;
}

const std::string&
SipccSdpAttributeList::GetIdentity() const
{
  if (!HasAttribute(SdpAttribute::kIdentityAttribute)) {
    return kEmptyString;
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kIdentityAttribute);
  return static_cast<const SdpStringAttribute*>(attr)->mValue;
}

const SdpImageattrAttributeList&
SipccSdpAttributeList::GetImageattr() const
{
  if (!HasAttribute(SdpAttribute::kImageattrAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kImageattrAttribute);
  return *static_cast<const SdpImageattrAttributeList*>(attr);
}

const SdpSimulcastAttribute&
SipccSdpAttributeList::GetSimulcast() const
{
  if (!HasAttribute(SdpAttribute::kSimulcastAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kSimulcastAttribute);
  return *static_cast<const SdpSimulcastAttribute*>(attr);
}

const std::string&
SipccSdpAttributeList::GetLabel() const
{
  if (!HasAttribute(SdpAttribute::kLabelAttribute)) {
    return kEmptyString;
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kLabelAttribute);
  return static_cast<const SdpStringAttribute*>(attr)->mValue;
}

uint32_t
SipccSdpAttributeList::GetMaxptime() const
{
  if (!HasAttribute(SdpAttribute::kMaxptimeAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kMaxptimeAttribute);
  return static_cast<const SdpNumberAttribute*>(attr)->mValue;
}

const std::string&
SipccSdpAttributeList::GetMid() const
{
  if (!HasAttribute(SdpAttribute::kMidAttribute)) {
    return kEmptyString;
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kMidAttribute);
  return static_cast<const SdpStringAttribute*>(attr)->mValue;
}

const SdpMsidAttributeList&
SipccSdpAttributeList::GetMsid() const
{
  if (!HasAttribute(SdpAttribute::kMsidAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kMsidAttribute);
  return *static_cast<const SdpMsidAttributeList*>(attr);
}

const SdpMsidSemanticAttributeList&
SipccSdpAttributeList::GetMsidSemantic() const
{
  if (!HasAttribute(SdpAttribute::kMsidSemanticAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kMsidSemanticAttribute);
  return *static_cast<const SdpMsidSemanticAttributeList*>(attr);
}

uint32_t
SipccSdpAttributeList::GetPtime() const
{
  if (!HasAttribute(SdpAttribute::kPtimeAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kPtimeAttribute);
  return static_cast<const SdpNumberAttribute*>(attr)->mValue;
}

const SdpRtcpAttribute&
SipccSdpAttributeList::GetRtcp() const
{
  if (!HasAttribute(SdpAttribute::kRtcpAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kRtcpAttribute);
  return *static_cast<const SdpRtcpAttribute*>(attr);
}

const SdpRtcpFbAttributeList&
SipccSdpAttributeList::GetRtcpFb() const
{
  if (!HasAttribute(SdpAttribute::kRtcpFbAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kRtcpFbAttribute);
  return *static_cast<const SdpRtcpFbAttributeList*>(attr);
}

const SdpRemoteCandidatesAttribute&
SipccSdpAttributeList::GetRemoteCandidates() const
{
  MOZ_CRASH("Not yet implemented");
}

const SdpRtpmapAttributeList&
SipccSdpAttributeList::GetRtpmap() const
{
  if (!HasAttribute(SdpAttribute::kRtpmapAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kRtpmapAttribute);
  return *static_cast<const SdpRtpmapAttributeList*>(attr);
}

const SdpSctpmapAttributeList&
SipccSdpAttributeList::GetSctpmap() const
{
  if (!HasAttribute(SdpAttribute::kSctpmapAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kSctpmapAttribute);
  return *static_cast<const SdpSctpmapAttributeList*>(attr);
}

const SdpSetupAttribute&
SipccSdpAttributeList::GetSetup() const
{
  if (!HasAttribute(SdpAttribute::kSetupAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kSetupAttribute);
  return *static_cast<const SdpSetupAttribute*>(attr);
}

const SdpSsrcAttributeList&
SipccSdpAttributeList::GetSsrc() const
{
  if (!HasAttribute(SdpAttribute::kSsrcAttribute)) {
    MOZ_CRASH();
  }
  const SdpAttribute* attr = GetAttribute(SdpAttribute::kSsrcAttribute);
  return *static_cast<const SdpSsrcAttributeList*>(attr);
}

const SdpSsrcGroupAttributeList&
SipccSdpAttributeList::GetSsrcGroup() const
{
  MOZ_CRASH("Not yet implemented");
}

void
SipccSdpAttributeList::Serialize(std::ostream& os) const
{
  for (size_t i = 0; i < kNumAttributeTypes; ++i) {
    if (mAttributes[i]) {
      os << *mAttributes[i];
    }
  }
}

} // namespace mozilla
