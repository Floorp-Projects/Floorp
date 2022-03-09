/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Base64_h__
#define mozilla_Base64_h__

#include "nsString.h"

class nsIInputStream;

namespace mozilla {

[[nodiscard]] nsresult Base64EncodeInputStream(nsIInputStream* aInputStream,
                                               nsACString& aDest,
                                               uint32_t aCount,
                                               uint32_t aOffset = 0);
[[nodiscard]] nsresult Base64EncodeInputStream(nsIInputStream* aInputStream,
                                               nsAString& aDest,
                                               uint32_t aCount,
                                               uint32_t aOffset = 0);

// Encode 8-bit data of a given length and append the Base64 encoded data to
// aBase64.
[[nodiscard]] nsresult Base64EncodeAppend(const char* aBinary,
                                          uint32_t aBinaryLen,
                                          nsAString& aBase64);
[[nodiscard]] nsresult Base64EncodeAppend(const char* aBinary,
                                          uint32_t aBinaryLen,
                                          nsACString& aBase64);
[[nodiscard]] nsresult Base64EncodeAppend(const nsACString& aBinary,
                                          nsACString& aBase64);
[[nodiscard]] nsresult Base64EncodeAppend(const nsACString& aBinary,
                                          nsAString& aBase64);

[[nodiscard]] nsresult Base64Encode(const char* aBinary, uint32_t aBinaryLen,
                                    char** aBase64);
[[nodiscard]] nsresult Base64Encode(const char* aBinary, uint32_t aBinaryLen,
                                    nsACString& aBase64);
[[nodiscard]] nsresult Base64Encode(const char* aBinary, uint32_t aBinaryLen,
                                    nsAString& aBase64);
[[nodiscard]] nsresult Base64Encode(const nsACString& aBinary,
                                    nsACString& aBase64);
[[nodiscard]] nsresult Base64Encode(const nsACString& aBinary,
                                    nsAString& aBase64);

// The high bits of any characters in aBinary are dropped.
[[nodiscard]] nsresult Base64Encode(const nsAString& aBinary,
                                    nsAString& aBase64);

[[nodiscard]] nsresult Base64Decode(const char* aBase64, uint32_t aBase64Len,
                                    char** aBinary, uint32_t* aBinaryLen);
[[nodiscard]] nsresult Base64Decode(const nsACString& aBase64,
                                    nsACString& aBinary);

// The high bits of any characters in aBase64 are dropped.
[[nodiscard]] nsresult Base64Decode(const nsAString& aBase64,
                                    nsAString& aBinary);
[[nodiscard]] nsresult Base64Decode(const nsAString& aBase64,
                                    nsACString& aBinary);

enum class Base64URLEncodePaddingPolicy {
  Include,
  Omit,
};

/**
 * Converts |aBinary| to an unpadded, Base64 URL-encoded string per RFC 4648.
 * Aims to encode the data in constant time. The caller retains ownership
 * of |aBinary|.
 */
[[nodiscard]] nsresult Base64URLEncode(
    uint32_t aBinaryLen, const uint8_t* aBinary,
    Base64URLEncodePaddingPolicy aPaddingPolicy, nsACString& aBase64);

enum class Base64URLDecodePaddingPolicy {
  Require,
  Ignore,
  Reject,
};

/**
 * Decodes a Base64 URL-encoded |aBase64| into |aBinary|.
 */
[[nodiscard]] nsresult Base64URLDecode(
    const nsACString& aBase64, Base64URLDecodePaddingPolicy aPaddingPolicy,
    FallibleTArray<uint8_t>& aBinary);

}  // namespace mozilla

#endif
