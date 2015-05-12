/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPCODECDESCRIPTION_H_
#define _JSEPCODECDESCRIPTION_H_

#include <iostream>
#include <string>
#include <string.h>
#include "signaling/src/sdp/SdpMediaSection.h"
#include "nsCRT.h"

namespace mozilla {

#define JSEP_CODEC_CLONE(T)                                                    \
  virtual JsepCodecDescription* Clone() const override                         \
  {                                                                            \
    return new T(*this);                                                       \
  }

// A single entry in our list of known codecs.
struct JsepCodecDescription {
  JsepCodecDescription(mozilla::SdpMediaSection::MediaType type,
                       const std::string& defaultPt,
                       const std::string& name,
                       uint32_t clock,
                       uint32_t channels,
                       bool enabled)
      : mType(type),
        mDefaultPt(defaultPt),
        mName(name),
        mClock(clock),
        mChannels(channels),
        mEnabled(enabled),
        mStronglyPreferred(false)
  {
  }
  virtual ~JsepCodecDescription() {}

  virtual JsepCodecDescription* Clone() const = 0;
  virtual void AddFmtps(SdpFmtpAttributeList& fmtp) const = 0;
  virtual void AddRtcpFbs(SdpRtcpFbAttributeList& rtcpfb) const = 0;

  // TODO(bug 1142105): This probably should be a helper function in
  // /sdp
  static bool
  GetPtAsInt(const std::string& ptString, uint16_t* ptOutparam)
  {
    char* end;
    unsigned long pt = strtoul(ptString.c_str(), &end, 10);
    size_t length = static_cast<size_t>(end - ptString.c_str());
    if ((pt > UINT16_MAX) || (length != ptString.size())) {
      return false;
    }
    *ptOutparam = pt;
    return true;
  }

  bool
  GetPtAsInt(uint16_t* ptOutparam) const
  {
    return GetPtAsInt(mDefaultPt, ptOutparam);
  }

  virtual bool
  Matches(const std::string& fmt, const SdpMediaSection& remoteMsection) const
  {
    auto& attrs = remoteMsection.GetAttributeList();
    if (!attrs.HasAttribute(SdpAttribute::kRtpmapAttribute)) {
      return false;
    }

    const SdpRtpmapAttributeList& rtpmap = attrs.GetRtpmap();
    if (!rtpmap.HasEntry(fmt)) {
      return false;
    }

    const SdpRtpmapAttributeList::Rtpmap& entry = rtpmap.GetEntry(fmt);

    if (mType == remoteMsection.GetMediaType()
        && !nsCRT::strcasecmp(mName.c_str(), entry.name.c_str())
        && (mClock == entry.clock)
        && (mChannels == entry.channels)) {
      return ParametersMatch(entry.pt, remoteMsection);
    }
    return false;
  }

  virtual bool
  ParametersMatch(const std::string& fmt,
                  const SdpMediaSection& remoteMsection) const
  {
    return true;
  }

  UniquePtr<JsepCodecDescription>
  MakeNegotiatedCodec(const SdpMediaSection& remoteMsection) const
  {
    UniquePtr<JsepCodecDescription> negotiated(Clone());

    if (!negotiated->Negotiate(remoteMsection)) {
      negotiated.reset();
    }

    return negotiated;
  }

  virtual bool
  Negotiate(const SdpMediaSection& remoteMsection)
  {
    return true;
  }

  // TODO(bug 1142105): This probably should be a helper function in
  // /sdp
  static const SdpFmtpAttributeList::Parameters*
  FindParameters(const std::string& pt,
                 const mozilla::SdpMediaSection& remoteMsection)
  {
    const SdpAttributeList& attrs = remoteMsection.GetAttributeList();

    if (attrs.HasAttribute(SdpAttribute::kFmtpAttribute)) {
      const SdpFmtpAttributeList& fmtps = attrs.GetFmtp();
      for (auto i = fmtps.mFmtps.begin(); i != fmtps.mFmtps.end(); ++i) {
        if (i->format == pt && i->parameters) {
          return i->parameters.get();
        }
      }
    }
    return nullptr;
  }

  UniquePtr<JsepCodecDescription>
  MakeSendCodec(const mozilla::SdpMediaSection& remoteMsection) const
  {
    UniquePtr<JsepCodecDescription> sendCodec(Clone());

    if (!sendCodec->LoadSendParameters(remoteMsection)) {
      sendCodec.reset();
    }

    return sendCodec;
  }

  UniquePtr<JsepCodecDescription>
  MakeRecvCodec(const mozilla::SdpMediaSection& remoteMsection) const
  {
    UniquePtr<JsepCodecDescription> recvCodec(Clone());

    if (!recvCodec->LoadRecvParameters(remoteMsection)) {
      recvCodec.reset();
    }

    return recvCodec;
  }

  virtual bool LoadSendParameters(
      const mozilla::SdpMediaSection& remoteMsection)
  {
    return true;
  }

  virtual bool LoadRecvParameters(
      const mozilla::SdpMediaSection& remoteMsection)
  {
    return true;
  }

  virtual void
  AddToMediaSection(SdpMediaSection& msection) const
  {
    if (mEnabled && msection.GetMediaType() == mType) {
      if (mType == SdpMediaSection::kApplication) {
        // Hack: using mChannels for number of streams
        msection.AddDataChannel(mDefaultPt, mName, mChannels);
      } else {
        msection.AddCodec(mDefaultPt, mName, mClock, mChannels);
      }
      AddFmtpsToMSection(msection);
      AddRtcpFbsToMSection(msection);
    }
  }

  virtual void
  AddFmtpsToMSection(SdpMediaSection& msection) const
  {
    SdpAttributeList& attrs = msection.GetAttributeList();

    UniquePtr<SdpFmtpAttributeList> fmtps;

    if (attrs.HasAttribute(SdpAttribute::kFmtpAttribute)) {
      fmtps.reset(new SdpFmtpAttributeList(attrs.GetFmtp()));
    } else {
      fmtps.reset(new SdpFmtpAttributeList);
    }

    AddFmtps(*fmtps);

    if (!fmtps->mFmtps.empty()) {
      attrs.SetAttribute(fmtps.release());
    }
  }

  virtual void
  AddRtcpFbsToMSection(SdpMediaSection& msection) const
  {
    SdpAttributeList& attrs = msection.GetAttributeList();

    UniquePtr<SdpRtcpFbAttributeList> rtcpfbs;

    if (attrs.HasAttribute(SdpAttribute::kRtcpFbAttribute)) {
      rtcpfbs.reset(new SdpRtcpFbAttributeList(attrs.GetRtcpFb()));
    } else {
      rtcpfbs.reset(new SdpRtcpFbAttributeList);
    }

    AddRtcpFbs(*rtcpfbs);

    if (!rtcpfbs->mFeedbacks.empty()) {
      attrs.SetAttribute(rtcpfbs.release());
    }
  }

  mozilla::SdpMediaSection::MediaType mType;
  std::string mDefaultPt;
  std::string mName;
  uint32_t mClock;
  uint32_t mChannels;
  bool mEnabled;
  bool mStronglyPreferred;
};

struct JsepAudioCodecDescription : public JsepCodecDescription {
  JsepAudioCodecDescription(const std::string& defaultPt,
                            const std::string& name,
                            uint32_t clock,
                            uint32_t channels,
                            uint32_t packetSize,
                            uint32_t bitRate,
                            bool enabled = true)
      : JsepCodecDescription(mozilla::SdpMediaSection::kAudio, defaultPt, name,
                             clock, channels, enabled),
        mPacketSize(packetSize),
        mBitrate(bitRate)
  {
  }

  virtual void
  AddFmtps(SdpFmtpAttributeList& fmtp) const override
  {
    // TODO
  }

  virtual void
  AddRtcpFbs(SdpRtcpFbAttributeList& rtcpfb) const override
  {
    // TODO: Do we want to add anything?
  }

  JSEP_CODEC_CLONE(JsepAudioCodecDescription)

  uint32_t mPacketSize;
  uint32_t mBitrate;
};

struct JsepVideoCodecDescription : public JsepCodecDescription {
  JsepVideoCodecDescription(const std::string& defaultPt,
                            const std::string& name,
                            uint32_t clock,
                            bool enabled = true)
      : JsepCodecDescription(mozilla::SdpMediaSection::kVideo, defaultPt, name,
                             clock, 0, enabled),
        mMaxFs(0),
        mMaxFr(0),
        mPacketizationMode(0),
        mMaxMbps(0),
        mMaxCpb(0),
        mMaxDpb(0),
        mMaxBr(0)
  {
  }

  virtual void
  AddFmtps(SdpFmtpAttributeList& fmtp) const override
  {
    if (mName == "H264") {
      UniquePtr<SdpFmtpAttributeList::H264Parameters> params =
          MakeUnique<SdpFmtpAttributeList::H264Parameters>();

      params->packetization_mode = mPacketizationMode;
      // Hard-coded, may need to change someday?
      params->level_asymmetry_allowed = true;
      params->profile_level_id = mProfileLevelId;
      params->max_mbps = mMaxMbps;
      params->max_fs = mMaxFs;
      params->max_cpb = mMaxCpb;
      params->max_dpb = mMaxDpb;
      params->max_br = mMaxBr;
      strncpy(params->sprop_parameter_sets,
              mSpropParameterSets.c_str(),
              sizeof(params->sprop_parameter_sets) - 1);
      fmtp.PushEntry(mDefaultPt, "", mozilla::Move(params));
    } else if (mName == "VP8" || mName == "VP9") {
      // VP8 and VP9 share the same SDP parameters thus far
      UniquePtr<SdpFmtpAttributeList::VP8Parameters> params =
          MakeUnique<SdpFmtpAttributeList::VP8Parameters>(
              mName == "VP8" ?
              SdpRtpmapAttributeList::kVP8 :
              SdpRtpmapAttributeList::kVP9);

      params->max_fs = mMaxFs;
      params->max_fr = mMaxFr;
      fmtp.PushEntry(mDefaultPt, "", mozilla::Move(params));
    }
  }

  virtual void
  AddRtcpFbs(SdpRtcpFbAttributeList& rtcpfb) const override
  {
    // Just hard code for now
    rtcpfb.PushEntry(mDefaultPt, SdpRtcpFbAttributeList::kNack);
    rtcpfb.PushEntry(
        mDefaultPt, SdpRtcpFbAttributeList::kNack, SdpRtcpFbAttributeList::pli);
    rtcpfb.PushEntry(
        mDefaultPt, SdpRtcpFbAttributeList::kCcm, SdpRtcpFbAttributeList::fir);
    if (mUseTmmbr) {
      rtcpfb.PushEntry(
          mDefaultPt, SdpRtcpFbAttributeList::kCcm, SdpRtcpFbAttributeList::tmmbr);
    }
  }

  SdpFmtpAttributeList::H264Parameters
  GetH264Parameters(const std::string& pt,
                    const SdpMediaSection& msection) const
  {
    // Will contain defaults if nothing else
    SdpFmtpAttributeList::H264Parameters result;
    auto* params = FindParameters(pt, msection);

    if (params && params->codec_type == SdpRtpmapAttributeList::kH264) {
      result =
        static_cast<const SdpFmtpAttributeList::H264Parameters&>(*params);
    }

    return result;
  }

  SdpFmtpAttributeList::VP8Parameters
  GetVP8Parameters(const std::string& pt,
                   const SdpMediaSection& msection) const
  {
    SdpRtpmapAttributeList::CodecType expectedType(
        mName == "VP8" ?
        SdpRtpmapAttributeList::kVP8 :
        SdpRtpmapAttributeList::kVP9);

    // Will contain defaults if nothing else
    SdpFmtpAttributeList::VP8Parameters result(expectedType);
    auto* params = FindParameters(pt, msection);

    if (params && params->codec_type == expectedType) {
      result =
        static_cast<const SdpFmtpAttributeList::VP8Parameters&>(*params);
    }

    return result;
  }

  virtual bool
  Negotiate(const SdpMediaSection& remoteMsection) override
  {
    if (mName == "H264") {
      SdpFmtpAttributeList::H264Parameters h264Params(
          GetH264Parameters(mDefaultPt, remoteMsection));
      if (!h264Params.level_asymmetry_allowed) {
        SetSaneH264Level(std::min(GetSaneH264Level(h264Params.profile_level_id),
                                  GetSaneH264Level(mProfileLevelId)),
                         &mProfileLevelId);
      }

      // TODO(bug 1143709): max-recv-level support
    }

    return JsepCodecDescription::Negotiate(remoteMsection);
  }

  // Maps the not-so-sane encoding of H264 level into something that is
  // ordered in the way one would expect
  // 1b is 0xAB, everything else is the level left-shifted one half-byte
  // (eg; 1.0 is 0xA0, 1.1 is 0xB0, 3.1 is 0x1F0)
  static uint32_t
  GetSaneH264Level(uint32_t profileLevelId)
  {
    uint32_t profileIdc = (profileLevelId >> 16);

    if (profileIdc == 0x42 || profileIdc == 0x4D || profileIdc == 0x58) {
      if ((profileLevelId & 0x10FF) == 0x100B) {
        // Level 1b
        return 0xAB;
      }
    }

    uint32_t level = profileLevelId & 0xFF;

    if (level == 0x09) {
      // Another way to encode level 1b
      return 0xAB;
    }

    return level << 4;
  }

  static void
  SetSaneH264Level(uint32_t level, uint32_t* profileLevelId)
  {
    uint32_t profileIdc = (*profileLevelId >> 16);
    uint32_t levelMask = 0xFF;

    if (profileIdc == 0x42 || profileIdc == 0x4d || profileIdc == 0x58) {
      levelMask = 0x10FF;
      if (level == 0xAB) {
        // Level 1b
        level = 0x100B;
      } else {
        // Not 1b, just shift
        level = level >> 4;
      }
    } else if (level == 0xAB) {
      // Another way to encode 1b
      level = 0x09;
    } else {
      // Not 1b, just shift
      level = level >> 4;
    }

    *profileLevelId = (*profileLevelId & ~levelMask) | level;
  }

  virtual bool
  LoadSendParameters(const mozilla::SdpMediaSection& remoteMsection) override
  {

    if (mName == "H264") {
      SdpFmtpAttributeList::H264Parameters h264Params(
          GetH264Parameters(mDefaultPt, remoteMsection));

      if (!h264Params.level_asymmetry_allowed) {
        SetSaneH264Level(std::min(GetSaneH264Level(h264Params.profile_level_id),
                                  GetSaneH264Level(mProfileLevelId)),
                         &mProfileLevelId);
      } else {
        SetSaneH264Level(GetSaneH264Level(h264Params.profile_level_id),
                         &mProfileLevelId);
      }

      mMaxFs = h264Params.max_fs;
      mMaxMbps = h264Params.max_mbps;
      mMaxCpb = h264Params.max_cpb;
      mMaxDpb = h264Params.max_dpb;
      mMaxBr = h264Params.max_br;
      mSpropParameterSets = h264Params.sprop_parameter_sets;
    } else if (mName == "VP8" || mName == "VP9") {
      SdpFmtpAttributeList::VP8Parameters vp8Params(
          GetVP8Parameters(mDefaultPt, remoteMsection));

      mMaxFs = vp8Params.max_fs;
      mMaxFr = vp8Params.max_fr;
    }
    return true;
  }

  virtual bool
  LoadRecvParameters(const mozilla::SdpMediaSection& remoteMsection) override
  {
    const SdpAttributeList& attrs(remoteMsection.GetAttributeList());

    if (attrs.HasAttribute(SdpAttribute::kRtcpFbAttribute)) {
      auto& rtcpfbs = attrs.GetRtcpFb().mFeedbacks;
      for (auto i = rtcpfbs.begin(); i != rtcpfbs.end(); ++i) {
        if (i->pt == mDefaultPt || i->pt == "*") {
          switch (i->type) {
            case SdpRtcpFbAttributeList::kAck:
              mAckFbTypes.push_back(i->parameter);
              break;
            case SdpRtcpFbAttributeList::kCcm:
              mCcmFbTypes.push_back(i->parameter);
              break;
            case SdpRtcpFbAttributeList::kNack:
              mNackFbTypes.push_back(i->parameter);
              break;
            case SdpRtcpFbAttributeList::kApp:
            case SdpRtcpFbAttributeList::kTrrInt:
              // We don't support these, ignore.
              {}
          }
        }
      }
    }

    if (mName == "H264") {
      SdpFmtpAttributeList::H264Parameters h264Params(
          GetH264Parameters(mDefaultPt, remoteMsection));
      if (!h264Params.level_asymmetry_allowed) {
        SetSaneH264Level(std::min(GetSaneH264Level(h264Params.profile_level_id),
                                  GetSaneH264Level(mProfileLevelId)),
                         &mProfileLevelId);
      }
    }
    return true;
  }

  enum Subprofile {
    kH264ConstrainedBaseline,
    kH264Baseline,
    kH264Main,
    kH264Extended,
    kH264High,
    kH264High10,
    kH264High42,
    kH264High44,
    kH264High10I,
    kH264High42I,
    kH264High44I,
    kH264CALVC44,
    kH264UnknownSubprofile
  };

  static Subprofile
  GetSubprofile(uint32_t profileLevelId)
  {
    // Based on Table 5 from RFC 6184:
    //        Profile     profile_idc        profile-iop
    //                    (hexadecimal)      (binary)

    //        CB          42 (B)             x1xx0000
    //           same as: 4D (M)             1xxx0000
    //           same as: 58 (E)             11xx0000
    //        B           42 (B)             x0xx0000
    //           same as: 58 (E)             10xx0000
    //        M           4D (M)             0x0x0000
    //        E           58                 00xx0000
    //        H           64                 00000000
    //        H10         6E                 00000000
    //        H42         7A                 00000000
    //        H44         F4                 00000000
    //        H10I        6E                 00010000
    //        H42I        7A                 00010000
    //        H44I        F4                 00010000
    //        C44I        2C                 00010000

    if ((profileLevelId & 0xFF4F00) == 0x424000) {
      // 01001111 (mask, 0x4F)
      // x1xx0000 (from table)
      // 01000000 (expected value, 0x40)
      return kH264ConstrainedBaseline;
    }

    if ((profileLevelId & 0xFF8F00) == 0x4D8000) {
      // 10001111 (mask, 0x8F)
      // 1xxx0000 (from table)
      // 10000000 (expected value, 0x80)
      return kH264ConstrainedBaseline;
    }

    if ((profileLevelId & 0xFFCF00) == 0x58C000) {
      // 11001111 (mask, 0xCF)
      // 11xx0000 (from table)
      // 11000000 (expected value, 0xC0)
      return kH264ConstrainedBaseline;
    }

    if ((profileLevelId & 0xFF4F00) == 0x420000) {
      // 01001111 (mask, 0x4F)
      // x0xx0000 (from table)
      // 00000000 (expected value)
      return kH264Baseline;
    }

    if ((profileLevelId & 0xFFCF00) == 0x588000) {
      // 11001111 (mask, 0xCF)
      // 10xx0000 (from table)
      // 10000000 (expected value, 0x80)
      return kH264Baseline;
    }

    if ((profileLevelId & 0xFFAF00) == 0x4D0000) {
      // 10101111 (mask, 0xAF)
      // 0x0x0000 (from table)
      // 00000000 (expected value)
      return kH264Main;
    }

    if ((profileLevelId & 0xFF0000) == 0x580000) {
      // 11001111 (mask, 0xCF)
      // 00xx0000 (from table)
      // 00000000 (expected value)
      return kH264Extended;
    }

    if ((profileLevelId & 0xFFFF00) == 0x640000) {
      return kH264High;
    }

    if ((profileLevelId & 0xFFFF00) == 0x6E0000) {
      return kH264High10;
    }

    if ((profileLevelId & 0xFFFF00) == 0x7A0000) {
      return kH264High42;
    }

    if ((profileLevelId & 0xFFFF00) == 0xF40000) {
      return kH264High44;
    }

    if ((profileLevelId & 0xFFFF00) == 0x6E1000) {
      return kH264High10I;
    }

    if ((profileLevelId & 0xFFFF00) == 0x7A1000) {
      return kH264High42I;
    }

    if ((profileLevelId & 0xFFFF00) == 0xF41000) {
      return kH264High44I;
    }

    if ((profileLevelId & 0xFFFF00) == 0x2C1000) {
      return kH264CALVC44;
    }

    return kH264UnknownSubprofile;
  }

  virtual bool
  ParametersMatch(const std::string& fmt,
                  const SdpMediaSection& remoteMsection) const override
  {
    if (mName == "H264") {
      SdpFmtpAttributeList::H264Parameters h264Params(
          GetH264Parameters(fmt, remoteMsection));

      if (h264Params.packetization_mode != mPacketizationMode) {
        return false;
      }

      if (GetSubprofile(h264Params.profile_level_id) !=
          GetSubprofile(mProfileLevelId)) {
        return false;
      }
    }

    return true;
  }

  JSEP_CODEC_CLONE(JsepVideoCodecDescription)

  std::vector<std::string> mAckFbTypes;
  std::vector<std::string> mNackFbTypes;
  std::vector<std::string> mCcmFbTypes;

  uint32_t mMaxFs;

  // H264-specific stuff
  uint32_t mProfileLevelId;
  uint32_t mMaxFr;
  uint32_t mPacketizationMode;
  uint32_t mMaxMbps;
  uint32_t mMaxCpb;
  uint32_t mMaxDpb;
  uint32_t mMaxBr;
  bool     mUseTmmbr;
  std::string mSpropParameterSets;
};

struct JsepApplicationCodecDescription : public JsepCodecDescription {
  JsepApplicationCodecDescription(const std::string& defaultPt,
                                  const std::string& name,
                                  uint16_t channels,
                                  bool enabled = true)
      : JsepCodecDescription(mozilla::SdpMediaSection::kApplication, defaultPt,
                             name, 0, channels, enabled)
  {
  }

  virtual void
  AddFmtps(SdpFmtpAttributeList& fmtp) const override
  {
    // TODO: Is there anything to do here?
  }

  virtual void
  AddRtcpFbs(SdpRtcpFbAttributeList& rtcpfb) const override
  {
    // Nothing to do here.
  }

  JSEP_CODEC_CLONE(JsepApplicationCodecDescription)

  // Override, uses sctpmap instead of rtpmap
  virtual bool
  Matches(const std::string& fmt,
          const SdpMediaSection& remoteMsection) const override
  {
    auto& attrs = remoteMsection.GetAttributeList();
    if (!attrs.HasAttribute(SdpAttribute::kSctpmapAttribute)) {
      return false;
    }

    const SdpSctpmapAttributeList& sctpmap = attrs.GetSctpmap();
    if (!sctpmap.HasEntry(fmt)) {
      return false;
    }

    const SdpSctpmapAttributeList::Sctpmap& entry = sctpmap.GetEntry(fmt);

    if (mType == remoteMsection.GetMediaType() &&
        !nsCRT::strcasecmp(mName.c_str(), entry.name.c_str())) {
      return true;
    }
    return false;
  }
};

} // namespace mozilla

#endif
