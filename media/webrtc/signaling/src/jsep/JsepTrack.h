/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPTRACK_H_
#define _JSEPTRACK_H_

#include <string>

#include <mozilla/RefPtr.h>
#include <mozilla/UniquePtr.h>
#include <mozilla/Maybe.h>
#include "nsISupportsImpl.h"
#include "nsError.h"

#include "signaling/src/jsep/JsepTransport.h"
#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SdpMediaSection.h"

namespace mozilla {

// Forward reference.
struct JsepCodecDescription;

class JsepTrackNegotiatedDetails
{
public:
  virtual ~JsepTrackNegotiatedDetails() {}

  virtual mozilla::SdpMediaSection::Protocol GetProtocol() const = 0;
  virtual Maybe<std::string> GetBandwidth(const std::string& type) const = 0;
  virtual size_t GetCodecCount() const = 0;
  virtual nsresult GetCodec(size_t index,
                            const JsepCodecDescription** config) const = 0;
  virtual const SdpExtmapAttributeList::Extmap* GetExt(
      const std::string& ext_name) const = 0;
  virtual std::vector<uint8_t> GetUniquePayloadTypes() const = 0;

  virtual void AddUniquePayloadType(uint8_t pt) = 0;
  virtual void ClearUniquePayloadTypes() = 0;
};

class JsepTrack
{
public:
  enum Direction { kJsepTrackSending, kJsepTrackReceiving };

  JsepTrack(mozilla::SdpMediaSection::MediaType type,
            const std::string& streamid,
            const std::string& trackid,
            Direction direction = kJsepTrackSending)
      : mType(type),
        mStreamId(streamid),
        mTrackId(trackid),
        mDirection(direction)
  {
  }

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

  virtual Direction
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

  // This is for JsepSession's use.
  virtual void
  SetNegotiatedDetails(UniquePtr<JsepTrackNegotiatedDetails> details)
  {
    mNegotiatedDetails = Move(details);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(JsepTrack);

protected:
  virtual ~JsepTrack() {}

private:
  const mozilla::SdpMediaSection::MediaType mType;
  std::string mStreamId;
  std::string mTrackId;
  std::string mCNAME;
  const Direction mDirection;
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
