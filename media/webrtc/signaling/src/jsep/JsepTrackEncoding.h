/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JESPTRACKENCODING_H_
#define _JESPTRACKENCODING_H_

#include "signaling/src/jsep/JsepCodecDescription.h"
#include "signaling/src/common/EncodingConstraints.h"
#include "signaling/src/common/PtrVector.h"

namespace mozilla {
// Represents a single encoding of a media track. When simulcast is used, there
// may be multiple. Each encoding may have some constraints (imposed by JS), and
// may be able to use any one of multiple codecs (JsepCodecDescription) at any
// given time.
class JsepTrackEncoding
{
public:
  const std::vector<JsepCodecDescription*>& GetCodecs() const
  {
    return mCodecs.values;
  }

  void AddCodec(const JsepCodecDescription& codec)
  {
    mCodecs.values.push_back(codec.Clone());
  }

  bool HasFormat(const std::string& format) const
  {
    for (const JsepCodecDescription* codec : mCodecs.values) {
      if (codec->mDefaultPt == format) {
        return true;
      }
    }
    return false;
  }

  void UpdateMaxBitrate(const SdpMediaSection& remote)
  {
    uint32_t tias = remote.GetBandwidth("TIAS");
    // select minimum of the two which is not zero
    mConstraints.maxBr = std::min(tias ? tias : mConstraints.maxBr,
                                  mConstraints.maxBr ? mConstraints.maxBr :
                                                       tias);
    // TODO add support for b=AS if TIAS is not set (bug 976521)
  }

  EncodingConstraints mConstraints;
  std::string mRid;

private:
  PtrVector<JsepCodecDescription> mCodecs;
};
}

#endif // _JESPTRACKENCODING_H_
