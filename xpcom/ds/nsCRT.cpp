/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * MODULE NOTES:
 * @update  gess7/30/98
 *
 * Much as I hate to do it, we were using string compares wrong.
 * Often, programmers call functions like strcmp(s1,s2), and pass
 * one or more null strings. Rather than blow up on these, I've
 * added quick checks to ensure that cases like this don't cause
 * us to fail.
 *
 * In general, if you pass a null into any of these string compare
 * routines, we simply return 0.
 */


#include "nsCRT.h"
#include "nsDebug.h"

//----------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
// My lovely strtok routine

#define IS_DELIM(m, c)          ((m)[(c) >> 3] & (1 << ((c) & 7)))
#define SET_DELIM(m, c)         ((m)[(c) >> 3] |= (1 << ((c) & 7)))
#define DELIM_TABLE_SIZE        32

char*
nsCRT::strtok(char* aString, const char* aDelims, char** aNewStr)
{
  NS_ASSERTION(aString,
               "Unlike regular strtok, the first argument cannot be null.");

  char delimTable[DELIM_TABLE_SIZE];
  uint32_t i;
  char* result;
  char* str = aString;

  for (i = 0; i < DELIM_TABLE_SIZE; ++i) {
    delimTable[i] = '\0';
  }

  for (i = 0; aDelims[i]; i++) {
    SET_DELIM(delimTable, static_cast<uint8_t>(aDelims[i]));
  }
  NS_ASSERTION(aDelims[i] == '\0', "too many delimiters");

  // skip to beginning
  while (*str && IS_DELIM(delimTable, static_cast<uint8_t>(*str))) {
    str++;
  }
  result = str;

  // fix up the end of the token
  while (*str) {
    if (IS_DELIM(delimTable, static_cast<uint8_t>(*str))) {
      *str++ = '\0';
      break;
    }
    str++;
  }
  *aNewStr = str;

  return str == result ? nullptr : result;
}

////////////////////////////////////////////////////////////////////////////////

/**
 * Compare unichar string ptrs, stopping at the 1st null
 * NOTE: If both are null, we return 0.
 * NOTE: We terminate the search upon encountering a nullptr
 *
 * @update  gess 11/10/99
 * @param   s1 and s2 both point to unichar strings
 * @return  0 if they match, -1 if s1<s2; 1 if s1>s2
 */
int32_t
nsCRT::strcmp(const char16_t* aStr1, const char16_t* aStr2)
{
  if (aStr1 && aStr2) {
    for (;;) {
      char16_t c1 = *aStr1++;
      char16_t c2 = *aStr2++;
      if (c1 != c2) {
        if (c1 < c2) {
          return -1;
        }
        return 1;
      }
      if (c1 == 0 || c2 == 0) {
        break;
      }
    }
  } else {
    if (aStr1) {  // aStr2 must have been null
      return -1;
    }
    if (aStr2) {  // aStr1 must have been null
      return 1;
    }
  }
  return 0;
}

const char*
nsCRT::memmem(const char* aHaystack, uint32_t aHaystackLen,
              const char* aNeedle, uint32_t aNeedleLen)
{
  // Sanity checking
  if (!(aHaystack && aNeedle && aHaystackLen && aNeedleLen &&
        aNeedleLen <= aHaystackLen)) {
    return nullptr;
  }

#ifdef HAVE_MEMMEM
  return (const char*)::memmem(aHaystack, aHaystackLen, aNeedle, aNeedleLen);
#else
  // No memmem means we need to roll our own.  This isn't really optimized
  // for performance ... if that becomes an issue we can take some inspiration
  // from the js string compare code in jsstr.cpp
  for (uint32_t i = 0; i < aHaystackLen - aNeedleLen; i++) {
    if (!memcmp(aHaystack + i, aNeedle, aNeedleLen)) {
      return aHaystack + i;
    }
  }
#endif
  return nullptr;
}

// This should use NSPR but NSPR isn't exporting its PR_strtoll function
// Until then...
int64_t
nsCRT::atoll(const char* aStr)
{
  if (!aStr) {
    return 0;
  }

  int64_t ll = 0;

  while (*aStr && *aStr >= '0' && *aStr <= '9') {
    ll *= 10;
    ll += *aStr - '0';
    aStr++;
  }

  return ll;
}

