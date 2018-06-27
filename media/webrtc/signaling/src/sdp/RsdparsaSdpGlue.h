/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _RUSTSDPGLUE_H_
#define _RUSTSDPGLUE_H_

#include <string>
#include <vector>

#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/RsdparsaSdpInc.h"

namespace mozilla
{

struct FreeRustSdpSession {
  void operator()(RustSdpSession* aSess) { sdp_free_session(aSess); }
};

typedef UniquePtr<RustSdpSession, FreeRustSdpSession> RsdparsaSessionHandle;

std::string convertStringView(StringView str);
std::vector<std::string> convertStringVec(StringVec* vec);
sdp::AddrType convertAddressType(RustSdpAddrType addr);
std::vector<uint8_t> convertU8Vec(U8Vec* vec);
std::vector<uint16_t> convertU16Vec(U16Vec* vec);

}


#endif
