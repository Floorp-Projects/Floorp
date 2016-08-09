/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Base64.h"

#include "mozilla/UniquePtrExtensions.h"
#include "nsIInputStream.h"
#include "nsString.h"
#include "nsTArray.h"

#include "plbase64.h"

namespace {

// BEGIN base64 encode code copied and modified from NSPR
const unsigned char* base =
  (unsigned char*)"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                  "abcdefghijklmnopqrstuvwxyz"
                  "0123456789+/";

template<typename T>
static void
Encode3to4(const unsigned char* aSrc, T* aDest)
{
  uint32_t b32 = (uint32_t)0;
  int i, j = 18;

  for (i = 0; i < 3; ++i) {
    b32 <<= 8;
    b32 |= (uint32_t)aSrc[i];
  }

  for (i = 0; i < 4; ++i) {
    aDest[i] = base[(uint32_t)((b32 >> j) & 0x3F)];
    j -= 6;
  }
}

template<typename T>
static void
Encode2to4(const unsigned char* aSrc, T* aDest)
{
  aDest[0] = base[(uint32_t)((aSrc[0] >> 2) & 0x3F)];
  aDest[1] = base[(uint32_t)(((aSrc[0] & 0x03) << 4) | ((aSrc[1] >> 4) & 0x0F))];
  aDest[2] = base[(uint32_t)((aSrc[1] & 0x0F) << 2)];
  aDest[3] = (unsigned char)'=';
}

template<typename T>
static void
Encode1to4(const unsigned char* aSrc, T* aDest)
{
  aDest[0] = base[(uint32_t)((aSrc[0] >> 2) & 0x3F)];
  aDest[1] = base[(uint32_t)((aSrc[0] & 0x03) << 4)];
  aDest[2] = (unsigned char)'=';
  aDest[3] = (unsigned char)'=';
}

template<typename T>
static void
Encode(const unsigned char* aSrc, uint32_t aSrcLen, T* aDest)
{
  while (aSrcLen >= 3) {
    Encode3to4(aSrc, aDest);
    aSrc += 3;
    aDest += 4;
    aSrcLen -= 3;
  }

  switch (aSrcLen) {
    case 2:
      Encode2to4(aSrc, aDest);
      break;
    case 1:
      Encode1to4(aSrc, aDest);
      break;
    case 0:
      break;
    default:
      NS_NOTREACHED("coding error");
  }
}

// END base64 encode code copied and modified from NSPR.

template<typename T>
struct EncodeInputStream_State
{
  unsigned char c[3];
  uint8_t charsOnStack;
  typename T::char_type* buffer;
};

template<typename T>
NS_METHOD
EncodeInputStream_Encoder(nsIInputStream* aStream,
                          void* aClosure,
                          const char* aFromSegment,
                          uint32_t aToOffset,
                          uint32_t aCount,
                          uint32_t* aWriteCount)
{
  NS_ASSERTION(aCount > 0, "Er, what?");

  EncodeInputStream_State<T>* state =
    static_cast<EncodeInputStream_State<T>*>(aClosure);

  // If we have any data left from last time, encode it now.
  uint32_t countRemaining = aCount;
  const unsigned char* src = (const unsigned char*)aFromSegment;
  if (state->charsOnStack) {
    unsigned char firstSet[4];
    if (state->charsOnStack == 1) {
      firstSet[0] = state->c[0];
      firstSet[1] = src[0];
      firstSet[2] = (countRemaining > 1) ? src[1] : '\0';
      firstSet[3] = '\0';
    } else /* state->charsOnStack == 2 */ {
      firstSet[0] = state->c[0];
      firstSet[1] = state->c[1];
      firstSet[2] = src[0];
      firstSet[3] = '\0';
    }
    Encode(firstSet, 3, state->buffer);
    state->buffer += 4;
    countRemaining -= (3 - state->charsOnStack);
    src += (3 - state->charsOnStack);
    state->charsOnStack = 0;
  }

  // Encode the bulk of the
  uint32_t encodeLength = countRemaining - countRemaining % 3;
  MOZ_ASSERT(encodeLength % 3 == 0,
             "Should have an exact number of triplets!");
  Encode(src, encodeLength, state->buffer);
  state->buffer += (encodeLength / 3) * 4;
  src += encodeLength;
  countRemaining -= encodeLength;

  // We must consume all data, so if there's some data left stash it
  *aWriteCount = aCount;

  if (countRemaining) {
    // We should never have a full triplet left at this point.
    MOZ_ASSERT(countRemaining < 3, "We should have encoded more!");
    state->c[0] = src[0];
    state->c[1] = (countRemaining == 2) ? src[1] : '\0';
    state->charsOnStack = countRemaining;
  }

  return NS_OK;
}

template<typename T>
nsresult
EncodeInputStream(nsIInputStream* aInputStream,
                  T& aDest,
                  uint32_t aCount,
                  uint32_t aOffset)
{
  nsresult rv;
  uint64_t count64 = aCount;

  if (!aCount) {
    rv = aInputStream->Available(&count64);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // if count64 is over 4GB, it will be failed at the below condition,
    // then will return NS_ERROR_OUT_OF_MEMORY
    aCount = (uint32_t)count64;
  }

  uint64_t countlong =
    (count64 + 2) / 3 * 4; // +2 due to integer math.
  if (countlong + aOffset > UINT32_MAX) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t count = uint32_t(countlong);

  if (!aDest.SetLength(count + aOffset, mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  EncodeInputStream_State<T> state;
  state.charsOnStack = 0;
  state.c[2] = '\0';
  state.buffer = aOffset + aDest.BeginWriting();

  while (1) {
    uint32_t read = 0;

    rv = aInputStream->ReadSegments(&EncodeInputStream_Encoder<T>,
                                    (void*)&state,
                                    aCount,
                                    &read);
    if (NS_FAILED(rv)) {
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        NS_RUNTIMEABORT("Not implemented for async streams!");
      }
      if (rv == NS_ERROR_NOT_IMPLEMENTED) {
        NS_RUNTIMEABORT("Requires a stream that implements ReadSegments!");
      }
      return rv;
    }

    if (!read) {
      break;
    }
  }

  // Finish encoding if anything is left
  if (state.charsOnStack) {
    Encode(state.c, state.charsOnStack, state.buffer);
  }

  if (aDest.Length()) {
    // May belong to an nsCString with an unallocated buffer, so only null
    // terminate if there is a need to.
    *aDest.EndWriting() = '\0';
  }

  return NS_OK;
}

static const char kBase64URLAlphabet[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// Maps an encoded character to a value in the Base64 URL alphabet, per
// RFC 4648, Table 2. Invalid input characters map to UINT8_MAX.
static const uint8_t kBase64URLDecodeTable[] = {
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255,
  62 /* - */,
  255, 255,
  52, 53, 54, 55, 56, 57, 58, 59, 60, 61, /* 0 - 9 */
  255, 255, 255, 255, 255, 255, 255,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, /* A - Z */
  255, 255, 255, 255,
  63 /* _ */,
  255,
  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
  42, 43, 44, 45, 46, 47, 48, 49, 50, 51, /* a - z */
  255, 255, 255, 255,
};

bool
Base64URLCharToValue(char aChar, uint8_t* aValue) {
  uint8_t index = static_cast<uint8_t>(aChar);
  *aValue = kBase64URLDecodeTable[index & 0x7f];
  return (*aValue != 255) && !(index & ~0x7f);
}

} // namespace

namespace mozilla {

nsresult
Base64EncodeInputStream(nsIInputStream* aInputStream,
                        nsACString& aDest,
                        uint32_t aCount,
                        uint32_t aOffset)
{
  return EncodeInputStream<nsACString>(aInputStream, aDest, aCount, aOffset);
}

nsresult
Base64EncodeInputStream(nsIInputStream* aInputStream,
                        nsAString& aDest,
                        uint32_t aCount,
                        uint32_t aOffset)
{
  return EncodeInputStream<nsAString>(aInputStream, aDest, aCount, aOffset);
}

nsresult
Base64Encode(const char* aBinary, uint32_t aBinaryLen, char** aBase64)
{
  // Check for overflow.
  if (aBinaryLen > (UINT32_MAX / 4) * 3) {
    return NS_ERROR_FAILURE;
  }

  // Don't ask PR_Base64Encode to encode empty strings.
  if (aBinaryLen == 0) {
    *aBase64 = (char*)moz_xmalloc(1);
    (*aBase64)[0] = '\0';
    return NS_OK;
  }

  *aBase64 = nullptr;
  uint32_t base64Len = ((aBinaryLen + 2) / 3) * 4;

  // Add one byte for null termination.
  UniqueFreePtr<char[]> base64((char*)malloc(base64Len + 1));
  if (!base64) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!PL_Base64Encode(aBinary, aBinaryLen, base64.get())) {
    return NS_ERROR_INVALID_ARG;
  }

  // PL_Base64Encode doesn't null terminate the buffer for us when we pass
  // the buffer in. Do that manually.
  base64[base64Len] = '\0';

  *aBase64 = base64.release();
  return NS_OK;
}

nsresult
Base64Encode(const nsACString& aBinary, nsACString& aBase64)
{
  // Check for overflow.
  if (aBinary.Length() > (UINT32_MAX / 4) * 3) {
    return NS_ERROR_FAILURE;
  }

  // Don't ask PR_Base64Encode to encode empty strings.
  if (aBinary.IsEmpty()) {
    aBase64.Truncate();
    return NS_OK;
  }

  uint32_t base64Len = ((aBinary.Length() + 2) / 3) * 4;

  // Add one byte for null termination.
  if (!aBase64.SetCapacity(base64Len + 1, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char* base64 = aBase64.BeginWriting();
  if (!PL_Base64Encode(aBinary.BeginReading(), aBinary.Length(), base64)) {
    aBase64.Truncate();
    return NS_ERROR_INVALID_ARG;
  }

  // PL_Base64Encode doesn't null terminate the buffer for us when we pass
  // the buffer in. Do that manually.
  base64[base64Len] = '\0';

  aBase64.SetLength(base64Len);
  return NS_OK;
}

nsresult
Base64Encode(const nsAString& aBinary, nsAString& aBase64)
{
  NS_LossyConvertUTF16toASCII binary(aBinary);
  nsAutoCString base64;

  nsresult rv = Base64Encode(binary, base64);
  if (NS_SUCCEEDED(rv)) {
    CopyASCIItoUTF16(base64, aBase64);
  } else {
    aBase64.Truncate();
  }

  return rv;
}

static nsresult
Base64DecodeHelper(const char* aBase64, uint32_t aBase64Len, char* aBinary,
                   uint32_t* aBinaryLen)
{
  if (!PL_Base64Decode(aBase64, aBase64Len, aBinary)) {
    return NS_ERROR_INVALID_ARG;
  }

  // PL_Base64Decode doesn't null terminate the buffer for us when we pass
  // the buffer in. Do that manually, taking into account the number of '='
  // characters we were passed.
  if (aBase64Len != 0 && aBase64[aBase64Len - 1] == '=') {
    if (aBase64Len > 1 && aBase64[aBase64Len - 2] == '=') {
      *aBinaryLen -= 2;
    } else {
      *aBinaryLen -= 1;
    }
  }
  aBinary[*aBinaryLen] = '\0';
  return NS_OK;
}

nsresult
Base64Decode(const char* aBase64, uint32_t aBase64Len, char** aBinary,
             uint32_t* aBinaryLen)
{
  // Check for overflow.
  if (aBase64Len > UINT32_MAX / 3) {
    return NS_ERROR_FAILURE;
  }

  // Don't ask PR_Base64Decode to decode the empty string.
  if (aBase64Len == 0) {
    *aBinary = (char*)moz_xmalloc(1);
    (*aBinary)[0] = '\0';
    *aBinaryLen = 0;
    return NS_OK;
  }

  *aBinary = nullptr;
  *aBinaryLen = (aBase64Len * 3) / 4;

  // Add one byte for null termination.
  UniqueFreePtr<char[]> binary((char*)malloc(*aBinaryLen + 1));
  if (!binary) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv =
    Base64DecodeHelper(aBase64, aBase64Len, binary.get(), aBinaryLen);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aBinary = binary.release();
  return NS_OK;
}

nsresult
Base64Decode(const nsACString& aBase64, nsACString& aBinary)
{
  // Check for overflow.
  if (aBase64.Length() > UINT32_MAX / 3) {
    return NS_ERROR_FAILURE;
  }

  // Don't ask PR_Base64Decode to decode the empty string
  if (aBase64.IsEmpty()) {
    aBinary.Truncate();
    return NS_OK;
  }

  uint32_t binaryLen = ((aBase64.Length() * 3) / 4);

  // Add one byte for null termination.
  if (!aBinary.SetCapacity(binaryLen + 1, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char* binary = aBinary.BeginWriting();
  nsresult rv = Base64DecodeHelper(aBase64.BeginReading(), aBase64.Length(),
                                   binary, &binaryLen);
  if (NS_FAILED(rv)) {
    aBinary.Truncate();
    return rv;
  }

  aBinary.SetLength(binaryLen);
  return NS_OK;
}

nsresult
Base64Decode(const nsAString& aBase64, nsAString& aBinary)
{
  NS_LossyConvertUTF16toASCII base64(aBase64);
  nsAutoCString binary;

  nsresult rv = Base64Decode(base64, binary);
  if (NS_SUCCEEDED(rv)) {
    CopyASCIItoUTF16(binary, aBinary);
  } else {
    aBinary.Truncate();
  }

  return rv;
}

nsresult
Base64URLDecode(const nsACString& aBase64,
                Base64URLDecodePaddingPolicy aPaddingPolicy,
                FallibleTArray<uint8_t>& aBinary)
{
  // Don't decode empty strings.
  if (aBase64.IsEmpty()) {
    aBinary.Clear();
    return NS_OK;
  }

  // Check for overflow.
  uint32_t base64Len = aBase64.Length();
  if (base64Len > UINT32_MAX / 3) {
    return NS_ERROR_FAILURE;
  }
  const char* base64 = aBase64.BeginReading();

  // The decoded length may be 1-2 bytes over, depending on the final quantum.
  uint32_t binaryLen = (base64Len * 3) / 4;

  // Determine whether to check for and ignore trailing padding.
  bool maybePadded = false;
  switch (aPaddingPolicy) {
    case Base64URLDecodePaddingPolicy::Require:
      if (base64Len % 4) {
        // Padded input length must be a multiple of 4.
        return NS_ERROR_INVALID_ARG;
      }
      maybePadded = true;
      break;

    case Base64URLDecodePaddingPolicy::Ignore:
      // Check for padding only if the length is a multiple of 4.
      maybePadded = !(base64Len % 4);
      break;

    // If we're expecting unpadded input, no need for additional checks.
    // `=` isn't in the decode table, so padded strings will fail to decode.
    default:
      MOZ_FALLTHROUGH_ASSERT("Invalid decode padding policy");
    case Base64URLDecodePaddingPolicy::Reject:
      break;
  }
  if (maybePadded && base64[base64Len - 1] == '=') {
    if (base64[base64Len - 2] == '=') {
      base64Len -= 2;
    } else {
      base64Len -= 1;
    }
  }

  if (NS_WARN_IF(!aBinary.SetCapacity(binaryLen, mozilla::fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aBinary.SetLengthAndRetainStorage(binaryLen);
  uint8_t* binary = aBinary.Elements();

  for (; base64Len >= 4; base64Len -= 4) {
    uint8_t w, x, y, z;
    if (!Base64URLCharToValue(*base64++, &w) ||
        !Base64URLCharToValue(*base64++, &x) ||
        !Base64URLCharToValue(*base64++, &y) ||
        !Base64URLCharToValue(*base64++, &z)) {
      return NS_ERROR_INVALID_ARG;
    }
    *binary++ = w << 2 | x >> 4;
    *binary++ = x << 4 | y >> 2;
    *binary++ = y << 6 | z;
  }

  if (base64Len == 3) {
    uint8_t w, x, y;
    if (!Base64URLCharToValue(*base64++, &w) ||
        !Base64URLCharToValue(*base64++, &x) ||
        !Base64URLCharToValue(*base64++, &y)) {
      return NS_ERROR_INVALID_ARG;
    }
    *binary++ = w << 2 | x >> 4;
    *binary++ = x << 4 | y >> 2;
  } else if (base64Len == 2) {
    uint8_t w, x;
    if (!Base64URLCharToValue(*base64++, &w) ||
        !Base64URLCharToValue(*base64++, &x)) {
      return NS_ERROR_INVALID_ARG;
    }
    *binary++ = w << 2 | x >> 4;
  } else if (base64Len) {
    return NS_ERROR_INVALID_ARG;
  }

  // Set the length to the actual number of decoded bytes.
  aBinary.TruncateLength(binary - aBinary.Elements());
  return NS_OK;
}

nsresult
Base64URLEncode(uint32_t aBinaryLen, const uint8_t* aBinary,
                Base64URLEncodePaddingPolicy aPaddingPolicy,
                nsACString& aBase64)
{
  // Don't encode empty strings.
  if (aBinaryLen == 0) {
    aBase64.Truncate();
    return NS_OK;
  }

  // Check for overflow.
  if (aBinaryLen > (UINT32_MAX / 4) * 3) {
    return NS_ERROR_FAILURE;
  }

  // Allocate a buffer large enough to hold the encoded string with padding.
  // Add one byte for null termination.
  uint32_t base64Len = ((aBinaryLen + 2) / 3) * 4;
  if (NS_WARN_IF(!aBase64.SetCapacity(base64Len + 1, fallible))) {
    aBase64.Truncate();
    return NS_ERROR_FAILURE;
  }

  char* base64 = aBase64.BeginWriting();

  uint32_t index = 0;
  for (; index + 3 <= aBinaryLen; index += 3) {
    *base64++ = kBase64URLAlphabet[aBinary[index] >> 2];
    *base64++ = kBase64URLAlphabet[((aBinary[index] & 0x3) << 4) |
                                   (aBinary[index + 1] >> 4)];
    *base64++ = kBase64URLAlphabet[((aBinary[index + 1] & 0xf) << 2) |
                                   (aBinary[index + 2] >> 6)];
    *base64++ = kBase64URLAlphabet[aBinary[index + 2] & 0x3f];
  }

  uint32_t remaining = aBinaryLen - index;
  if (remaining == 1) {
    *base64++ = kBase64URLAlphabet[aBinary[index] >> 2];
    *base64++ = kBase64URLAlphabet[((aBinary[index] & 0x3) << 4)];
  } else if (remaining == 2) {
    *base64++ = kBase64URLAlphabet[aBinary[index] >> 2];
    *base64++ = kBase64URLAlphabet[((aBinary[index] & 0x3) << 4) |
                                   (aBinary[index + 1] >> 4)];
    *base64++ = kBase64URLAlphabet[((aBinary[index + 1] & 0xf) << 2)];
  }

  uint32_t length = base64 - aBase64.BeginWriting();
  if (aPaddingPolicy == Base64URLEncodePaddingPolicy::Include) {
    if (length % 4 == 2) {
      *base64++ = '=';
      *base64++ = '=';
      length += 2;
    } else if (length % 4 == 3) {
      *base64++ = '=';
      length += 1;
    }
  } else {
    MOZ_ASSERT(aPaddingPolicy == Base64URLEncodePaddingPolicy::Omit,
               "Invalid encode padding policy");
  }

  // Null terminate and truncate to the actual number of characters.
  *base64 = '\0';
  aBase64.SetLength(length);

  return NS_OK;
}

} // namespace mozilla
