/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPTRACK_H_
#define _JSEPTRACK_H_

#include <string>
#include <map>
#include <set>

#include <mozilla/RefPtr.h>
#include <mozilla/UniquePtr.h>
#include <mozilla/Maybe.h>
#include "nsISupportsImpl.h"
#include "nsError.h"

#include "signaling/src/jsep/JsepTransport.h"
#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SdpAttribute.h"
#include "signaling/src/sdp/SdpMediaSection.h"
#include "signaling/src/common/PtrVector.h"

namespace mozilla {

// Forward reference.
class JsepCodecDescription;

class JsepTrackNegotiatedDetails
{
public:
  size_t
  GetCodecCount() const
  {
    return mCodecs.values.size();
  }

  const JsepCodecDescription*
  GetCodec(size_t index) const
  {
    MOZ_RELEASE_ASSERT(index < mCodecs.values.size());
    return mCodecs.values[index];
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

  std::vector<uint8_t> GetUniquePayloadTypes() const
  {
    return mUniquePayloadTypes;
  }

private:
  friend class JsepTrack;

  std::map<std::string, SdpExtmapAttributeList::Extmap> mExtmap;
  std::vector<uint8_t> mUniquePayloadTypes;
  PtrVector<JsepCodecDescription> mCodecs;
};

class JsepTrack
{
public:
  JsepTrack(mozilla::SdpMediaSection::MediaType type,
            const std::string& streamid,
            const std::string& trackid,
            sdp::Direction direction = sdp::kSend)
      : mType(type),
        mStreamId(streamid),
        mTrackId(trackid),
        mDirection(direction)
  {}

  virtual mozilla::SdpMediaSection::MediaType
  GetMediaType() const
  {
    return mType;
  }

  virtual const std::string&
  GetStreamId() const
  {
    return mStreamId;
  }

  virtual void
  SetStreamId(const std::string& id)
  {
    mStreamId = id;
  }

  virtual const std::string&
  GetTrackId() const
  {
    return mTrackId;
  }

  virtual void
  SetTrackId(const std::string& id)
  {
    mTrackId = id;
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

  virtual void
  AddSsrc(uint32_t ssrc)
  {
    mSsrcs.push_back(ssrc);
  }

  virtual void PopulateCodecs(
      const std::vector<JsepCodecDescription*>& prototype);
  virtual void AddToOffer(SdpMediaSection* offer) const;
  virtual void AddToAnswer(const SdpMediaSection& offer,
                           SdpMediaSection* answer) const;
  virtual void Negotiate(const SdpMediaSection& answer,
                         const SdpMediaSection& remote);
  static void SetUniquePayloadTypes(
      const std::vector<RefPtr<JsepTrack>>& tracks);
  virtual void GetNegotiatedPayloadTypes(std::vector<uint16_t>* payloadTypes);

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

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(JsepTrack);

protected:
  virtual ~JsepTrack() {}

private:
  virtual std::vector<JsepCodecDescription*> GetCodecClones() const;
  static void EnsureNoDuplicatePayloadTypes(
      std::vector<JsepCodecDescription*>* codecs);
  static void GetPayloadTypes(
      const std::vector<JsepCodecDescription*>& codecs,
      std::vector<uint16_t>* pts);
  static void EnsurePayloadTypeIsUnique(std::set<uint16_t>* uniquePayloadTypes,
                                        JsepCodecDescription* codec);
  virtual void AddToMsection(const std::vector<JsepCodecDescription*>& codecs,
                             SdpMediaSection* msection) const;

  // |answer| is set when performing the final negotiation on completion of
  // offer/answer, and is used to update the formats in |codecs|, since the
  // answer is authoritative. |formatChanges| is also set on completion of
  // offer/answer, and records how the formats in |codecs| were changed, which
  // is used by |Negotiate| to update |mPrototypeCodecs|.
  virtual void NegotiateCodecs(
      const SdpMediaSection& remote,
      std::vector<JsepCodecDescription*>* codecs,
      const SdpMediaSection* answer = nullptr,
      std::map<std::string, std::string>* formatChanges = nullptr) const;

  const mozilla::SdpMediaSection::MediaType mType;
  std::string mStreamId;
  std::string mTrackId;
  std::string mCNAME;
  const sdp::Direction mDirection;
  PtrVector<JsepCodecDescription> mPrototypeCodecs;
  UniquePtr<JsepTrackNegotiatedDetails> mNegotiatedDetails;
  std::vector<uint32_t> mSsrcs;
};

// Need a better name for this.
struct JsepTrackPair {
  size_t mLevel;
  // Is this track pair sharing a transport with another?
  Maybe<size_t> mBundleLevel;
  uint32_t mRecvonlySsrc;
  RefPtr<JsepTrack> mSending;
  RefPtr<JsepTrack> mReceiving;
  RefPtr<JsepTransport> mRtpTransport;
  RefPtr<JsepTransport> mRtcpTransport;
};

} // namespace mozilla

#endif
