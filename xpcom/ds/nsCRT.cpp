/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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

// XXX Bug: These tables don't lowercase the upper 128 characters properly

// This table maps uppercase characters to lower case characters;
// characters that are neither upper nor lower case are unaffected.
static const unsigned char kUpper2Lower[256] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64,

    // upper band mapped to lower [A-Z] => [a-z]
       97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,

                                               91, 92, 93, 94, 95,
   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

static const unsigned char kLower2Upper[256] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96,

    // lower band mapped to upper [a-z] => [A-Z]
       65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,

                                              123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

//----------------------------------------------------------------------

char nsCRT::ToUpper(char aChar)
{
  return (char)kLower2Upper[aChar];
}

char nsCRT::ToLower(char aChar)
{
  return (char)kUpper2Lower[aChar];
}

PRBool nsCRT::IsUpper(char aChar)
{
  return aChar != nsCRT::ToLower(aChar);
}

PRBool nsCRT::IsLower(char aChar)
{
  return aChar != nsCRT::ToUpper(aChar);
}

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
    SET_DELIM(delimTable, NS_STATIC_CAST(PRUint8, delims[i]));
  }
  NS_ASSERTION(delims[i] == '\0', "too many delimiters");

  // skip to beginning
  while (*str && IS_DELIM(delimTable, NS_STATIC_CAST(PRUint8, *str))) {
    str++;
  }
  result = str;

  // fix up the end of the token
  while (*str) {
    if (IS_DELIM(delimTable, NS_STATIC_CAST(PRUint8, *str))) {
      *str++ = '\0';
      break;
    }
    str++;
  }
  *newStr = str;

  return str == result ? NULL : result;
}

////////////////////////////////////////////////////////////////////////////////

PRUint32 nsCRT::strlen(const PRUnichar* s)
{
  PRUint32 len = 0;
  if(s) {
    while (*s++ != 0) {
      len++;
    }
  }
  return len;
}


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
  nsCRT::memcpy(rslt, str, len * sizeof(PRUnichar));
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

  unsigned char c;
  while ( (c = *s++) )
    h = (h>>28) ^ (h<<4) ^ c;

  if ( resultingStrLen )
    *resultingStrLen = (s-str)-1;
  return h;
}

PRUint32 nsCRT::HashCode(const PRUnichar* str, PRUint32* resultingStrLen)
{
  PRUint32 h = 0;
  const PRUnichar* s = str;

  {
    PRUint16 W1 = 0;      // the first UTF-16 word in a two word tuple
    PRUint32 U = 0;       // the current char as UCS-4
    int code_length = 0;  // the number of bytes in the UTF-8 sequence for the current char

    PRUint16 W;
    while ( (W = *s++) )
      {
          /*
           * On the fly, decoding from UTF-16 (and/or UCS-2) into UTF-8 as per
           *  http://www.ietf.org/rfc/rfc2781.txt
           *  http://www.ietf.org/rfc/rfc2279.txt
           */

        if ( !W1 )
          {
            if ( W < 0xD800 || 0xDFFF < W )
              {
                U = W;
                if ( W <= 0x007F )
                  code_length = 1;
                else if ( W <= 0x07FF )
                  code_length = 2;
                else
                  code_length = 3;
              }
            else if ( /* 0xD800 <= W1 && */ W <= 0xDBFF )
              W1 = W;
          }
        else
          {
              // as required by the standard, this code is careful to
              //  throw out illegal sequences

            if ( 0xDC00 <= W && W <= 0xDFFF )
              {
                U = PRUint32( (W1&0x03FF)<<10 | (W&0x3FFF) );
                if ( U <= 0x001FFFFF )
                  code_length = 4;
                else if ( U <= 0x3FFFFFF )
                  code_length = 5;
                else
                  code_length = 6;
              }
            W1 = 0;
          }


        if ( code_length > 0 )
          {
            static const PRUint16 sBytePrefix[7]  = { 0x0000, 0x0000, 0x00C0, 0x00E0, 0x00F0, 0x00F8, 0x00FC };
            static const PRUint16 sShift[7]       = { 0, 0, 6, 12, 18, 24, 30 };

              /*
               * Unlike the algorithm in http://www.ietf.org/rfc/rfc2279.txt
               *  we must calculate the bytes in left to right order so that
               *  our hash result matches what the narrow version would calculate
               *  on an already UTF-8 string.
               */

              // hash the first (and often, only, byte)
            h = (h>>28) ^ (h<<4) ^ (sBytePrefix[code_length] | (U>>sShift[code_length]));

              // an unrolled loop for hashing any remaining bytes in this sequence
            switch ( code_length )
              {  // falling through in each case
                case 6:   h = (h>>28) ^ (h<<4) ^ (0x80 | ((U>>24) & 0x003F));
                case 5:   h = (h>>28) ^ (h<<4) ^ (0x80 | ((U>>18) & 0x003F));
                case 4:   h = (h>>28) ^ (h<<4) ^ (0x80 | ((U>>12) & 0x003F));
                case 3:   h = (h>>28) ^ (h<<4) ^ (0x80 | ((U>>6 ) & 0x003F));
                case 2:   h = (h>>28) ^ (h<<4) ^ (0x80 | ( U      & 0x003F));
                default:  code_length = 0;
                  break;
              }
          }
      }
  }

  if ( resultingStrLen )
    *resultingStrLen = (s-str)-1;
  return h;
}

PRUint32 nsCRT::BufferHashCode(const char* s, PRUint32 len)
{
  PRUint32 h = 0;
  const char* done = s + len;

  while ( s < done )
    h = (h>>28) ^ (h<<4) ^ PRUint8(*s++); // cast to unsigned to prevent possible sign extension

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

/**
 *  Determine if given char in valid ascii range
 *  
 *  @update  ftang 04.27.2000
 *  @param   aChar is character to be tested
 *  @return  TRUE if in ASCII range
 */
PRBool nsCRT::IsAscii(PRUnichar aChar) {
  return (0x0080 > aChar);
}
/**
 *  Determine if given char in valid ascii range
 *  
 *  @update  ftang 10.02.2001
 *  @param   aString is null terminated to be tested
 *  @return  TRUE if all characters aare in ASCII range
 */
PRBool nsCRT::IsAscii(const PRUnichar *aString) {
  while(*aString) {
     if( 0x0080 <= *aString)
        return PR_FALSE;
     aString++;
  }
  return PR_TRUE;
}
/**
 *  Determine if given char in valid ascii range
 *  
 *  @update  ftang 10.02.2001
 *  @param   aString is null terminated to be tested
 *  @return  TRUE if all characters aare in ASCII range
 */
PRBool nsCRT::IsAscii(const char *aString) {
  while(*aString) {
     if( 0x80 & *aString)
        return PR_FALSE;
     aString++;
  }
  return PR_TRUE;
}
/**
 *  Determine if given char in valid alpha range
 *  
 *  @update  rickg 03.10.2000
 *  @param   aChar is character to be tested
 *  @return  TRUE if in alpha range
 */
PRBool nsCRT::IsAsciiAlpha(PRUnichar aChar) {
  // XXX i18n
  if (((aChar >= 'A') && (aChar <= 'Z')) || ((aChar >= 'a') && (aChar <= 'z'))) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

/**
 *  Determine if given char is a valid space character
 *  
 *  @update  rickg 03.10.2000
 *  @param   aChar is character to be tested
 *  @return  TRUE if is valid space char
 */
PRBool nsCRT::IsAsciiSpace(PRUnichar aChar) {
  // XXX i18n
  if ((aChar == ' ') || (aChar == '\r') || (aChar == '\n') || (aChar == '\t')) {
    return PR_TRUE;
  }
  return PR_FALSE;
}



/**
 *  Determine if given char is valid digit
 *  
 *  @update  rickg 03.10.2000
 *  @param   aChar is character to be tested
 *  @return  TRUE if char is a valid digit
 */
PRBool nsCRT::IsAsciiDigit(PRUnichar aChar) {
  // XXX i18n
  return PRBool((aChar >= '0') && (aChar <= '9'));
}
