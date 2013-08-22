/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIServiceManager.h"
#include "nsCharTraits.h"
#include "nsUTF8Utils.h"

//----------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
// My lovely strtok routine

#define IS_DELIM(m, c)          ((m)[(c) >> 3] & (1 << ((c) & 7)))
#define SET_DELIM(m, c)         ((m)[(c) >> 3] |= (1 << ((c) & 7)))
#define DELIM_TABLE_SIZE        32

char* nsCRT::strtok(char* string, const char* delims, char* *newStr)
{
  NS_ASSERTION(string, "Unlike regular strtok, the first argument cannot be null.");

  char delimTable[DELIM_TABLE_SIZE];
  uint32_t i;
  char* result;
  char* str = string;

  for (i = 0; i < DELIM_TABLE_SIZE; i++)
    delimTable[i] = '\0';

  for (i = 0; delims[i]; i++) {
    SET_DELIM(delimTable, static_cast<uint8_t>(delims[i]));
  }
  NS_ASSERTION(delims[i] == '\0', "too many delimiters");

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
  *newStr = str;

  return str == result ? NULL : result;
}

////////////////////////////////////////////////////////////////////////////////

/**
 * Compare unichar string ptrs, stopping at the 1st null 
 * NOTE: If both are null, we return 0.
 * NOTE: We terminate the search upon encountering a NULL
 *
 * @update  gess 11/10/99
 * @param   s1 and s2 both point to unichar strings
 * @return  0 if they match, -1 if s1<s2; 1 if s1>s2
 */
int32_t nsCRT::strcmp(const PRUnichar* s1, const PRUnichar* s2) {
  if(s1 && s2) {
    for (;;) {
      PRUnichar c1 = *s1++;
      PRUnichar c2 = *s2++;
      if (c1 != c2) {
        if (c1 < c2) return -1;
        return 1;
      }
      if ((0==c1) || (0==c2)) break;
    }
  }
  else {
    if (s1)                     // s2 must have been null
      return -1;
    if (s2)                     // s1 must have been null
      return 1;
  }
  return 0;
}

/**
 * Compare unichar string ptrs, stopping at the 1st null or nth char.
 * NOTE: If either is null, we return 0.
 * NOTE: We DO NOT terminate the search upon encountering NULL's before N
 *
 * @update  gess 11/10/99
 * @param   s1 and s2 both point to unichar strings
 * @return  0 if they match, -1 if s1<s2; 1 if s1>s2
 */
int32_t nsCRT::strncmp(const PRUnichar* s1, const PRUnichar* s2, uint32_t n) {
  if(s1 && s2) { 
    if(n != 0) {
      do {
        PRUnichar c1 = *s1++;
        PRUnichar c2 = *s2++;
        if (c1 != c2) {
          if (c1 < c2) return -1;
          return 1;
        }
      } while (--n != 0);
    }
  }
  return 0;
}

const char* nsCRT::memmem(const char* haystack, uint32_t haystackLen,
                          const char* needle, uint32_t needleLen)
{
  // Sanity checking
  if (!(haystack && needle && haystackLen && needleLen &&
        needleLen <= haystackLen))
    return NULL;

#ifdef HAVE_MEMMEM
  return (const char*)::memmem(haystack, haystackLen, needle, needleLen);
#else
  // No memmem means we need to roll our own.  This isn't really optimized
  // for performance ... if that becomes an issue we can take some inspiration
  // from the js string compare code in jsstr.cpp
  for (uint32_t i = 0; i < haystackLen - needleLen; i++) {
    if (!memcmp(haystack + i, needle, needleLen))
      return haystack + i;
  }
#endif
  return NULL;
}

// This should use NSPR but NSPR isn't exporting its PR_strtoll function
// Until then...
int64_t nsCRT::atoll(const char *str)
{
    if (!str)
        return 0;

    int64_t ll = 0;

    while (*str && *str >= '0' && *str <= '9') {
        ll *= 10;
        ll += *str - '0';
        str++;
    }

    return ll;
}

