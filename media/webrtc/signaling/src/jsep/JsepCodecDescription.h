/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPCODECDESCRIPTION_H_
#define _JSEPCODECDESCRIPTION_H_

#include <string>
#include "signaling/src/sdp/SdpMediaSection.h"
#include "signaling/src/sdp/SdpHelper.h"
#include "nsCRT.h"

namespace mozilla {

#define JSEP_CODEC_CLONE(T)                                                    \
  virtual JsepCodecDescription* Clone() const override                         \
  {                                                                            \
    return new T(*this);                                                       \
  }

// A single entry in our list of known codecs.
class JsepCodecDescription {
 public:
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
        mStronglyPreferred(false),
        mDirection(sdp::kSend)
  {
  }
  virtual ~JsepCodecDescription() {}

  virtual JsepCodecDescription* Clone() const = 0;

  bool
  GetPtAsInt(uint16_t* ptOutparam) const
  {
    return SdpHelper::GetPtAsInt(mDefaultPt, ptOutparam);
  }

  virtual bool
  Matches(const std::string& fmt, const SdpMediaSection& remoteMsection) const
  {
    if (mType != remoteMsection.GetMediaType()) {
      return false;
    }

    const SdpRtpmapAttributeList::Rtpmap* entry(remoteMsection.FindRtpmap(fmt));

    if (entry) {
      if (!nsCRT::strcasecmp(mName.c_str(), entry->name.c_str())
          && (mClock == entry->clock)
          && (mChannels == entry->channels)) {
        return ParametersMatch(fmt, remoteMsection);
      }
    } else if (!fmt.compare("9") && mName == "G722") {
      return true;
    } else if (!fmt.compare("0") && mName == "PCMU") {
      return true;
    } else if (!fmt.compare("8") && mName == "PCMA") {
      return true;
    }
    return false;
  }

  virtual bool
  ParametersMatch(const std::string& fmt,
                  const SdpMediaSection& remoteMsection) const
  {
    return true;
  }

  virtual bool
  Negotiate(const std::string& pt, const SdpMediaSection& remoteMsection)
  {
    mDefaultPt = pt;
    return true;
  }

  virtual void
  AddToMediaSection(SdpMediaSection& msection) const
  {
    if (mEnabled && msection.GetMediaType() == mType) {
      // Both send and recv codec will have the same pt, so don't add twice
      if (!msection.HasFormat(mDefaultPt)) {
        if (mType == SdpMediaSection::kApplication) {
          // Hack: using mChannels for number of streams
          msection.AddDataChannel(mDefaultPt, mName, mChannels);
        } else {
          msection.AddCodec(mDefaultPt, mName, mClock, mChannels);
        }
      }

      AddParametersToMSection(msection);
    }
  }

  virtual void AddParametersToMSection(SdpMediaSection& msection) const {}

  mozilla::SdpMediaSection::MediaType mType;
  std::string mDefaultPt;
  std::string mName;
  uint32_t mClock;
  uint32_t mChannels;
  bool mEnabled;
  bool mStronglyPreferred;
  sdp::Direction mDirection;
  // Will hold constraints from both fmtp and rid
  EncodingConstraints mConstraints;
};

class JsepAudioCodecDescription : public JsepCodecDescription {
 public:
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
        mBitrate(bitRate),
        mMaxPlaybackRate(0),
        mForceMono(false)
  {
  }

  JSEP_CODEC_CLONE(JsepAudioCodecDescription)

  SdpFmtpAttributeList::OpusParameters
  GetOpusParameters(const std::string& pt,
                    const SdpMediaSection& msection) const
  {
    // Will contain defaults if nothing else
    SdpFmtpAttributeList::OpusParameters result;
    auto* params = msection.FindFmtp(pt);

    if (params && params->codec_type == SdpRtpmapAttributeList::kOpus) {
      result =
        static_cast<const SdpFmtpAttributeList::OpusParameters&>(*params);
    }

    return result;
  }

  void
  AddParametersToMSection(SdpMediaSection& msection) const override
  {
    if (mDirection == sdp::kSend) {
      return;
    }

    if (mName == "opus") {
      SdpFmtpAttributeList::OpusParameters opusParams(
          GetOpusParameters(mDefaultPt, msection));
      if (mMaxPlaybackRate) {
        opusParams.maxplaybackrate = mMaxPlaybackRate;
      }
      if (mChannels == 2 && !mForceMono) {
        // We prefer to receive stereo, if available.
        opusParams.stereo = 1;
      }
      msection.SetFmtp(SdpFmtpAttributeList::Fmtp(mDefaultPt, opusParams));
    }
  }

  bool
  Negotiate(const std::string& pt,
            const SdpMediaSection& remoteMsection) override
  {
    JsepCodecDescription::Negotiate(pt, remoteMsection);
    if (mName == "opus" && mDirection == sdp::kSend) {
      SdpFmtpAttributeList::OpusParameters opusParams(
          GetOpusParameters(mDefaultPt, remoteMsection));

      mMaxPlaybackRate = opusParams.maxplaybackrate;
      mForceMono = !opusParams.stereo;
    }

    return true;
  }

  uint32_t mPacketSize;
  uint32_t mBitrate;
  uint32_t mMaxPlaybackRate;
  bool mForceMono;
};

class JsepVideoCodecDescription : public JsepCodecDescription {
 public:
  JsepVideoCodecDescription(const std::string& defaultPt,
                            const std::string& name,
                            uint32_t clock,
                            bool enabled = true)
      : JsepCodecDescription(mozilla::SdpMediaSection::kVideo, defaultPt, name,
                             clock, 0, enabled),
        mTmmbrEnabled(false),
        mRembEnabled(false),
        mPacketizationMode(0)
  {
    // Add supported rtcp-fb types
    mNackFbTypes.push_back("");
    mNackFbTypes.push_back(SdpRtcpFbAttributeList::pli);
    mCcmFbTypes.push_back(SdpRtcpFbAttributeList::fir);
  }

  virtual void
  EnableTmmbr() {
    // EnableTmmbr can be called multiple times due to multiple calls to
    // PeerConnectionImpl::ConfigureJsepSessionCodecs
    if (!mTmmbrEnabled) {
      mTmmbrEnabled = true;
      mCcmFbTypes.push_back(SdpRtcpFbAttributeList::tmmbr);
    }
  }

  virtual void
  EnableRemb() {
    // EnableRemb can be called multiple times due to multiple calls to
    // PeerConnectionImpl::ConfigureJsepSessionCodecs
    if (!mRembEnabled) {
      mRembEnabled = true;
      mOtherFbTypes.push_back({ "", SdpRtcpFbAttributeList::kRemb, "", ""});
    }
  }

  void
  AddParametersToMSection(SdpMediaSection& msection) const override
  {
    AddFmtpsToMSection(msection);
    AddRtcpFbsToMSection(msection);
  }

  void
  AddFmtpsToMSection(SdpMediaSection& msection) const
  {
    if (mName == "H264") {
      SdpFmtpAttributeList::H264Parameters h264Params(
          GetH264Parameters(mDefaultPt, msection));

      if (mDirection == sdp::kSend) {
        if (!h264Params.level_asymmetry_allowed) {
          // First time the fmtp has been set; set just in case this is for a
          // sendonly m-line, since even though we aren't receiving the level
          // negotiation still needs to happen (sigh).
          h264Params.profile_level_id = mProfileLevelId;
        }
      } else {
        // Parameters that only apply to what we receive
        h264Params.max_mbps = mConstraints.maxMbps;
        h264Params.max_fs = mConstraints.maxFs;
        h264Params.max_cpb = mConstraints.maxCpb;
        h264Params.max_dpb = mConstraints.maxDpb;
        h264Params.max_br = mConstraints.maxBr;
        strncpy(h264Params.sprop_parameter_sets,
                mSpropParameterSets.c_str(),
                sizeof(h264Params.sprop_parameter_sets) - 1);
        h264Params.profile_level_id = mProfileLevelId;
      }

      // Parameters that apply to both the send and recv directions
      h264Params.packetization_mode = mPacketizationMode;
      // Hard-coded, may need to change someday?
      h264Params.level_asymmetry_allowed = true;

      msection.SetFmtp(SdpFmtpAttributeList::Fmtp(mDefaultPt, h264Params));
    } else if (mName == "VP8" || mName == "VP9") {
      if (mDirection == sdp::kRecv) {
        // VP8 and VP9 share the same SDP parameters thus far
        SdpFmtpAttributeList::VP8Parameters vp8Params(
            GetVP8Parameters(mDefaultPt, msection));

        vp8Params.max_fs = mConstraints.maxFs;
        vp8Params.max_fr = mConstraints.maxFps;
        msection.SetFmtp(SdpFmtpAttributeList::Fmtp(mDefaultPt, vp8Params));
      }
    }
  }

  void
  AddRtcpFbsToMSection(SdpMediaSection& msection) const
  {
    SdpRtcpFbAttributeList rtcpfbs(msection.GetRtcpFbs());
    for (const auto& rtcpfb : rtcpfbs.mFeedbacks) {
      if (rtcpfb.pt == mDefaultPt) {
        // Already set by the codec for the other direction.
        return;
      }
    }

    for (const std::string& type : mAckFbTypes) {
      rtcpfbs.PushEntry(mDefaultPt, SdpRtcpFbAttributeList::kAck, type);
    }
    for (const std::string& type : mNackFbTypes) {
      rtcpfbs.PushEntry(mDefaultPt, SdpRtcpFbAttributeList::kNack, type);
    }
    for (const std::string& type : mCcmFbTypes) {
      rtcpfbs.PushEntry(mDefaultPt, SdpRtcpFbAttributeList::kCcm, type);
    }
    for (const auto& fb : mOtherFbTypes) {
      rtcpfbs.PushEntry(mDefaultPt, fb.type, fb.parameter, fb.extra);
    }

    msection.SetRtcpFbs(rtcpfbs);
  }

  SdpFmtpAttributeList::H264Parameters
  GetH264Parameters(const std::string& pt,
                    const SdpMediaSection& msection) const
  {
    // Will contain defaults if nothing else
    SdpFmtpAttributeList::H264Parameters result;
    auto* params = msection.FindFmtp(pt);

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
    auto* params = msection.FindFmtp(pt);

    if (params && params->codec_type == expectedType) {
      result =
        static_cast<const SdpFmtpAttributeList::VP8Parameters&>(*params);
    }

    return result;
  }

  void
  NegotiateRtcpFb(const SdpMediaSection& remoteMsection,
                  SdpRtcpFbAttributeList::Type type,
                  std::vector<std::string>* supportedTypes)
  {
    std::vector<std::string> temp;
    for (auto& subType : *supportedTypes) {
      if (remoteMsection.HasRtcpFb(mDefaultPt, type, subType)) {
        temp.push_back(subType);
      }
    }
    *supportedTypes = temp;
  }

  void
  NegotiateRtcpFb(const SdpMediaSection& remoteMsection,
                  std::vector<SdpRtcpFbAttributeList::Feedback>* supportedFbs) {
    std::vector<SdpRtcpFbAttributeList::Feedback> temp;
    for (auto& fb : *supportedFbs) {
      if (remoteMsection.HasRtcpFb(mDefaultPt, fb.type, fb.parameter)) {
        temp.push_back(fb);
      }
    }
    *supportedFbs = temp;
  }

  void
  NegotiateRtcpFb(const SdpMediaSection& remote)
  {
    // Removes rtcp-fb types that the other side doesn't support
    NegotiateRtcpFb(remote, SdpRtcpFbAttributeList::kAck, &mAckFbTypes);
    NegotiateRtcpFb(remote, SdpRtcpFbAttributeList::kNack, &mNackFbTypes);
    NegotiateRtcpFb(remote, SdpRtcpFbAttributeList::kCcm, &mCcmFbTypes);
    NegotiateRtcpFb(remote, &mOtherFbTypes);
  }

  virtual bool
  Negotiate(const std::string& pt,
            const SdpMediaSection& remoteMsection) override
  {
    JsepCodecDescription::Negotiate(pt, remoteMsection);
    if (mName == "H264") {
      SdpFmtpAttributeList::H264Parameters h264Params(
          GetH264Parameters(mDefaultPt, remoteMsection));

      // Level is negotiated symmetrically if level asymmetry is disallowed
      if (!h264Params.level_asymmetry_allowed) {
        SetSaneH264Level(std::min(GetSaneH264Level(h264Params.profile_level_id),
                                  GetSaneH264Level(mProfileLevelId)),
                         &mProfileLevelId);
      }

      if (mDirection == sdp::kSend) {
        // Remote values of these apply only to the send codec.
        mConstraints.maxFs = h264Params.max_fs;
        mConstraints.maxMbps = h264Params.max_mbps;
        mConstraints.maxCpb = h264Params.max_cpb;
        mConstraints.maxDpb = h264Params.max_dpb;
        mConstraints.maxBr = h264Params.max_br;
        mSpropParameterSets = h264Params.sprop_parameter_sets;
        // Only do this if we didn't symmetrically negotiate above
        if (h264Params.level_asymmetry_allowed) {
          SetSaneH264Level(GetSaneH264Level(h264Params.profile_level_id),
                           &mProfileLevelId);
        }
      } else {
        // TODO(bug 1143709): max-recv-level support
      }

    } else if (mName == "VP8" || mName == "VP9") {
      if (mDirection == sdp::kSend) {
        SdpFmtpAttributeList::VP8Parameters vp8Params(
            GetVP8Parameters(mDefaultPt, remoteMsection));

        mConstraints.maxFs = vp8Params.max_fs;
        mConstraints.maxFps = vp8Params.max_fr;
      }
    }

    NegotiateRtcpFb(remoteMsection);
    return true;
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

  virtual bool
  RtcpFbRembIsSet() const
  {
    for (const auto& fb : mOtherFbTypes) {
      if (fb.type == SdpRtcpFbAttributeList::kRemb) {
        return true;
      }
    }
    return false;
  }

  JSEP_CODEC_CLONE(JsepVideoCodecDescription)

  std::vector<std::string> mAckFbTypes;
  std::vector<std::string> mNackFbTypes;
  std::vector<std::string> mCcmFbTypes;
  std::vector<SdpRtcpFbAttributeList::Feedback> mOtherFbTypes;
  bool mTmmbrEnabled;
  bool mRembEnabled;

  // H264-specific stuff
  uint32_t mProfileLevelId;
  uint32_t mPacketizationMode;
  std::string mSpropParameterSets;
};

class JsepApplicationCodecDescription : public JsepCodecDescription {
 public:
  JsepApplicationCodecDescription(const std::string& defaultPt,
                                  const std::string& name,
                                  uint16_t channels,
                                  bool enabled = true)
      : JsepCodecDescription(mozilla::SdpMediaSection::kApplication, defaultPt,
                             name, 0, channels, enabled)
  {
  }

  JSEP_CODEC_CLONE(JsepApplicationCodecDescription)

  // Override, uses sctpmap instead of rtpmap
  virtual bool
  Matches(const std::string& fmt,
          const SdpMediaSection& remoteMsection) const override
  {
    if (mType != remoteMsection.GetMediaType()) {
      return false;
    }

    const SdpSctpmapAttributeList::Sctpmap* entry(
        remoteMsection.FindSctpmap(fmt));

    if (entry && !nsCRT::strcasecmp(mName.c_str(), entry->name.c_str())) {
      return true;
    }
    return false;
  }
};

} // namespace mozilla

#endif
