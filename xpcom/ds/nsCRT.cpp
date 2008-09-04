/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

 
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
#include "prbit.h"

#define ADD_TO_HASHVAL(hashval, c) \
    hashval = PR_ROTATE_LEFT32(hashval, 4) ^ (c);

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
  PRUint32 i;
  char* result;
  char* str = string;

  for (i = 0; i < DELIM_TABLE_SIZE; i++)
    delimTable[i] = '\0';

  for (i = 0; delims[i]; i++) {
    SET_DELIM(delimTable, static_cast<PRUint8>(delims[i]));
  }
  NS_ASSERTION(delims[i] == '\0', "too many delimiters");

  // skip to beginning
  while (*str && IS_DELIM(delimTable, static_cast<PRUint8>(*str))) {
    str++;
  }
  result = str;

  // fix up the end of the token
  while (*str) {
    if (IS_DELIM(delimTable, static_cast<PRUint8>(*str))) {
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
PRInt32 nsCRT::strcmp(const PRUnichar* s1, const PRUnichar* s2) {
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
PRInt32 nsCRT::strncmp(const PRUnichar* s1, const PRUnichar* s2, PRUint32 n) {
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

PRUnichar* nsCRT::strdup(const PRUnichar* str)
{
  PRUint32 len = nsCRT::strlen(str);
  return strndup(str, len);
}

PRUnichar* nsCRT::strndup(const PRUnichar* str, PRUint32 len)
{
	nsCppSharedAllocator<PRUnichar> shared_allocator;
	PRUnichar* rslt = shared_allocator.allocate(len + 1); // add one for the null
  // PRUnichar* rslt = new PRUnichar[len + 1];

  if (rslt == NULL) return NULL;
  memcpy(rslt, str, len * sizeof(PRUnichar));
  rslt[len] = 0;
  return rslt;
}

  /**
   * |nsCRT::HashCode| is identical to |PL_HashString|, which tests
   *  (http://bugzilla.mozilla.org/showattachment.cgi?attach_id=26596)
   *  show to be the best hash among several other choices.
   *
   * We re-implement it here rather than calling it for two reasons:
   *  (1) in this interface, we also calculate the length of the
   *  string being hashed; and (2) the narrow and wide and `buffer' versions here
   *  will hash equivalent strings to the same value, e.g., "Hello" and L"Hello".
   */
PRUint32 nsCRT::HashCode(const char* str, PRUint32* resultingStrLen)
{
  PRUint32 h = 0;
  const char* s = str;

  if (!str) return h;

  unsigned char c;
  while ( (c = *s++) )
    ADD_TO_HASHVAL(h, c);

  if ( resultingStrLen )
    *resultingStrLen = (s-str)-1;
  return h;
}

PRUint32 nsCRT::HashCode(const char* start, PRUint32 length)
{
  PRUint32 h = 0;
  const char* s = start;
  const char* end = start + length;

  unsigned char c;
  while ( s < end ) {
    c = *s++;
    ADD_TO_HASHVAL(h, c);
  }

  return h;
}

PRUint32 nsCRT::HashCode(const PRUnichar* str, PRUint32* resultingStrLen)
{
  PRUint32 h = 0;
  const PRUnichar* s = str;

  if (!str) return h;

  PRUnichar c;
  while ( (c = *s++) )
    ADD_TO_HASHVAL(h, c);

  if ( resultingStrLen )
    *resultingStrLen = (s-str)-1;
  return h;
}

PRUint32 nsCRT::HashCodeAsUTF8(const PRUnichar* start, PRUint32 length)
{
  PRUint32 h = 0;
  const PRUnichar* s = start;
  const PRUnichar* end = start + length;

  PRUint16 W1 = 0;      // the first UTF-16 word in a two word tuple
  PRUint32 U = 0;       // the current char as UCS-4
  int code_length = 0;  // the number of bytes in the UTF-8 sequence for the current char

  PRUint16 W;
  while ( s < end )
    {
      W = *s++;
        /*
         * On the fly, decoding from UTF-16 (and/or UCS-2) into UTF-8 as per
         *  http://www.ietf.org/rfc/rfc2781.txt
         *  http://www.ietf.org/rfc/rfc3629.txt
         */

      if ( !W1 )
        {
          if ( !IS_SURROGATE(W) )
            {
              U = W;
              if ( W <= 0x007F )
                code_length = 1;
              else if ( W <= 0x07FF )
                code_length = 2;
              else
                code_length = 3;
            }
          else if ( NS_IS_HIGH_SURROGATE(W) && s < end)
            {
              W1 = W;

              continue;
            }
          else
            {
              // Treat broken characters as the Unicode replacement
              // character 0xFFFD
              U = 0xFFFD;

              code_length = 3;

              NS_WARNING("Got low surrogate but no previous high surrogate");
            }
        }
      else
        {
          // as required by the standard, this code is careful to
          // throw out illegal sequences

          if ( NS_IS_LOW_SURROGATE(W) )
            {
              U = SURROGATE_TO_UCS4(W1, W);
              NS_ASSERTION(IS_VALID_CHAR(U), "How did this happen?");
              code_length = 4;
            }
          else
            {
              // Treat broken characters as the Unicode replacement
              // character 0xFFFD
              U = 0xFFFD;

              code_length = 3;

              NS_WARNING("High surrogate not followed by low surrogate");

              // The pointer to the next character points to the second 16-bit
              // value, not beyond it, as per Unicode 5.0.0 Chapter 3 C10, only
              // the first code unit of an illegal sequence must be treated as
              // an illegally terminated code unit sequence (also Chapter 3
              // D91, "isolated [not paired and ill-formed] UTF-16 code units
              // in the range D800..DFFF are ill-formed").
              --s;
            }

          W1 = 0;
        }


      static const PRUint16 sBytePrefix[5]  = { 0x0000, 0x0000, 0x00C0, 0x00E0, 0x00F0  };
      static const PRUint16 sShift[5]       = { 0, 0, 6, 12, 18 };

      /*
       *  Unlike the algorithm in
       *  http://www.ietf.org/rfc/rfc3629.txt we must calculate the
       *  bytes in left to right order so that our hash result
       *  matches what the narrow version would calculate on an
       *  already UTF-8 string.
       */

      // hash the first (and often, only, byte)
      ADD_TO_HASHVAL(h, (sBytePrefix[code_length] | (U>>sShift[code_length])));

      // an unrolled loop for hashing any remaining bytes in this
      // sequence
      switch ( code_length )
        {  // falling through in each case
          case 4:   ADD_TO_HASHVAL(h, (0x80 | ((U>>12) & 0x003F)));
          case 3:   ADD_TO_HASHVAL(h, (0x80 | ((U>>6 ) & 0x003F)));
          case 2:   ADD_TO_HASHVAL(h, (0x80 | ( U      & 0x003F)));
          default:  code_length = 0;
            break;
        }
    }

  return h;
}

PRUint32 nsCRT::BufferHashCode(const PRUnichar* s, PRUint32 len)
{
  PRUint32 h = 0;
  const PRUnichar* done = s + len;

  while ( s < done )
    h = PR_ROTATE_LEFT32(h, 4) ^ PRUint16(*s++); // cast to unsigned to prevent possible sign extension
  return h;
}

// This should use NSPR but NSPR isn't exporting its PR_strtoll function
// Until then...
PRInt64 nsCRT::atoll(const char *str)
{
    if (!str)
        return LL_Zero();

    PRInt64 ll = LL_Zero(), digitll = LL_Zero();

    while (*str && *str >= '0' && *str <= '9') {
        LL_MUL(ll, ll, 10);
        LL_UI2L(digitll, (*str - '0'));
        LL_ADD(ll, ll, digitll);
        str++;
    }

    return ll;
}

