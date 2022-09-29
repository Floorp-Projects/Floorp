/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Base64.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsIInputStream.h"
#include "nsString.h"
#include "nsTArray.h"

#include "plbase64.h"

namespace {

// BEGIN base64 encode code copied and modified from NSPR
const unsigned char* const base =
  (unsigned char*)"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                  "abcdefghijklmnopqrstuvwxyz"
                  "0123456789+/";

// The Base64 encoder assumes all characters are less than 256; for 16-bit
// strings, that means assuming that all characters are within range, and
// masking off high bits if necessary.
template <typename T>
uint8_t CharTo8Bit(T aChar) {
  return uint8_t(aChar);
}

template <typename SrcT, typename DestT>
static void Encode3to4(const SrcT* aSrc, DestT* aDest) {
  uint32_t b32 = (uint32_t)0;
  int i, j = 18;

  for (i = 0; i < 3; ++i) {
    b32 <<= 8;
    b32 |= CharTo8Bit(aSrc[i]);
  }

  for (i = 0; i < 4; ++i) {
    aDest[i] = base[(uint32_t)((b32 >> j) & 0x3F)];
    j -= 6;
  }
}

template <typename SrcT, typename DestT>
static void Encode2to4(const SrcT* aSrc, DestT* aDest) {
  uint8_t src0 = CharTo8Bit(aSrc[0]);
  uint8_t src1 = CharTo8Bit(aSrc[1]);
  aDest[0] = base[(uint32_t)((src0 >> 2) & 0x3F)];
  aDest[1] = base[(uint32_t)(((src0 & 0x03) << 4) | ((src1 >> 4) & 0x0F))];
  aDest[2] = base[(uint32_t)((src1 & 0x0F) << 2)];
  aDest[3] = DestT('=');
}

template <typename SrcT, typename DestT>
static void Encode1to4(const SrcT* aSrc, DestT* aDest) {
  uint8_t src0 = CharTo8Bit(aSrc[0]);
  aDest[0] = base[(uint32_t)((src0 >> 2) & 0x3F)];
  aDest[1] = base[(uint32_t)((src0 & 0x03) << 4)];
  aDest[2] = DestT('=');
  aDest[3] = DestT('=');
}

template <typename SrcT, typename DestT>
static void Encode(const SrcT* aSrc, uint32_t aSrcLen, DestT* aDest) {
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
      MOZ_ASSERT_UNREACHABLE("coding error");
  }
}

// END base64 encode code copied and modified from NSPR.

template <typename T>
struct EncodeInputStream_State {
  unsigned char c[3];
  uint8_t charsOnStack;
  typename T::char_type* buffer;
};

template <typename T>
nsresult EncodeInputStream_Encoder(nsIInputStream* aStream, void* aClosure,
                                   const char* aFromSegment, uint32_t aToOffset,
                                   uint32_t aCount, uint32_t* aWriteCount) {
  MOZ_ASSERT(aCount > 0, "Er, what?");

  EncodeInputStream_State<T>* state =
      static_cast<EncodeInputStream_State<T>*>(aClosure);

  // We consume the whole data always.
  *aWriteCount = aCount;

  // If we have any data left from last time, encode it now.
  uint32_t countRemaining = aCount;
  const unsigned char* src = (const unsigned char*)aFromSegment;
  if (state->charsOnStack) {
    MOZ_ASSERT(state->charsOnStack == 1 || state->charsOnStack == 2);

    // Not enough data to compose a triple.
    if (state->charsOnStack == 1 && countRemaining == 1) {
      state->charsOnStack = 2;
      state->c[1] = src[0];
      return NS_OK;
    }

    uint32_t consumed = 0;
    unsigned char firstSet[4];
    if (state->charsOnStack == 1) {
      firstSet[0] = state->c[0];
      firstSet[1] = src[0];
      firstSet[2] = src[1];
      firstSet[3] = '\0';
      consumed = 2;
    } else /* state->charsOnStack == 2 */ {
      firstSet[0] = state->c[0];
      firstSet[1] = state->c[1];
      firstSet[2] = src[0];
      firstSet[3] = '\0';
      consumed = 1;
    }

    Encode(firstSet, 3, state->buffer);
    state->buffer += 4;
    countRemaining -= consumed;
    src += consumed;
    state->charsOnStack = 0;

    // Nothing is left.
    if (!countRemaining) {
      return NS_OK;
    }
  }

  // Encode as many full triplets as possible.
  uint32_t encodeLength = countRemaining - countRemaining % 3;
  MOZ_ASSERT(encodeLength % 3 == 0, "Should have an exact number of triplets!");
  Encode(src, encodeLength, state->buffer);
  state->buffer += (encodeLength / 3) * 4;
  src += encodeLength;
  countRemaining -= encodeLength;

  if (countRemaining) {
    // We should never have a full triplet left at this point.
    MOZ_ASSERT(countRemaining < 3, "We should have encoded more!");
    state->c[0] = src[0];
    state->c[1] = (countRemaining == 2) ? src[1] : '\0';
    state->charsOnStack = countRemaining;
  }

  return NS_OK;
}

mozilla::Result<uint32_t, nsresult> CalculateBase64EncodedLength(
    const size_t aBinaryLen, const uint32_t aPrefixLen = 0) {
  mozilla::CheckedUint32 res = aBinaryLen;
  // base 64 encoded length is 4/3rds the length of the input data, rounded up
  res += 2;
  res /= 3;
  res *= 4;
  res += aPrefixLen;
  if (!res.isValid()) {
    return mozilla::Err(NS_ERROR_FAILURE);
  }
  return res.value();
}

template <typename T>
nsresult EncodeInputStream(nsIInputStream* aInputStream, T& aDest,
                           uint32_t aCount, uint32_t aOffset) {
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

  const auto base64LenOrErr = CalculateBase64EncodedLength(count64, aOffset);
  if (base64LenOrErr.isErr()) {
    // XXX For some reason, it was NS_ERROR_OUT_OF_MEMORY here instead of
    // NS_ERROR_FAILURE, so we keep that.
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!aDest.SetLength(base64LenOrErr.inspect(), mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  EncodeInputStream_State<T> state;
  state.charsOnStack = 0;
  state.c[2] = '\0';
  state.buffer = aOffset + aDest.BeginWriting();

  while (true) {
    uint32_t read = 0;

    rv = aInputStream->ReadSegments(&EncodeInputStream_Encoder<T>,
                                    (void*)&state, aCount, &read);
    if (NS_FAILED(rv)) {
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        MOZ_CRASH("Not implemented for async streams!");
      }
      if (rv == NS_ERROR_NOT_IMPLEMENTED) {
        MOZ_CRASH("Requires a stream that implements ReadSegments!");
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

// Maps an encoded character to a value in the Base64 alphabet, per
// RFC 4648, Table 1. Invalid input characters map to UINT8_MAX.

static const uint8_t kBase64DecodeTable[] = {
    // clang-format off
  /* 0 */  255, 255, 255, 255, 255, 255, 255, 255,
  /* 8 */  255, 255, 255, 255, 255, 255, 255, 255,
  /* 16 */ 255, 255, 255, 255, 255, 255, 255, 255,
  /* 24 */ 255, 255, 255, 255, 255, 255, 255, 255,
  /* 32 */ 255, 255, 255, 255, 255, 255, 255, 255,
  /* 40 */ 255, 255, 255,
  62 /* + */,
  255, 255, 255,
  63 /* / */,

  /* 48 */ /* 0 - 9 */ 52, 53, 54, 55, 56, 57, 58, 59,
  /* 56 */ 60, 61, 255, 255, 255, 255, 255, 255,

  /* 64 */ 255, /* A - Z */ 0, 1, 2, 3, 4, 5, 6,
  /* 72 */ 7, 8, 9, 10, 11, 12, 13, 14,
  /* 80 */ 15, 16, 17, 18, 19, 20, 21, 22,
  /* 88 */ 23, 24, 25, 255, 255, 255, 255, 255,
  /* 96 */ 255, /* a - z */ 26, 27, 28, 29, 30, 31, 32,
  /* 104 */ 33, 34, 35, 36, 37, 38, 39, 40,
  /* 112 */ 41, 42, 43, 44, 45, 46, 47, 48,
  /* 120 */ 49, 50, 51, 255, 255, 255, 255, 255,
};
static_assert(mozilla::ArrayLength(kBase64DecodeTable) == 0x80);
// clang-format on

template <typename T>
[[nodiscard]] bool Base64CharToValue(T aChar, uint8_t* aValue) {
  size_t index = static_cast<uint8_t>(aChar);
  if (index >= mozilla::ArrayLength(kBase64DecodeTable)) {
    *aValue = 255;
    return false;
  }
  *aValue = kBase64DecodeTable[index];
  return *aValue != 255;
}

static const char kBase64URLAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
static_assert(mozilla::ArrayLength(kBase64URLAlphabet) == 0x41);

// Maps an encoded character to a value in the Base64 URL alphabet, per
// RFC 4648, Table 2. Invalid input characters map to UINT8_MAX.
static const uint8_t kBase64URLDecodeTable[] = {
    // clang-format off
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
  255, 255, 255, 255, 255,
};
static_assert(mozilla::ArrayLength(kBase64URLDecodeTable) == 0x80);
// clang-format on

bool Base64URLCharToValue(char aChar, uint8_t* aValue) {
  uint8_t index = static_cast<uint8_t>(aChar);
  if (index >= mozilla::ArrayLength(kBase64URLDecodeTable)) {
    *aValue = 255;
    return false;
  }
  *aValue = kBase64URLDecodeTable[index];
  return *aValue != 255;
}

}  // namespace

namespace mozilla {

nsresult Base64EncodeInputStream(nsIInputStream* aInputStream,
                                 nsACString& aDest, uint32_t aCount,
                                 uint32_t aOffset) {
  return EncodeInputStream<nsACString>(aInputStream, aDest, aCount, aOffset);
}

nsresult Base64EncodeInputStream(nsIInputStream* aInputStream, nsAString& aDest,
                                 uint32_t aCount, uint32_t aOffset) {
  return EncodeInputStream<nsAString>(aInputStream, aDest, aCount, aOffset);
}

nsresult Base64Encode(const char* aBinary, uint32_t aBinaryLen,
                      char** aBase64) {
  if (aBinaryLen == 0) {
    *aBase64 = (char*)moz_xmalloc(1);
    (*aBase64)[0] = '\0';
    return NS_OK;
  }

  const auto base64LenOrErr = CalculateBase64EncodedLength(aBinaryLen);
  if (base64LenOrErr.isErr()) {
    return base64LenOrErr.inspectErr();
  }
  const uint32_t base64Len = base64LenOrErr.inspect();

  *aBase64 = nullptr;

  // Add one byte for null termination.
  UniqueFreePtr<char[]> base64((char*)malloc(base64Len + 1));
  if (!base64) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  Encode(aBinary, aBinaryLen, base64.get());
  base64[base64Len] = '\0';

  *aBase64 = base64.release();
  return NS_OK;
}

template <bool Append = false, typename T, typename U>
static nsresult Base64EncodeHelper(const T* const aBinary,
                                   const size_t aBinaryLen, U& aBase64) {
  if (aBinaryLen == 0) {
    if (!Append) {
      aBase64.Truncate();
    }
    return NS_OK;
  }

  const uint32_t prefixLen = Append ? aBase64.Length() : 0;
  const auto base64LenOrErr =
      CalculateBase64EncodedLength(aBinaryLen, prefixLen);
  if (base64LenOrErr.isErr()) {
    return base64LenOrErr.inspectErr();
  }
  const uint32_t base64Len = base64LenOrErr.inspect();

  auto handleOrErr = aBase64.BulkWrite(base64Len, prefixLen, false);
  if (handleOrErr.isErr()) {
    return handleOrErr.unwrapErr();
  }

  auto handle = handleOrErr.unwrap();

  Encode(aBinary, aBinaryLen, handle.Elements() + prefixLen);
  handle.Finish(base64Len, false);
  return NS_OK;
}

nsresult Base64EncodeAppend(const char* aBinary, uint32_t aBinaryLen,
                            nsAString& aBase64) {
  return Base64EncodeHelper<true>(aBinary, aBinaryLen, aBase64);
}

nsresult Base64EncodeAppend(const char* aBinary, uint32_t aBinaryLen,
                            nsACString& aBase64) {
  return Base64EncodeHelper<true>(aBinary, aBinaryLen, aBase64);
}

nsresult Base64EncodeAppend(const nsACString& aBinary, nsACString& aBase64) {
  return Base64EncodeHelper<true>(aBinary.BeginReading(), aBinary.Length(),
                                  aBase64);
}

nsresult Base64EncodeAppend(const nsACString& aBinary, nsAString& aBase64) {
  return Base64EncodeHelper<true>(aBinary.BeginReading(), aBinary.Length(),
                                  aBase64);
}

nsresult Base64Encode(const char* aBinary, uint32_t aBinaryLen,
                      nsACString& aBase64) {
  return Base64EncodeHelper(aBinary, aBinaryLen, aBase64);
}

nsresult Base64Encode(const char* aBinary, uint32_t aBinaryLen,
                      nsAString& aBase64) {
  return Base64EncodeHelper(aBinary, aBinaryLen, aBase64);
}

nsresult Base64Encode(const nsACString& aBinary, nsACString& aBase64) {
  return Base64EncodeHelper(aBinary.BeginReading(), aBinary.Length(), aBase64);
}

nsresult Base64Encode(const nsACString& aBinary, nsAString& aBase64) {
  return Base64EncodeHelper(aBinary.BeginReading(), aBinary.Length(), aBase64);
}

nsresult Base64Encode(const nsAString& aBinary, nsAString& aBase64) {
  return Base64EncodeHelper(aBinary.BeginReading(), aBinary.Length(), aBase64);
}

template <typename T, typename U, typename Decoder>
static bool Decode4to3(const T* aSrc, U* aDest, Decoder aToVal) {
  uint8_t w, x, y, z;
  if (!aToVal(aSrc[0], &w) || !aToVal(aSrc[1], &x) || !aToVal(aSrc[2], &y) ||
      !aToVal(aSrc[3], &z)) {
    return false;
  }
  aDest[0] = U(uint8_t(w << 2 | x >> 4));
  aDest[1] = U(uint8_t(x << 4 | y >> 2));
  aDest[2] = U(uint8_t(y << 6 | z));
  return true;
}

template <typename T, typename U, typename Decoder>
static bool Decode3to2(const T* aSrc, U* aDest, Decoder aToVal) {
  uint8_t w, x, y;
  if (!aToVal(aSrc[0], &w) || !aToVal(aSrc[1], &x) || !aToVal(aSrc[2], &y)) {
    return false;
  }
  aDest[0] = U(uint8_t(w << 2 | x >> 4));
  aDest[1] = U(uint8_t(x << 4 | y >> 2));
  return true;
}

template <typename T, typename U, typename Decoder>
static bool Decode2to1(const T* aSrc, U* aDest, Decoder aToVal) {
  uint8_t w, x;
  if (!aToVal(aSrc[0], &w) || !aToVal(aSrc[1], &x)) {
    return false;
  }
  aDest[0] = U(uint8_t(w << 2 | x >> 4));
  return true;
}

template <typename SrcT, typename DestT>
static nsresult Base64DecodeHelper(const SrcT* aBase64, uint32_t aBase64Len,
                                   DestT* aBinary, uint32_t* aBinaryLen) {
  MOZ_ASSERT(aBinary);

  const SrcT* input = aBase64;
  uint32_t inputLength = aBase64Len;
  DestT* binary = aBinary;
  uint32_t binaryLength = 0;

  // Handle trailing '=' characters.
  if (inputLength && (inputLength % 4 == 0)) {
    if (aBase64[inputLength - 1] == SrcT('=')) {
      if (aBase64[inputLength - 2] == SrcT('=')) {
        inputLength -= 2;
      } else {
        inputLength -= 1;
      }
    }
  }

  while (inputLength >= 4) {
    if (!Decode4to3(input, binary, Base64CharToValue<SrcT>)) {
      return NS_ERROR_INVALID_ARG;
    }

    input += 4;
    inputLength -= 4;
    binary += 3;
    binaryLength += 3;
  }

  switch (inputLength) {
    case 3:
      if (!Decode3to2(input, binary, Base64CharToValue<SrcT>)) {
        return NS_ERROR_INVALID_ARG;
      }
      binaryLength += 2;
      break;
    case 2:
      if (!Decode2to1(input, binary, Base64CharToValue<SrcT>)) {
        return NS_ERROR_INVALID_ARG;
      }
      binaryLength += 1;
      break;
    case 1:
      return NS_ERROR_INVALID_ARG;
    case 0:
      break;
    default:
      MOZ_CRASH("Too many characters leftover");
  }

  aBinary[binaryLength] = DestT('\0');
  *aBinaryLen = binaryLength;

  return NS_OK;
}

nsresult Base64Decode(const char* aBase64, uint32_t aBase64Len, char** aBinary,
                      uint32_t* aBinaryLen) {
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

template <typename T, typename U>
static nsresult Base64DecodeString(const T& aBase64, U& aBinary) {
  aBinary.Truncate();

  // Check for overflow.
  if (aBase64.Length() > UINT32_MAX / 3) {
    return NS_ERROR_FAILURE;
  }

  // Don't decode the empty string
  if (aBase64.IsEmpty()) {
    return NS_OK;
  }

  uint32_t binaryLen = ((aBase64.Length() * 3) / 4);

  auto handleOrErr = aBinary.BulkWrite(binaryLen, 0, false);
  if (handleOrErr.isErr()) {
    // Must not touch the handle if failing here, but we
    // already truncated the string at the top, so it's
    // unchanged.
    return handleOrErr.unwrapErr();
  }

  auto handle = handleOrErr.unwrap();

  nsresult rv = Base64DecodeHelper(aBase64.BeginReading(), aBase64.Length(),
                                   handle.Elements(), &binaryLen);
  if (NS_FAILED(rv)) {
    // Retruncate to match old semantics of this method.
    handle.Finish(0, true);
    return rv;
  }

  handle.Finish(binaryLen, true);
  return NS_OK;
}

nsresult Base64Decode(const nsACString& aBase64, nsACString& aBinary) {
  return Base64DecodeString(aBase64, aBinary);
}

nsresult Base64Decode(const nsAString& aBase64, nsAString& aBinary) {
  return Base64DecodeString(aBase64, aBinary);
}

nsresult Base64Decode(const nsAString& aBase64, nsACString& aBinary) {
  return Base64DecodeString(aBase64, aBinary);
}

nsresult Base64URLDecode(const nsACString& aBase64,
                         Base64URLDecodePaddingPolicy aPaddingPolicy,
                         FallibleTArray<uint8_t>& aBinary) {
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
    if (!Decode4to3(base64, binary, Base64URLCharToValue)) {
      return NS_ERROR_INVALID_ARG;
    }
    base64 += 4;
    binary += 3;
  }

  if (base64Len == 3) {
    if (!Decode3to2(base64, binary, Base64URLCharToValue)) {
      return NS_ERROR_INVALID_ARG;
    }
    binary += 2;
  } else if (base64Len == 2) {
    if (!Decode2to1(base64, binary, Base64URLCharToValue)) {
      return NS_ERROR_INVALID_ARG;
    }
    binary += 1;
  } else if (base64Len) {
    return NS_ERROR_INVALID_ARG;
  }

  // Set the length to the actual number of decoded bytes.
  aBinary.TruncateLength(binary - aBinary.Elements());
  return NS_OK;
}

nsresult Base64URLEncode(uint32_t aBinaryLen, const uint8_t* aBinary,
                         Base64URLEncodePaddingPolicy aPaddingPolicy,
                         nsACString& aBase64) {
  aBase64.Truncate();
  // Don't encode empty strings.
  if (aBinaryLen == 0) {
    return NS_OK;
  }

  // Allocate a buffer large enough to hold the encoded string with padding.
  const auto base64LenOrErr = CalculateBase64EncodedLength(aBinaryLen);
  if (base64LenOrErr.isErr()) {
    return base64LenOrErr.inspectErr();
  }
  const uint32_t base64Len = base64LenOrErr.inspect();

  auto handleOrErr = aBase64.BulkWrite(base64Len, 0, false);
  if (handleOrErr.isErr()) {
    return handleOrErr.unwrapErr();
  }

  auto handle = handleOrErr.unwrap();

  char* base64 = handle.Elements();

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

  uint32_t length = base64 - handle.Elements();
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

  handle.Finish(length, false);
  return NS_OK;
}

}  // namespace mozilla
