/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Base64.h"

#include "nsIInputStream.h"
#include "nsString.h"

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
  NS_ABORT_IF_FALSE(encodeLength % 3 == 0,
                    "Should have an exact number of triplets!");
  Encode(src, encodeLength, state->buffer);
  state->buffer += (encodeLength / 3) * 4;
  src += encodeLength;
  countRemaining -= encodeLength;

  // We must consume all data, so if there's some data left stash it
  *aWriteCount = aCount;

  if (countRemaining) {
    // We should never have a full triplet left at this point.
    NS_ABORT_IF_FALSE(countRemaining < 3, "We should have encoded more!");
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

  aDest.SetLength(count + aOffset);
  if (aDest.Length() != count + aOffset) {
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

} // namespace (anonymous)

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
Base64Encode(const nsACString& aBinaryData, nsACString& aString)
{
  // Check for overflow.
  if (aBinaryData.Length() > (UINT32_MAX / 4) * 3) {
    return NS_ERROR_FAILURE;
  }

  // Don't ask PR_Base64Encode to encode empty strings
  if (aBinaryData.IsEmpty()) {
    aString.Truncate();
    return NS_OK;
  }

  uint32_t stringLen = ((aBinaryData.Length() + 2) / 3) * 4;

  char* buffer;

  // Add one byte for null termination.
  if (aString.SetCapacity(stringLen + 1, fallible_t()) &&
      (buffer = aString.BeginWriting()) &&
      PL_Base64Encode(aBinaryData.BeginReading(), aBinaryData.Length(), buffer)) {
    // PL_Base64Encode doesn't null terminate the buffer for us when we pass
    // the buffer in. Do that manually.
    buffer[stringLen] = '\0';

    aString.SetLength(stringLen);
    return NS_OK;
  }

  aString.Truncate();
  return NS_ERROR_INVALID_ARG;
}

nsresult
Base64Encode(const nsAString& aString, nsAString& aBinaryData)
{
  NS_LossyConvertUTF16toASCII string(aString);
  nsAutoCString binaryData;

  nsresult rv = Base64Encode(string, binaryData);
  if (NS_SUCCEEDED(rv)) {
    CopyASCIItoUTF16(binaryData, aBinaryData);
  } else {
    aBinaryData.Truncate();
  }

  return rv;
}

nsresult
Base64Decode(const nsACString& aString, nsACString& aBinaryData)
{
  // Check for overflow.
  if (aString.Length() > UINT32_MAX / 3) {
    return NS_ERROR_FAILURE;
  }

  // Don't ask PR_Base64Decode to decode the empty string
  if (aString.IsEmpty()) {
    aBinaryData.Truncate();
    return NS_OK;
  }

  uint32_t binaryDataLen = ((aString.Length() * 3) / 4);

  char* buffer;

  // Add one byte for null termination.
  if (aBinaryData.SetCapacity(binaryDataLen + 1, fallible_t()) &&
      (buffer = aBinaryData.BeginWriting()) &&
      PL_Base64Decode(aString.BeginReading(), aString.Length(), buffer)) {
    // PL_Base64Decode doesn't null terminate the buffer for us when we pass
    // the buffer in. Do that manually, taking into account the number of '='
    // characters we were passed.
    if (!aString.IsEmpty() && aString[aString.Length() - 1] == '=') {
      if (aString.Length() > 1 && aString[aString.Length() - 2] == '=') {
        binaryDataLen -= 2;
      } else {
        binaryDataLen -= 1;
      }
    }
    buffer[binaryDataLen] = '\0';

    aBinaryData.SetLength(binaryDataLen);
    return NS_OK;
  }

  aBinaryData.Truncate();
  return NS_ERROR_INVALID_ARG;
}

nsresult
Base64Decode(const nsAString& aBinaryData, nsAString& aString)
{
  NS_LossyConvertUTF16toASCII binaryData(aBinaryData);
  nsAutoCString string;

  nsresult rv = Base64Decode(binaryData, string);
  if (NS_SUCCEEDED(rv)) {
    CopyASCIItoUTF16(string, aString);
  } else {
    aString.Truncate();
  }

  return rv;
}

} // namespace mozilla
