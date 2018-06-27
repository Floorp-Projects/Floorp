/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <string>

#include "signaling/src/sdp/RsdparsaSdpInc.h"
#include "signaling/src/sdp/RsdparsaSdpGlue.h"
namespace mozilla
{

std::string convertStringView(StringView str)
{
  if (nullptr == str.buf) {
    return std::string();
  } else {
    return std::string(str.buf, str.len);
  }
}

std::vector<std::string> convertStringVec(StringVec* vec)
{
  std::vector<std::string> ret;
  size_t len = string_vec_len(vec);
  for (size_t i = 0; i < len; i++) {
    StringView view;
    string_vec_get_view(vec, i, &view);
    ret.push_back(convertStringView(view));
  }
  return ret;
}

sdp::AddrType convertAddressType(RustSdpAddrType addrType)
{
  switch(addrType) {
    case RustSdpAddrType::kRustAddrNone:
      return sdp::kAddrTypeNone;
    case RustSdpAddrType::kRustAddrIp4:
      return sdp::kIPv4;
    case RustSdpAddrType::kRustAddrIp6:
      return sdp::kIPv6;
  }

  MOZ_CRASH("unknown address type");
}

std::vector<uint8_t> convertU8Vec(U8Vec* vec)
{
  std::vector<std::uint8_t> ret;

  size_t len = u8_vec_len(vec);
  for (size_t i = 0; i < len; i++) {
    uint8_t byte;
    u8_vec_get(vec, i, &byte);
    ret.push_back(byte);
  }

  return ret;
}

std::vector<uint16_t> convertU16Vec(U16Vec* vec)
{
  std::vector<std::uint16_t> ret;

  size_t len = u16_vec_len(vec);
  for (size_t i = 0; i < len; i++) {
    uint16_t word;
    u16_vec_get(vec, i, &word);
    ret.push_back(word);
  }

  return ret;
}

}
