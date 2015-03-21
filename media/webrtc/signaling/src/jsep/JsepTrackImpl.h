/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPTRACKIMPL_H_
#define _JSEPTRACKIMPL_H_

#include <map>

#include <mozilla/RefPtr.h>
#include <mozilla/UniquePtr.h>

#include "signaling/src/jsep/JsepCodecDescription.h"
#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SdpMediaSection.h"

namespace mozilla {

class JsepTrackNegotiatedDetailsImpl : public JsepTrackNegotiatedDetails
{
public:
  virtual ~JsepTrackNegotiatedDetailsImpl()
  {
    for (auto c = mCodecs.begin(); c != mCodecs.end(); ++c) {
      delete *c;
    }
  }

  // Implement JsepTrackNegotiatedDetails.
  virtual mozilla::SdpMediaSection::Protocol
  GetProtocol() const override
  {
    return mProtocol;
  }
  virtual Maybe<std::string>
  GetBandwidth(const std::string& type) const override
  {
    return mBandwidth;
  }
  virtual size_t
  GetCodecCount() const override
  {
    return mCodecs.size();
  }
  virtual nsresult
  GetCodec(size_t index, const JsepCodecDescription** config) const override
  {
    if (index >= mCodecs.size()) {
      return NS_ERROR_INVALID_ARG;
    }
    *config = mCodecs[index];
    return NS_OK;
  }

  virtual const SdpExtmapAttributeList::Extmap*
  GetExt(const std::string& ext_name) const override
  {
    auto it = mExtmap.find(ext_name);
    if (it != mExtmap.end()) {
      return &it->second;
    }
    return nullptr;
  }

  virtual std::vector<uint8_t> GetUniquePayloadTypes() const override
  {
    return mUniquePayloadTypes;
  }

  virtual void AddUniquePayloadType(uint8_t pt) override
  {
    mUniquePayloadTypes.push_back(pt);
  }

  virtual void ClearUniquePayloadTypes() override
  {
    mUniquePayloadTypes.clear();
  }

private:
  // Make these friends to JsepSessionImpl to avoid having to
  // write setters.
  friend class JsepSessionImpl;

  mozilla::SdpMediaSection::Protocol mProtocol;
  Maybe<std::string> mBandwidth;
  std::vector<JsepCodecDescription*> mCodecs;
  std::map<std::string, SdpExtmapAttributeList::Extmap> mExtmap;
  std::vector<uint8_t> mUniquePayloadTypes;
};

} // namespace mozilla

#endif
