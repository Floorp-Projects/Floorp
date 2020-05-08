/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPCODECDESCRIPTION_H_
#define _JSEPCODECDESCRIPTION_H_

#include <string>
#include "signaling/src/sdp/SdpMediaSection.h"
#include "signaling/src/sdp/SdpHelper.h"
#include "nsCRT.h"
#include "mozilla/net/DataChannelProtocol.h"

namespace mozilla {

#define JSEP_CODEC_CLONE(T) \
  virtual JsepCodecDescription* Clone() const override { return new T(*this); }

// A single entry in our list of known codecs.
class JsepCodecDescription {
 public:
  JsepCodecDescription(mozilla::SdpMediaSection::MediaType type,
                       const std::string& defaultPt, const std::string& name,
                       uint32_t clock, uint32_t channels, bool enabled)
      : mType(type),
        mDefaultPt(defaultPt),
        mName(name),
        mClock(clock),
        mChannels(channels),
        mEnabled(enabled),
        mStronglyPreferred(false),
        mDirection(sdp::kSend) {}
  virtual ~JsepCodecDescription() {}

  virtual JsepCodecDescription* Clone() const = 0;

  bool GetPtAsInt(uint16_t* ptOutparam) const {
    return SdpHelper::GetPtAsInt(mDefaultPt, ptOutparam);
  }

  virtual bool Matches(const std::string& fmt,
                       const SdpMediaSection& remoteMsection) const {
    // note: fmt here is remote fmt (to go with remoteMsection)
    if (mType != remoteMsection.GetMediaType()) {
      return false;
    }

    const SdpRtpmapAttributeList::Rtpmap* entry(remoteMsection.FindRtpmap(fmt));

    if (entry) {
      if (!nsCRT::strcasecmp(mName.c_str(), entry->name.c_str()) &&
          (mClock == entry->clock) && (mChannels == entry->channels)) {
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

  virtual bool ParametersMatch(const std::string& fmt,
                               const SdpMediaSection& remoteMsection) const {
    return true;
  }

  virtual bool Negotiate(const std::string& pt,
                         const SdpMediaSection& remoteMsection, bool isOffer) {
    if (mDirection == sdp::kSend || isOffer) {
      mDefaultPt = pt;
    }
    return true;
  }

  virtual void AddToMediaSection(SdpMediaSection& msection) const {
    if (mEnabled && msection.GetMediaType() == mType) {
      if (mDirection == sdp::kRecv) {
        msection.AddCodec(mDefaultPt, mName, mClock, mChannels);
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
                            const std::string& name, uint32_t clock,
                            uint32_t channels, bool enabled = true)
      : JsepCodecDescription(mozilla::SdpMediaSection::kAudio, defaultPt, name,
                             clock, channels, enabled),
        mMaxPlaybackRate(0),
        mForceMono(false),
        mFECEnabled(false),
        mDtmfEnabled(false),
        mMaxAverageBitrate(0),
        mDTXEnabled(false),
        mFrameSizeMs(0),
        mMinFrameSizeMs(0),
        mMaxFrameSizeMs(0),
        mCbrEnabled(false) {}

  JSEP_CODEC_CLONE(JsepAudioCodecDescription)

  SdpFmtpAttributeList::OpusParameters GetOpusParameters(
      const std::string& pt, const SdpMediaSection& msection) const {
    // Will contain defaults if nothing else
    SdpFmtpAttributeList::OpusParameters result;
    auto* params = msection.FindFmtp(pt);

    if (params && params->codec_type == SdpRtpmapAttributeList::kOpus) {
      result =
          static_cast<const SdpFmtpAttributeList::OpusParameters&>(*params);
    }

    return result;
  }

  SdpFmtpAttributeList::TelephoneEventParameters GetTelephoneEventParameters(
      const std::string& pt, const SdpMediaSection& msection) const {
    // Will contain defaults if nothing else
    SdpFmtpAttributeList::TelephoneEventParameters result;
    auto* params = msection.FindFmtp(pt);

    if (params &&
        params->codec_type == SdpRtpmapAttributeList::kTelephoneEvent) {
      result =
          static_cast<const SdpFmtpAttributeList::TelephoneEventParameters&>(
              *params);
    }

    return result;
  }

  void AddParametersToMSection(SdpMediaSection& msection) const override {
    if (mDirection == sdp::kSend) {
      return;
    }

    if (mName == "opus") {
      SdpFmtpAttributeList::OpusParameters opusParams(
          GetOpusParameters(mDefaultPt, msection));
      if (mMaxPlaybackRate) {
        opusParams.maxplaybackrate = mMaxPlaybackRate;
      }
      opusParams.maxAverageBitrate = mMaxAverageBitrate;
      if (mChannels == 2 && !mForceMono) {
        // We prefer to receive stereo, if available.
        opusParams.stereo = 1;
      }
      opusParams.useInBandFec = mFECEnabled ? 1 : 0;
      opusParams.useDTX = mDTXEnabled;
      opusParams.frameSizeMs = mFrameSizeMs;
      opusParams.minFrameSizeMs = mMinFrameSizeMs;
      opusParams.maxFrameSizeMs = mMaxFrameSizeMs;
      opusParams.useCbr = mCbrEnabled;
      msection.SetFmtp(SdpFmtpAttributeList::Fmtp(mDefaultPt, opusParams));
    } else if (mName == "telephone-event") {
      // add the default dtmf tones
      SdpFmtpAttributeList::TelephoneEventParameters teParams(
          GetTelephoneEventParameters(mDefaultPt, msection));
      msection.SetFmtp(SdpFmtpAttributeList::Fmtp(mDefaultPt, teParams));
    }
  }

  bool Negotiate(const std::string& pt, const SdpMediaSection& remoteMsection,
                 bool isOffer) override {
    JsepCodecDescription::Negotiate(pt, remoteMsection, isOffer);
    if (mName == "opus" && mDirection == sdp::kSend) {
      SdpFmtpAttributeList::OpusParameters opusParams(
          GetOpusParameters(mDefaultPt, remoteMsection));

      mMaxPlaybackRate = opusParams.maxplaybackrate;
      mForceMono = !opusParams.stereo;
      // draft-ietf-rtcweb-fec-03.txt section 4.2 says support for FEC
      // at the received side is declarative and can be negotiated
      // separately for either media direction.
      mFECEnabled = opusParams.useInBandFec;
      if ((opusParams.maxAverageBitrate >= 6000) &&
          (opusParams.maxAverageBitrate <= 510000)) {
        mMaxAverageBitrate = opusParams.maxAverageBitrate;
      }
      mDTXEnabled = opusParams.useDTX;
      if (remoteMsection.GetAttributeList().HasAttribute(
              SdpAttribute::kPtimeAttribute)) {
        mFrameSizeMs = remoteMsection.GetAttributeList().GetPtime();
      } else {
        mFrameSizeMs = opusParams.frameSizeMs;
      }
      mMinFrameSizeMs = opusParams.minFrameSizeMs;
      if (remoteMsection.GetAttributeList().HasAttribute(
              SdpAttribute::kMaxptimeAttribute)) {
        mFrameSizeMs = remoteMsection.GetAttributeList().GetMaxptime();
      } else {
        mMaxFrameSizeMs = opusParams.maxFrameSizeMs;
      }
      mCbrEnabled = opusParams.useCbr;
    }

    return true;
  }

  uint32_t mMaxPlaybackRate;
  bool mForceMono;
  bool mFECEnabled;
  bool mDtmfEnabled;
  uint32_t mMaxAverageBitrate;
  bool mDTXEnabled;
  uint32_t mFrameSizeMs;
  uint32_t mMinFrameSizeMs;
  uint32_t mMaxFrameSizeMs;
  bool mCbrEnabled;
};

class JsepVideoCodecDescription : public JsepCodecDescription {
 public:
  JsepVideoCodecDescription(const std::string& defaultPt,
                            const std::string& name, uint32_t clock,
                            bool enabled = true)
      : JsepCodecDescription(mozilla::SdpMediaSection::kVideo, defaultPt, name,
                             clock, 0, enabled),
        mTmmbrEnabled(false),
        mRembEnabled(false),
        mFECEnabled(false),
        mTransportCCEnabled(false),
        mRtxEnabled(false),
        mREDPayloadType(0),
        mULPFECPayloadType(0),
        mProfileLevelId(0),
        mPacketizationMode(0) {
    // Add supported rtcp-fb types
    mNackFbTypes.push_back("");
    mNackFbTypes.push_back(SdpRtcpFbAttributeList::pli);
    mCcmFbTypes.push_back(SdpRtcpFbAttributeList::fir);
  }

  virtual void EnableTmmbr() {
    // EnableTmmbr can be called multiple times due to multiple calls to
    // PeerConnectionImpl::ConfigureJsepSessionCodecs
    if (!mTmmbrEnabled) {
      mTmmbrEnabled = true;
      mCcmFbTypes.push_back(SdpRtcpFbAttributeList::tmmbr);
    }
  }

  virtual void EnableRemb() {
    // EnableRemb can be called multiple times due to multiple calls to
    // PeerConnectionImpl::ConfigureJsepSessionCodecs
    if (!mRembEnabled) {
      mRembEnabled = true;
      mOtherFbTypes.push_back({"", SdpRtcpFbAttributeList::kRemb, "", ""});
    }
  }

  virtual void EnableFec(std::string redPayloadType,
                         std::string ulpfecPayloadType) {
    // Enabling FEC for video works a little differently than enabling
    // REMB or TMMBR.  Support for FEC is indicated by the presence of
    // particular codes (red and ulpfec) instead of using rtcpfb
    // attributes on a given codec.  There is no rtcpfb to push for FEC
    // as can be seen above when REMB or TMMBR are enabled.

    // Ensure we have valid payload types. This returns zero on failure, which
    // is a valid payload type.
    uint16_t redPt, ulpfecPt;
    if (!SdpHelper::GetPtAsInt(redPayloadType, &redPt) ||
        !SdpHelper::GetPtAsInt(ulpfecPayloadType, &ulpfecPt)) {
      return;
    }

    mFECEnabled = true;
    mREDPayloadType = redPt;
    mULPFECPayloadType = ulpfecPt;
  }

  virtual void EnableTransportCC() {
    if (!mTransportCCEnabled) {
      mTransportCCEnabled = true;
      mOtherFbTypes.push_back(
          {"", SdpRtcpFbAttributeList::kTransportCC, "", ""});
    }
  }

  void EnableRtx(const std::string& rtxPayloadType) {
    mRtxEnabled = true;
    mRtxPayloadType = rtxPayloadType;
  }

  void AddParametersToMSection(SdpMediaSection& msection) const override {
    AddFmtpsToMSection(msection);
    AddRtcpFbsToMSection(msection);
  }

  void AddFmtpsToMSection(SdpMediaSection& msection) const {
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
        strncpy(h264Params.sprop_parameter_sets, mSpropParameterSets.c_str(),
                sizeof(h264Params.sprop_parameter_sets) - 1);
        h264Params.profile_level_id = mProfileLevelId;
      }

      // Parameters that apply to both the send and recv directions
      h264Params.packetization_mode = mPacketizationMode;
      // Hard-coded, may need to change someday?
      h264Params.level_asymmetry_allowed = true;

      msection.SetFmtp(SdpFmtpAttributeList::Fmtp(mDefaultPt, h264Params));
    } else if (mName == "red" && !mRedundantEncodings.empty()) {
      SdpFmtpAttributeList::RedParameters redParams(
          GetRedParameters(mDefaultPt, msection));
      redParams.encodings = mRedundantEncodings;
      msection.SetFmtp(SdpFmtpAttributeList::Fmtp(mDefaultPt, redParams));
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

    if (mRtxEnabled && mDirection == sdp::kRecv) {
      SdpFmtpAttributeList::RtxParameters params(
          GetRtxParameters(mDefaultPt, msection));
      uint16_t apt;
      if (SdpHelper::GetPtAsInt(mDefaultPt, &apt)) {
        if (apt <= 127) {
          msection.AddCodec(mRtxPayloadType, "rtx", mClock, mChannels);

          params.apt = apt;
          msection.SetFmtp(SdpFmtpAttributeList::Fmtp(mRtxPayloadType, params));
        }
      }
    }
  }

  void AddRtcpFbsToMSection(SdpMediaSection& msection) const {
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

  SdpFmtpAttributeList::H264Parameters GetH264Parameters(
      const std::string& pt, const SdpMediaSection& msection) const {
    // Will contain defaults if nothing else
    SdpFmtpAttributeList::H264Parameters result;
    auto* params = msection.FindFmtp(pt);

    if (params && params->codec_type == SdpRtpmapAttributeList::kH264) {
      result =
          static_cast<const SdpFmtpAttributeList::H264Parameters&>(*params);
    }

    return result;
  }

  SdpFmtpAttributeList::RedParameters GetRedParameters(
      const std::string& pt, const SdpMediaSection& msection) const {
    SdpFmtpAttributeList::RedParameters result;
    auto* params = msection.FindFmtp(pt);

    if (params && params->codec_type == SdpRtpmapAttributeList::kRed) {
      result = static_cast<const SdpFmtpAttributeList::RedParameters&>(*params);
    }

    return result;
  }

  SdpFmtpAttributeList::RtxParameters GetRtxParameters(
      const std::string& pt, const SdpMediaSection& msection) const {
    SdpFmtpAttributeList::RtxParameters result;
    const auto* params = msection.FindFmtp(pt);

    if (params && params->codec_type == SdpRtpmapAttributeList::kRtx) {
      result = static_cast<const SdpFmtpAttributeList::RtxParameters&>(*params);
    }

    return result;
  }

  Maybe<std::string> GetRtxPtByApt(const std::string& apt,
                                   const SdpMediaSection& msection) const {
    Maybe<std::string> result;
    uint16_t aptAsInt;
    if (!SdpHelper::GetPtAsInt(apt, &aptAsInt)) {
      return result;
    }

    const SdpAttributeList& attrs = msection.GetAttributeList();
    if (attrs.HasAttribute(SdpAttribute::kFmtpAttribute)) {
      for (const auto& fmtpAttr : attrs.GetFmtp().mFmtps) {
        if (fmtpAttr.parameters) {
          auto* params = fmtpAttr.parameters.get();
          if (params && params->codec_type == SdpRtpmapAttributeList::kRtx) {
            const SdpFmtpAttributeList::RtxParameters* rtxParams =
                static_cast<const SdpFmtpAttributeList::RtxParameters*>(params);
            if (rtxParams->apt == aptAsInt) {
              result = Some(fmtpAttr.format);
              break;
            }
          }
        }
      }
    }
    return result;
  }

  SdpFmtpAttributeList::VP8Parameters GetVP8Parameters(
      const std::string& pt, const SdpMediaSection& msection) const {
    SdpRtpmapAttributeList::CodecType expectedType(
        mName == "VP8" ? SdpRtpmapAttributeList::kVP8
                       : SdpRtpmapAttributeList::kVP9);

    // Will contain defaults if nothing else
    SdpFmtpAttributeList::VP8Parameters result(expectedType);
    auto* params = msection.FindFmtp(pt);

    if (params && params->codec_type == expectedType) {
      result = static_cast<const SdpFmtpAttributeList::VP8Parameters&>(*params);
    }

    return result;
  }

  void NegotiateRtcpFb(const SdpMediaSection& remoteMsection,
                       SdpRtcpFbAttributeList::Type type,
                       std::vector<std::string>* supportedTypes) {
    std::vector<std::string> temp;
    for (auto& subType : *supportedTypes) {
      if (remoteMsection.HasRtcpFb(mDefaultPt, type, subType)) {
        temp.push_back(subType);
      }
    }
    *supportedTypes = temp;
  }

  void NegotiateRtcpFb(
      const SdpMediaSection& remoteMsection,
      std::vector<SdpRtcpFbAttributeList::Feedback>* supportedFbs) {
    std::vector<SdpRtcpFbAttributeList::Feedback> temp;
    for (auto& fb : *supportedFbs) {
      if (remoteMsection.HasRtcpFb(mDefaultPt, fb.type, fb.parameter)) {
        temp.push_back(fb);
      }
    }
    *supportedFbs = temp;
  }

  void NegotiateRtcpFb(const SdpMediaSection& remote) {
    // Removes rtcp-fb types that the other side doesn't support
    NegotiateRtcpFb(remote, SdpRtcpFbAttributeList::kAck, &mAckFbTypes);
    NegotiateRtcpFb(remote, SdpRtcpFbAttributeList::kNack, &mNackFbTypes);
    NegotiateRtcpFb(remote, SdpRtcpFbAttributeList::kCcm, &mCcmFbTypes);
    NegotiateRtcpFb(remote, &mOtherFbTypes);
  }

  virtual bool Negotiate(const std::string& pt,
                         const SdpMediaSection& remoteMsection,
                         bool isOffer) override {
    JsepCodecDescription::Negotiate(pt, remoteMsection, isOffer);
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
    } else if (mName == "red") {
      SdpFmtpAttributeList::RedParameters redParams(
          GetRedParameters(mDefaultPt, remoteMsection));
      mRedundantEncodings = redParams.encodings;
    } else if (mName == "VP8" || mName == "VP9") {
      if (mDirection == sdp::kSend) {
        SdpFmtpAttributeList::VP8Parameters vp8Params(
            GetVP8Parameters(mDefaultPt, remoteMsection));

        mConstraints.maxFs = vp8Params.max_fs;
        mConstraints.maxFps = vp8Params.max_fr;
      }
    }

    if (mRtxEnabled && (mDirection == sdp::kSend || isOffer)) {
      Maybe<std::string> rtxPt = GetRtxPtByApt(mDefaultPt, remoteMsection);
      if (rtxPt.isSome()) {
        EnableRtx(*rtxPt);
      } else {
        mRtxEnabled = false;
        mRtxPayloadType = "";
      }
    }

    NegotiateRtcpFb(remoteMsection);
    return true;
  }

  // Maps the not-so-sane encoding of H264 level into something that is
  // ordered in the way one would expect
  // 1b is 0xAB, everything else is the level left-shifted one half-byte
  // (eg; 1.0 is 0xA0, 1.1 is 0xB0, 3.1 is 0x1F0)
  static uint32_t GetSaneH264Level(uint32_t profileLevelId) {
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

  static void SetSaneH264Level(uint32_t level, uint32_t* profileLevelId) {
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

  static Subprofile GetSubprofile(uint32_t profileLevelId) {
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

  virtual bool ParametersMatch(
      const std::string& fmt,
      const SdpMediaSection& remoteMsection) const override {
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

  virtual bool RtcpFbRembIsSet() const {
    for (const auto& fb : mOtherFbTypes) {
      if (fb.type == SdpRtcpFbAttributeList::kRemb) {
        return true;
      }
    }
    return false;
  }

  virtual bool RtcpFbTransportCCIsSet() const {
    for (const auto& fb : mOtherFbTypes) {
      if (fb.type == SdpRtcpFbAttributeList::kTransportCC) {
        return true;
      }
    }
    return false;
  }

  virtual void UpdateRedundantEncodings(
      const std::vector<UniquePtr<JsepCodecDescription>>& codecs) {
    for (const auto& codec : codecs) {
      if (codec->mType == SdpMediaSection::kVideo && codec->mEnabled &&
          codec->mName != "red") {
        uint16_t pt;
        if (!SdpHelper::GetPtAsInt(codec->mDefaultPt, &pt)) {
          continue;
        }
        mRedundantEncodings.push_back(pt);
      }
    }
  }

  JSEP_CODEC_CLONE(JsepVideoCodecDescription)

  std::vector<std::string> mAckFbTypes;
  std::vector<std::string> mNackFbTypes;
  std::vector<std::string> mCcmFbTypes;
  std::vector<SdpRtcpFbAttributeList::Feedback> mOtherFbTypes;
  bool mTmmbrEnabled;
  bool mRembEnabled;
  bool mFECEnabled;
  bool mTransportCCEnabled;
  bool mRtxEnabled;
  uint8_t mREDPayloadType;
  uint8_t mULPFECPayloadType;
  std::string mRtxPayloadType;
  std::vector<uint8_t> mRedundantEncodings;

  // H264-specific stuff
  uint32_t mProfileLevelId;
  uint32_t mPacketizationMode;
  std::string mSpropParameterSets;
};

class JsepApplicationCodecDescription : public JsepCodecDescription {
  // This is the new draft-21 implementation
 public:
  JsepApplicationCodecDescription(const std::string& name, uint16_t channels,
                                  uint16_t localPort,
                                  uint32_t localMaxMessageSize,
                                  bool enabled = true)
      : JsepCodecDescription(mozilla::SdpMediaSection::kApplication, "", name,
                             0, channels, enabled),
        mLocalPort(localPort),
        mLocalMaxMessageSize(localMaxMessageSize),
        mRemotePort(0),
        mRemoteMaxMessageSize(0),
        mRemoteMMSSet(false) {}

  JSEP_CODEC_CLONE(JsepApplicationCodecDescription)

  // Override, uses sctpport or sctpmap instead of rtpmap
  virtual bool Matches(const std::string& fmt,
                       const SdpMediaSection& remoteMsection) const override {
    if (mType != remoteMsection.GetMediaType()) {
      return false;
    }

    int sctp_port = remoteMsection.GetSctpPort();
    bool fmt_matches =
        nsCRT::strcasecmp(mName.c_str(),
                          remoteMsection.GetFormats()[0].c_str()) == 0;
    if (sctp_port && fmt_matches) {
      // New sctp draft 21 format
      return true;
    }

    const SdpSctpmapAttributeList::Sctpmap* sctp_map(
        remoteMsection.GetSctpmap());
    if (sctp_map) {
      // Old sctp draft 05 format
      return nsCRT::strcasecmp(mName.c_str(), sctp_map->name.c_str()) == 0;
    }

    return false;
  }

  virtual void AddToMediaSection(SdpMediaSection& msection) const override {
    if (mEnabled && msection.GetMediaType() == mType) {
      if (mDirection == sdp::kRecv) {
        msection.AddDataChannel(mName, mLocalPort, mChannels,
                                mLocalMaxMessageSize);
      }

      AddParametersToMSection(msection);
    }
  }

  bool Negotiate(const std::string& pt, const SdpMediaSection& remoteMsection,
                 bool isOffer) override {
    JsepCodecDescription::Negotiate(pt, remoteMsection, isOffer);

    uint32_t message_size;
    mRemoteMMSSet = remoteMsection.GetMaxMessageSize(&message_size);
    if (mRemoteMMSSet) {
      mRemoteMaxMessageSize = message_size;
    } else {
      mRemoteMaxMessageSize =
          WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_REMOTE_DEFAULT;
    }

    int sctp_port = remoteMsection.GetSctpPort();
    if (sctp_port) {
      mRemotePort = sctp_port;
      return true;
    }

    const SdpSctpmapAttributeList::Sctpmap* sctp_map(
        remoteMsection.GetSctpmap());
    if (sctp_map) {
      mRemotePort = std::stoi(sctp_map->pt);
      return true;
    }

    return false;
  }

  uint16_t mLocalPort;
  uint32_t mLocalMaxMessageSize;
  uint16_t mRemotePort;
  uint32_t mRemoteMaxMessageSize;
  bool mRemoteMMSSet;
};

}  // namespace mozilla

#endif
