/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPTRACK_H_
#define _JSEPTRACK_H_

#include <functional>
#include <algorithm>
#include <string>
#include <map>
#include <set>

#include <mozilla/UniquePtr.h>
#include "nsError.h"

#include "signaling/src/jsep/JsepTrackEncoding.h"
#include "signaling/src/jsep/SsrcGenerator.h"
#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SdpAttribute.h"
#include "signaling/src/sdp/SdpMediaSection.h"
#include "signaling/src/common/PtrVector.h"

namespace mozilla {

class JsepTrackNegotiatedDetails
{
public:
  JsepTrackNegotiatedDetails() :
    mTias(0)
  {}

  JsepTrackNegotiatedDetails(const JsepTrackNegotiatedDetails& orig) :
    mExtmap(orig.mExtmap),
    mUniquePayloadTypes(orig.mUniquePayloadTypes),
    mTias(orig.mTias)
  {
    for (const JsepTrackEncoding* encoding : orig.mEncodings.values) {
      mEncodings.values.push_back(new JsepTrackEncoding(*encoding));
    }
  }

  size_t
  GetEncodingCount() const
  {
    return mEncodings.values.size();
  }

  const JsepTrackEncoding&
  GetEncoding(size_t index) const
  {
    MOZ_RELEASE_ASSERT(index < mEncodings.values.size());
    return *mEncodings.values[index];
  }

  const SdpExtmapAttributeList::Extmap*
  GetExt(const std::string& ext_name) const
  {
    auto it = mExtmap.find(ext_name);
    if (it != mExtmap.end()) {
      return &it->second;
    }
    return nullptr;
  }

  void
  ForEachRTPHeaderExtension(
    const std::function<void(const SdpExtmapAttributeList::Extmap& extmap)> & fn) const
  {
    for(auto entry: mExtmap) {
      fn(entry.second);
    }
  }

  std::vector<uint8_t> GetUniquePayloadTypes() const
  {
    return mUniquePayloadTypes;
  }

  uint32_t GetTias() const
  {
    return mTias;
  }

private:
  friend class JsepTrack;

  std::map<std::string, SdpExtmapAttributeList::Extmap> mExtmap;
  std::vector<uint8_t> mUniquePayloadTypes;
  PtrVector<JsepTrackEncoding> mEncodings;
  uint32_t mTias; // bits per second
};

class JsepTrack
{
public:
  JsepTrack(mozilla::SdpMediaSection::MediaType type,
            sdp::Direction direction)
      : mType(type),
        mDirection(direction),
        mActive(false),
        mRemoteSetSendBit(false)
  {
  }

  virtual ~JsepTrack() {}

  void UpdateTrackIds(const std::vector<std::string>& streamIds,
                      const std::string& trackId)
  {
    mStreamIds = streamIds;
    mTrackId = trackId;
  }

  void ClearTrackIds()
  {
    mStreamIds.clear();
    mTrackId.clear();
  }

  void UpdateRecvTrack(const Sdp& sdp, const SdpMediaSection& msection)
  {
    MOZ_ASSERT(mDirection == sdp::kRecv);
    MOZ_ASSERT(
        msection.GetMediaType() != SdpMediaSection::MediaType::kApplication);
    std::string error;
    SdpHelper helper(&error);

    mRemoteSetSendBit = msection.IsSending();

    if (msection.IsSending()) {
      (void)helper.GetIdsFromMsid(sdp, msection, &mStreamIds, &mTrackId);
    } else {
      mStreamIds.clear();
    }

    // We do this whether or not the track is active
    SetCNAME(helper.GetCNAME(msection));
    mSsrcs.clear();
    if (msection.GetAttributeList().HasAttribute(
          SdpAttribute::kSsrcAttribute)) {
      for (auto& ssrcAttr : msection.GetAttributeList().GetSsrc().mSsrcs) {
        mSsrcs.push_back(ssrcAttr.ssrc);
      }
    }
  }

  JsepTrack(const JsepTrack& orig)
  {
    *this = orig;
  }

  JsepTrack(JsepTrack&& orig) = default;
  JsepTrack& operator=(JsepTrack&& rhs) = default;

  JsepTrack& operator=(const JsepTrack& rhs)
  {
    if (this != &rhs) {
      mType = rhs.mType;
      mStreamIds = rhs.mStreamIds;
      mTrackId = rhs.mTrackId;
      mCNAME = rhs.mCNAME;
      mDirection = rhs.mDirection;
      mJsEncodeConstraints = rhs.mJsEncodeConstraints;
      mSsrcs = rhs.mSsrcs;
      mActive = rhs.mActive;
      mRemoteSetSendBit = rhs.mRemoteSetSendBit;

      for (const JsepCodecDescription* codec : rhs.mPrototypeCodecs.values) {
        mPrototypeCodecs.values.push_back(codec->Clone());
      }
      if (rhs.mNegotiatedDetails) {
        mNegotiatedDetails.reset(
          new JsepTrackNegotiatedDetails(*rhs.mNegotiatedDetails));
      }
    }
    return *this;
  }

  virtual mozilla::SdpMediaSection::MediaType
  GetMediaType() const
  {
    return mType;
  }

  virtual const std::vector<std::string>&
  GetStreamIds() const
  {
    return mStreamIds;
  }

  virtual const std::string&
  GetTrackId() const
  {
    return mTrackId;
  }

  virtual const std::string&
  GetCNAME() const
  {
    return mCNAME;
  }

  virtual void
  SetCNAME(const std::string& cname)
  {
    mCNAME = cname;
  }

  virtual sdp::Direction
  GetDirection() const
  {
    return mDirection;
  }

  virtual const std::vector<uint32_t>&
  GetSsrcs() const
  {
    return mSsrcs;
  }

  virtual void EnsureSsrcs(SsrcGenerator& ssrcGenerator);

  bool
  GetActive() const
  {
    return mActive;
  }

  void
  SetActive(bool active)
  {
    mActive = active;
  }

  bool
  GetRemoteSetSendBit() const
  {
    return mRemoteSetSendBit;
  }

  virtual void PopulateCodecs(
      const std::vector<JsepCodecDescription*>& prototype);

  template <class UnaryFunction>
  void ForEachCodec(UnaryFunction func)
  {
    std::for_each(mPrototypeCodecs.values.begin(),
                  mPrototypeCodecs.values.end(), func);
  }

  template <class BinaryPredicate>
  void SortCodecs(BinaryPredicate sorter)
  {
    std::stable_sort(mPrototypeCodecs.values.begin(),
                     mPrototypeCodecs.values.end(), sorter);
  }

  // These two are non-const because this is where ssrcs are chosen.
  virtual void AddToOffer(SsrcGenerator& ssrcGenerator,
                          SdpMediaSection* offer);
  virtual void AddToAnswer(const SdpMediaSection& offer,
                           SsrcGenerator& ssrcGenerator,
                           SdpMediaSection* answer);

  virtual void Negotiate(const SdpMediaSection& answer,
                         const SdpMediaSection& remote);
  static void SetUniquePayloadTypes(std::vector<JsepTrack*>& tracks);
  virtual void GetNegotiatedPayloadTypes(
      std::vector<uint16_t>* payloadTypes) const;

  // This will be set when negotiation is carried out.
  virtual const JsepTrackNegotiatedDetails*
  GetNegotiatedDetails() const
  {
    if (mNegotiatedDetails) {
      return mNegotiatedDetails.get();
    }
    return nullptr;
  }

  virtual JsepTrackNegotiatedDetails*
  GetNegotiatedDetails()
  {
    if (mNegotiatedDetails) {
      return mNegotiatedDetails.get();
    }
    return nullptr;
  }

  virtual void
  ClearNegotiatedDetails()
  {
    mNegotiatedDetails.reset();
  }

  struct JsConstraints
  {
    std::string rid;
    EncodingConstraints constraints;
  };

  void SetJsConstraints(const std::vector<JsConstraints>& constraintsList);

  void GetJsConstraints(std::vector<JsConstraints>* outConstraintsList) const
  {
    MOZ_ASSERT(outConstraintsList);
    *outConstraintsList = mJsEncodeConstraints;
  }

  void AddToMsection(const std::vector<JsConstraints>& constraintsList,
                     sdp::Direction direction,
                     SsrcGenerator& ssrcGenerator,
                     SdpMediaSection* msection);


private:
  std::vector<JsepCodecDescription*> GetCodecClones() const;
  static void EnsureNoDuplicatePayloadTypes(
      std::vector<JsepCodecDescription*>* codecs);
  static void GetPayloadTypes(
      const std::vector<JsepCodecDescription*>& codecs,
      std::vector<uint16_t>* pts);
  static void EnsurePayloadTypeIsUnique(std::set<uint16_t>* uniquePayloadTypes,
                                        JsepCodecDescription* codec);
  void AddToMsection(const std::vector<JsepCodecDescription*>& codecs,
                     SdpMediaSection* msection);
  void GetRids(const SdpMediaSection& msection,
               sdp::Direction direction,
               std::vector<SdpRidAttributeList::Rid>* rids) const;
  void CreateEncodings(
      const SdpMediaSection& remote,
      const std::vector<JsepCodecDescription*>& negotiatedCodecs,
      JsepTrackNegotiatedDetails* details);

  // |formatChanges| is set on completion of offer/answer, and records how the
  // formats in |codecs| were changed, which is used by |Negotiate| to update
  // |mPrototypeCodecs|.
  virtual void NegotiateCodecs(
      const SdpMediaSection& remote,
      std::vector<JsepCodecDescription*>* codecs,
      std::map<std::string, std::string>* formatChanges = nullptr) const;

  JsConstraints* FindConstraints(
      const std::string& rid,
      std::vector<JsConstraints>& constraintsList) const;
  void NegotiateRids(const std::vector<SdpRidAttributeList::Rid>& rids,
                     std::vector<JsConstraints>* constraints) const;
  void UpdateSsrcs(SsrcGenerator& ssrcGenerator, size_t encodings);

  mozilla::SdpMediaSection::MediaType mType;
  // These are the ids that everyone outside of JsepSession care about
  std::vector<std::string> mStreamIds;
  std::string mTrackId;
  std::string mCNAME;
  sdp::Direction mDirection;
  PtrVector<JsepCodecDescription> mPrototypeCodecs;
  // Holds encoding params/constraints from JS. Simulcast happens when there are
  // multiple of these. If there are none, we assume unconstrained unicast with
  // no rid.
  std::vector<JsConstraints> mJsEncodeConstraints;
  UniquePtr<JsepTrackNegotiatedDetails> mNegotiatedDetails;
  std::vector<uint32_t> mSsrcs;
  bool mActive;
  bool mRemoteSetSendBit;
};

} // namespace mozilla

#endif
