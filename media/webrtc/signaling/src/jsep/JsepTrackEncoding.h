/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JESPTRACKENCODING_H_
#define _JESPTRACKENCODING_H_

#include "signaling/src/jsep/JsepCodecDescription.h"
#include "signaling/src/common/EncodingConstraints.h"

#include <vector>

namespace mozilla {
// Represents a single encoding of a media track. When simulcast is used, there
// may be multiple. Each encoding may have some constraints (imposed by JS), and
// may be able to use any one of multiple codecs (JsepCodecDescription) at any
// given time.
class JsepTrackEncoding {
 public:
  JsepTrackEncoding() = default;
  JsepTrackEncoding(const JsepTrackEncoding& orig)
      : mConstraints(orig.mConstraints), mRid(orig.mRid) {
    for (const auto& codec : orig.mCodecs) {
      mCodecs.emplace_back(codec->Clone());
    }
  }

  const std::vector<UniquePtr<JsepCodecDescription>>& GetCodecs() const {
    return mCodecs;
  }

  void AddCodec(const JsepCodecDescription& codec) {
    mCodecs.emplace_back(codec.Clone());
  }

  bool HasFormat(const std::string& format) const {
    for (const auto& codec : mCodecs) {
      if (codec->mDefaultPt == format) {
        return true;
      }
    }
    return false;
  }

  EncodingConstraints mConstraints;
  std::string mRid;

 private:
  std::vector<UniquePtr<JsepCodecDescription>> mCodecs;
};
}  // namespace mozilla

#endif  // _JESPTRACKENCODING_H_
