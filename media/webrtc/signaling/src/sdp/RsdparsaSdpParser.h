/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RUSTSDPPARSER_H_
#define _RUSTSDPPARSER_H_

#include <string>

#include "mozilla/UniquePtr.h"

#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SdpErrorHolder.h"

namespace mozilla {

class RsdparsaSdpParser final : public SdpErrorHolder {
 public:
  RsdparsaSdpParser() {}
  virtual ~RsdparsaSdpParser() {}

  /**
   * This parses the provided text into an SDP object.
   * This returns a nullptr-valued pointer if things go poorly.
   */
  UniquePtr<Sdp> Parse(const std::string& sdpText);
};

}  // namespace mozilla

#endif
