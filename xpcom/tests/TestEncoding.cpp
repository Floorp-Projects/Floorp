/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

nsresult TestGoodSurrogatePair()
{
  // When this string is decoded, the surrogate pair is U+10302 and the rest of
  // the string is specified by indexes 2 onward.
  const PRUnichar goodPairData[] = {  0xD800, 0xDF02, 0x65, 0x78, 0x0 };
  nsDependentString goodPair16(goodPairData);

  uint32_t byteCount = 0;
  char* goodPair8 = ToNewUTF8String(goodPair16, &byteCount);
  if (!goodPair8)
  {
    fail("out of memory creating goodPair8");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (byteCount != 6)
  {
    fail("wrong number of bytes; expected 6, got %lu", byteCount);
    return NS_ERROR_FAILURE;
  }

  const unsigned char expected8[] =
    { 0xF0, 0x90, 0x8C, 0x82, 0x65, 0x78, 0x0 };
  if (0 != memcmp(expected8, goodPair8, sizeof(expected8)))
  {
    fail("wrong translation to UTF8");
    return NS_ERROR_FAILURE;
  }

  // This takes a different code path from the above, so test it to make sure
  // the UTF-16 enumeration remains in sync with the UTF-8 enumeration.
  nsDependentCString expected((const char*)expected8);
  if (0 != CompareUTF8toUTF16(expected, goodPair16))
  {
    fail("bad comparison between UTF-8 and equivalent UTF-16");
    return NS_ERROR_FAILURE;
  }

  NS_Free(goodPair8);

  passed("TestGoodSurrogatePair");
  return NS_OK;
}

nsresult TestBackwardsSurrogatePair()
{
  // When this string is decoded, the two surrogates are wrongly ordered and
  // must each be interpreted as U+FFFD.
  const PRUnichar backwardsPairData[] = { 0xDDDD, 0xD863, 0x65, 0x78, 0x0 };
  nsDependentString backwardsPair16(backwardsPairData);

  uint32_t byteCount = 0;
  char* backwardsPair8 = ToNewUTF8String(backwardsPair16, &byteCount);
  if (!backwardsPair8)
  {
    fail("out of memory creating backwardsPair8");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (byteCount != 8)
  {
    fail("wrong number of bytes; expected 8, got %lu", byteCount);
    return NS_ERROR_FAILURE;
  }

  const unsigned char expected8[] =
    { 0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD, 0x65, 0x78, 0x0 };
  if (0 != memcmp(expected8, backwardsPair8, sizeof(expected8)))
  {
    fail("wrong translation to UTF8");
    return NS_ERROR_FAILURE;
  }

  // This takes a different code path from the above, so test it to make sure
  // the UTF-16 enumeration remains in sync with the UTF-8 enumeration.
  nsDependentCString expected((const char*)expected8);
  if (0 != CompareUTF8toUTF16(expected, backwardsPair16))
  {
    fail("bad comparison between UTF-8 and malformed but equivalent UTF-16");
    return NS_ERROR_FAILURE;
  }

  NS_Free(backwardsPair8);

  passed("TestBackwardsSurrogatePair");
  return NS_OK;
}

nsresult TestMalformedUTF16OrphanHighSurrogate()
{
  // When this string is decoded, the high surrogate should be replaced and the
  // rest of the string is specified by indexes 1 onward.
  const PRUnichar highSurrogateData[] = { 0xD863, 0x74, 0x65, 0x78, 0x74, 0x0 };
  nsDependentString highSurrogate16(highSurrogateData);

  uint32_t byteCount = 0;
  char* highSurrogate8 = ToNewUTF8String(highSurrogate16, &byteCount);
  if (!highSurrogate8)
  {
    fail("out of memory creating highSurrogate8");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (byteCount != 7)
  {
    fail("wrong number of bytes; expected 7, got %lu", byteCount);
    return NS_ERROR_FAILURE;
  }

  const unsigned char expected8[] =
    { 0xEF, 0xBF, 0xBD, 0x74, 0x65, 0x78, 0x74, 0x0 };
  if (0 != memcmp(expected8, highSurrogate8, sizeof(expected8)))
  {
    fail("wrong translation to UTF8");
    return NS_ERROR_FAILURE;
  }

  // This takes a different code path from the above, so test it to make sure
  // the UTF-16 enumeration remains in sync with the UTF-8 enumeration.
  nsDependentCString expected((const char*)expected8);
  if (0 != CompareUTF8toUTF16(expected, highSurrogate16))
  {
    fail("bad comparison between UTF-8 and malformed but equivalent UTF-16");
    return NS_ERROR_FAILURE;
  }

  NS_Free(highSurrogate8);

  passed("TestMalformedUTF16OrphanHighSurrogate");
  return NS_OK;
}

nsresult TestMalformedUTF16OrphanLowSurrogate()
{
  // When this string is decoded, the low surrogate should be replaced and the
  // rest of the string is specified by indexes 1 onward.
  const PRUnichar lowSurrogateData[] = { 0xDDDD, 0x74, 0x65, 0x78, 0x74, 0x0 };
  nsDependentString lowSurrogate16(lowSurrogateData);

  uint32_t byteCount = 0;
  char* lowSurrogate8 = ToNewUTF8String(lowSurrogate16, &byteCount);
  if (!lowSurrogate8)
  {
    fail("out of memory creating lowSurrogate8");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (byteCount != 7)
  {
    fail("wrong number of bytes; expected 7, got %lu", byteCount);
    return NS_ERROR_FAILURE;
  }

  const unsigned char expected8[] =
    { 0xEF, 0xBF, 0xBD, 0x74, 0x65, 0x78, 0x74, 0x0 };
  if (0 != memcmp(expected8, lowSurrogate8, sizeof(expected8)))
  {
    fail("wrong translation to UTF8");
    return NS_ERROR_FAILURE;
  }

  // This takes a different code path from the above, so test it to make sure
  // the UTF-16 enumeration remains in sync with the UTF-8 enumeration.
  nsDependentCString expected((const char*)expected8);
  if (0 != CompareUTF8toUTF16(expected, lowSurrogate16))
  {
    fail("bad comparison between UTF-8 and malformed but equivalent UTF-16");
    return NS_ERROR_FAILURE;
  }

  NS_Free(lowSurrogate8);

  passed("TestMalformedUTF16OrphanLowSurrogate");
  return NS_OK;
}


int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("TestEncoding");
  if (xpcom.failed())
    return 1;

  int rv = 0;

  if (NS_FAILED(TestGoodSurrogatePair()))
    rv = 1;
  if (NS_FAILED(TestBackwardsSurrogatePair()))
    rv = 1;
  if (NS_FAILED(TestMalformedUTF16OrphanHighSurrogate()))
    rv = 1;
  if (NS_FAILED(TestMalformedUTF16OrphanLowSurrogate()))
    rv = 1;

  return rv;
}
