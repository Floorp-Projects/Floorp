/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */ 

 
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
#include "nsUnicharUtilCIID.h"
#include "nsIServiceManager.h"
#include "nsICaseConversion.h"


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

// XXX bug: this doesn't map 0x80 to 0x9f properly
 const PRUnichar kIsoLatin1ToUCS2[256] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
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

//----------------------------------------------------------------------

#define TOLOWER(_ucs2) \
  (((_ucs2) < 128) ? PRUnichar(kUpper2Lower[_ucs2]) : _ToLower(_ucs2))

#define TOUPPER(_ucs2) \
  (((_ucs2) < 128) ? PRUnichar(kLower2Upper[_ucs2]) : _ToUpper(_ucs2))

class HandleCaseConversionShutdown : public nsIShutdownListener {
public :
   NS_IMETHOD OnShutdown(const nsCID& cid, nsISupports* service);
   HandleCaseConversionShutdown(void) { NS_INIT_REFCNT(); }
   virtual ~HandleCaseConversionShutdown(void) {}
   NS_DECL_ISUPPORTS
};
static NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);
static NS_DEFINE_IID(kICaseConversionIID, NS_ICASECONVERSION_IID);

static nsICaseConversion * gCaseConv = NULL; 

static NS_DEFINE_IID(kIShutdownListenerIID, NS_ISHUTDOWNLISTENER_IID);
NS_IMPL_ISUPPORTS(HandleCaseConversionShutdown, kIShutdownListenerIID);

nsresult
HandleCaseConversionShutdown::OnShutdown(const nsCID& cid, nsISupports* service)
{
    if (cid.Equals(kUnicharUtilCID)) {
        NS_ASSERTION(service == gCaseConv, "wrong service!");
        nsrefcnt cnt = gCaseConv->Release();
        gCaseConv = NULL;
    }
    return NS_OK;
}

static HandleCaseConversionShutdown* gListener = NULL;

static void StartUpCaseConversion()
{
    nsresult err;

    if ( NULL == gListener )
    {
      gListener = new HandleCaseConversionShutdown();
      gListener->AddRef();
    }
    err = nsServiceManager::GetService(kUnicharUtilCID, kICaseConversionIID,
                                        (nsISupports**) &gCaseConv, gListener);
}
static void CheckCaseConversion()
{
    if(NULL == gCaseConv )
      StartUpCaseConversion();

    NS_ASSERTION( gCaseConv != NULL , "cannot obtain UnicharUtil");
   
}

static PRUnichar _ToLower(PRUnichar aChar)
{
  PRUnichar oLower;
  CheckCaseConversion();
  nsresult err = gCaseConv->ToLower(aChar, &oLower);
  NS_ASSERTION( NS_SUCCEEDED(err),  "failed to communicate to UnicharUtil");
  return ( NS_SUCCEEDED(err) ) ? oLower : aChar ;
}

static PRUnichar _ToUpper(PRUnichar aChar)
{
  nsresult err;
  PRUnichar oUpper;
  CheckCaseConversion();
  err = gCaseConv->ToUpper(aChar, &oUpper);
  NS_ASSERTION( NS_SUCCEEDED(err),  "failed to communicate to UnicharUtil");
  return ( NS_SUCCEEDED(err) ) ? oUpper : aChar ;
}

//----------------------------------------------------------------------

PRUnichar nsCRT::ToUpper(PRUnichar aChar)
{
  return TOUPPER(aChar);
}

PRUnichar nsCRT::ToLower(PRUnichar aChar)
{
  return TOLOWER(aChar);
}

PRBool nsCRT::IsUpper(PRUnichar aChar)
{
  return aChar != nsCRT::ToLower(aChar);
}

PRBool nsCRT::IsLower(PRUnichar aChar)
{
  return aChar != nsCRT::ToUpper(aChar);
}

////////////////////////////////////////////////////////////////////////////////
// My lovely strtok routine

#define IS_DELIM(m, c)          ((m)[(c) >> 3] & (1 << ((c) & 7)))
#define SET_DELIM(m, c)         ((m)[(c) >> 3] |= (1 << ((c) & 7)))
#define DELIM_TABLE_SIZE        32

char* nsCRT::strtok(char* str, const char* delims, char* *newStr)
{
  NS_ASSERTION(str, "Unlike regular strtok, the first argument cannot be null.");

  char delimTable[DELIM_TABLE_SIZE];
  PRUint32 i;
  char* result;

  for (i = 0; i < DELIM_TABLE_SIZE; i++)
    delimTable[i] = '\0';

  for (i = 0; i < DELIM_TABLE_SIZE && delims[i]; i++) {
    SET_DELIM(delimTable, delims[i]);
  }
  NS_ASSERTION(delims[i] == '\0', "too many delimiters");

  // skip to beginning
  while (*str && IS_DELIM(delimTable, *str)) {
    str++;
  }
  result = str;

  // fix up the end of the token
  while (*str) {
    if (IS_DELIM(delimTable, *str)) {
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
 * @update  gess7/30/98
 * @param   s1 and s2 both point to unichar strings
 * @return  0 if they match, -1 if s1<s2; 1 if s1>s2
 */
PRInt32 nsCRT::strcmp(const PRUnichar* s1, const PRUnichar* s2)
{
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
  return 0;
}

/**
 * Compare unichar string ptrs, stopping at the 1st null or nth char.
 * NOTE: If either is null, we return 0.
 * @update  gess7/30/98
 * @param   s1 and s2 both point to unichar strings
 * @return  0 if they match, -1 if s1<s2; 1 if s1>s2
 */
PRInt32 nsCRT::strncmp(const PRUnichar* s1, const PRUnichar* s2, PRUint32 n)
{
  if(s1 && s2) { 
    if(n != 0) {
      do {
        PRUnichar c1 = *s1++;
        PRUnichar c2 = *s2++;
        if (c1 != c2) {
          if (c1 < c2) return -1;
          return 1;
        }
        if ((0==c1) || (0==c2)) break;
      } while (--n != 0);
    }
  }
  return 0;
}


/**
 * Compare unichar string ptrs without regard to case
 * NOTE: If both are null, we return 0.
 * @update  gess7/30/98
 * @param   s1 and s2 both point to unichar strings
 * @return  0 if they match, -1 if s1<s2; 1 if s1>s2
 */
PRInt32 nsCRT::strcasecmp(const PRUnichar* s1, const PRUnichar* s2)
{
  if(s1 && s2) {
    for (;;) {
      PRUnichar c1 = *s1++;
      PRUnichar c2 = *s2++;
      if (c1 != c2) {
        c1 = TOLOWER(c1);
        c2 = TOLOWER(c2);
        if (c1 != c2) {
          if (c1 < c2) return -1;
          return 1;
        }
      }
      if ((0==c1) || (0==c2)) break;
    }
  }
  return 0;
}

/**
 * Compare unichar string ptrs, stopping at the 1st null or nth char;
 * also ignoring the case of characters.
 * NOTE: If both are null, we return 0.
 * @update  gess7/30/98
 * @param   s1 and s2 both point to unichar strings
 * @return  0 if they match, -1 if s1<s2; 1 if s1>s2
 */
PRInt32 nsCRT::strncasecmp(const PRUnichar* s1, const PRUnichar* s2, PRUint32 n)
{
  if(s1 && s2) {
    if(0<n){
      while (--n >= 0) {
        PRUnichar c1 = *s1++;
        PRUnichar c2 = *s2++;
        if (c1 != c2) {
          c1 = TOLOWER(c1);
          c2 = TOLOWER(c2);
          if (c1 != c2) {
            if (c1 < c2) return -1;
            return 1;
          }
        }
        if ((0==c1) || (0==c2)) break;
      }
    }
  }
  return 0;
}


/**
 * Compare a unichar string ptr to cstring.
 * NOTE: If both are null, we return 0.
 * @update  gess7/30/98
 * @param   s1 points to unichar string
 * @param   s2 points to cstring
 * @return  0 if they match, -1 if s1<s2; 1 if s1>s2
 */
PRInt32 nsCRT::strcmp(const PRUnichar* s1, const char* s2)
{
  if(s1 && s2) {
    for (;;) {
      PRUnichar c1 = *s1++;
      PRUnichar c2 = kIsoLatin1ToUCS2[*(const unsigned char*)s2++];
      if (c1 != c2) {
        if (c1 < c2) return -1;
        return 1;
      }
      if ((0==c1) || (0==c2)) break;
    }
  }
  return 0;
}


/**
 * Compare a unichar string ptr to cstring, up to N chars.
 * NOTE: If both are null, we return 0.
 * @update  gess7/30/98
 * @param   s1 points to unichar string
 * @param   s2 points to cstring
 * @return  0 if they match, -1 if s1<s2; 1 if s1>s2
 */
PRInt32 nsCRT::strncmp(const PRUnichar* s1, const char* s2, PRUint32 n)
{
  if(s1 && s2) {
    if(n != 0){
      do {
        PRUnichar c1 = *s1++;
        PRUnichar c2 = kIsoLatin1ToUCS2[*(const unsigned char*)s2++];
        if (c1 != c2) {
          if (c1 < c2) return -1;
          return 1;
        }
        if ((0==c1) || (0==c2)) break;
      } while (--n != 0);
    }
  }
  return 0;
}

/**
 * Compare a unichar string ptr to cstring without regard to case
 * NOTE: If both are null, we return 0.
 * @update  gess7/30/98
 * @param   s1 points to unichar string
 * @param   s2 points to cstring
 * @return  0 if they match, -1 if s1<s2; 1 if s1>s2
 */
PRInt32 nsCRT::strcasecmp(const PRUnichar* s1, const char* s2)
{
  if(s1 && s2) {
    for (;;) {
      PRUnichar c1 = *s1++;
      PRUnichar c2 = kIsoLatin1ToUCS2[*(const unsigned char*)s2++];
      if (c1 != c2) {
        c1 = TOLOWER(c1);
        c2 = TOLOWER(c2);
        if (c1 != c2) {
          if (c1 < c2) return -1;
          return 1;
        }
      }
      if ((0==c1) || (0==c2)) break;
    }
  }
  return 0;
}

/**
 * Caseless compare up to N chars between unichar string ptr to cstring.
 * NOTE: If both are null, we return 0.
 * @update  gess7/30/98
 * @param   s1 points to unichar string
 * @param   s2 points to cstring
 * @return  0 if they match, -1 if s1<s2; 1 if s1>s2
 */
PRInt32 nsCRT::strncasecmp(const PRUnichar* s1, const char* s2, PRUint32 n)
{
  if(s1 && s2){
    if(n != 0){
      do {
        PRUnichar c1 = *s1++;
        PRUnichar c2 = kIsoLatin1ToUCS2[*(const unsigned char*)s2++];
        if (c1 != c2) {
          c1 = TOLOWER(c1);
          c2 = TOLOWER(c2);
          if (c1 != c2) {
            if (c1 < c2) return -1;
            return 1;
          }
        }
        if (c1 == 0) break;
      } while (--n != 0);
    }
  }
  return 0;
}

PRUnichar* nsCRT::strdup(const PRUnichar* str)
{
  PRUint32 len = nsCRT::strlen(str) + 1; // add one for null
  PRUnichar* rslt = new PRUnichar[len];
  if (rslt == NULL) return NULL;
  nsCRT::memcpy(rslt, str, len * sizeof(PRUnichar));
  return rslt;
}

PRUint32 nsCRT::HashValue(const PRUnichar* us)
{
  PRUint32 rv = 0;
  if(us) {
    PRUnichar ch;
    while ((ch = *us++) != 0) {
      // FYI: rv = rv*37 + ch
      rv = ((rv << 5) + (rv << 2) + rv) + ch;
    }
  }
  return rv;
}

PRUint32 nsCRT::HashValue(const PRUnichar* us, PRUint32* uslenp)
{
  PRUint32 rv = 0;
  PRUint32 len = 0;
  PRUnichar ch;
  while ((ch = *us++) != 0) {
    // FYI: rv = rv*37 + ch
    rv = ((rv << 5) + (rv << 2) + rv) + ch;
    len++;
  }
  *uslenp = len;
  return rv;
}

PRInt32 nsCRT::atoi( const PRUnichar *string )
{
  return atoi(string);
}

