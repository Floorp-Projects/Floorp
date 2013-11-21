/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
//	First checked in on 98/12/03 by John R. McMullen, derived from net.h/mkparse.c.

#include "nsEscape.h"
#include "nsMemory.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"

const int netCharType[256] =
/*	Bit 0		xalpha		-- the alphas
**	Bit 1		xpalpha		-- as xalpha but 
**                             converts spaces to plus and plus to %2B
**	Bit 3 ...	path		-- as xalphas but doesn't escape '/'
*/
    /*   0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 0x */
		 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 1x */
		 0,0,0,0,0,0,0,0,0,0,7,4,0,7,7,4,	/* 2x   !"#$%&'()*+,-./	 */
         7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,0,	/* 3x  0123456789:;<=>?	 */
	     0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,	/* 4x  @ABCDEFGHIJKLMNO  */
	     /* bits for '@' changed from 7 to 0 so '@' can be escaped   */
	     /* in usernames and passwords in publishing.                */
	     7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,7,	/* 5X  PQRSTUVWXYZ[\]^_	 */
	     0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,	/* 6x  `abcdefghijklmno	 */
	     7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,	/* 7X  pqrstuvwxyz{\}~	DEL */
		 0, };

/* decode % escaped hex codes into character values
 */
#define UNHEX(C) \
    ((C >= '0' && C <= '9') ? C - '0' : \
     ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
     ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))


#define IS_OK(C) (netCharType[((unsigned int) (C))] & (flags))
#define HEX_ESCAPE '%'

//----------------------------------------------------------------------------------------
static char* nsEscapeCount(
    const char * str,
    nsEscapeMask flags,
    size_t* out_len)
//----------------------------------------------------------------------------------------
{
	if (!str)
		return 0;

    size_t i, len = 0, charsToEscape = 0;
    static const char hexChars[] = "0123456789ABCDEF";

	const unsigned char* src = (const unsigned char *) str;
    while (*src)
	{
        len++;
        if (!IS_OK(*src++))
            charsToEscape++;
	}

    // calculate how much memory should be allocated
    // original length + 2 bytes for each escaped character + terminating '\0'
    // do the sum in steps to check for overflow
    size_t dstSize = len + 1 + charsToEscape;
    if (dstSize <= len)
	return 0;
    dstSize += charsToEscape;
    if (dstSize < len)
	return 0;

    // fail if we need more than 4GB
    // size_t is likely to be long unsigned int but nsMemory::Alloc(size_t)
    // calls NS_Alloc_P(size_t) which calls PR_Malloc(uint32_t), so there is
    // no chance to allocate more than 4GB using nsMemory::Alloc()
    if (dstSize > UINT32_MAX)
        return 0;

	char* result = (char *)nsMemory::Alloc(dstSize);
    if (!result)
        return 0;

    unsigned char* dst = (unsigned char *) result;
	src = (const unsigned char *) str;
	if (flags == url_XPAlphas)
	{
	    for (i = 0; i < len; i++)
		{
			unsigned char c = *src++;
			if (IS_OK(c))
				*dst++ = c;
			else if (c == ' ')
				*dst++ = '+'; /* convert spaces to pluses */
			else 
			{
				*dst++ = HEX_ESCAPE;
				*dst++ = hexChars[c >> 4];	/* high nibble */
				*dst++ = hexChars[c & 0x0f];	/* low nibble */
			}
		}
	}
	else
	{
	    for (i = 0; i < len; i++)
		{
			unsigned char c = *src++;
			if (IS_OK(c))
				*dst++ = c;
			else 
			{
				*dst++ = HEX_ESCAPE;
				*dst++ = hexChars[c >> 4];	/* high nibble */
				*dst++ = hexChars[c & 0x0f];	/* low nibble */
			}
		}
	}

    *dst = '\0';     /* tack on eos */
	if(out_len)
		*out_len = dst - (unsigned char *) result;
    return result;
}

//----------------------------------------------------------------------------------------
char* nsEscape(const char * str, nsEscapeMask flags)
//----------------------------------------------------------------------------------------
{
    if(!str)
        return nullptr;
    return nsEscapeCount(str, flags, nullptr);
}

//----------------------------------------------------------------------------------------
char* nsUnescape(char * str)
//----------------------------------------------------------------------------------------
{
	nsUnescapeCount(str);
	return str;
}

//----------------------------------------------------------------------------------------
int32_t nsUnescapeCount(char * str)
//----------------------------------------------------------------------------------------
{
    char *src = str;
    char *dst = str;
    static const char hexChars[] = "0123456789ABCDEFabcdef";

    char c1[] = " ";
    char c2[] = " ";
    char* const pc1 = c1;
    char* const pc2 = c2;

    while (*src)
    {
        c1[0] = *(src+1);
        if (*(src+1) == '\0') 
            c2[0] = '\0';
        else
            c2[0] = *(src+2);

        if (*src != HEX_ESCAPE || PL_strpbrk(pc1, hexChars) == 0 || 
                                  PL_strpbrk(pc2, hexChars) == 0 )
        	*dst++ = *src++;
        else 	
		{
        	src++; /* walk over escape */
        	if (*src)
            {
            	*dst = UNHEX(*src) << 4;
            	src++;
            }
        	if (*src)
            {
            	*dst = (*dst + UNHEX(*src));
            	src++;
            }
        	dst++;
        }
    }

    *dst = 0;
    return (int)(dst - str);

} /* NET_UnEscapeCnt */


char *
nsEscapeHTML(const char * string)
{
    char *rv = nullptr;
    /* XXX Hardcoded max entity len. The +1 is for the trailing null. */
    uint32_t len = strlen(string);
    if (len >= (UINT32_MAX / 6))
      return nullptr;

    rv = (char *)NS_Alloc( (6 * len) + 1 );
    char *ptr = rv;

    if(rv)
      {
        for(; *string != '\0'; string++)
          {
            if(*string == '<')
              {
                *ptr++ = '&';
                *ptr++ = 'l';
                *ptr++ = 't';
                *ptr++ = ';';
              }
            else if(*string == '>')
              {
                *ptr++ = '&';
                *ptr++ = 'g';
                *ptr++ = 't';
                *ptr++ = ';';
              }
            else if(*string == '&')
              {
                *ptr++ = '&';
                *ptr++ = 'a';
                *ptr++ = 'm';
                *ptr++ = 'p';
                *ptr++ = ';';
              }
            else if (*string == '"')
              {
                *ptr++ = '&';
                *ptr++ = 'q';
                *ptr++ = 'u';
                *ptr++ = 'o';
                *ptr++ = 't';
                *ptr++ = ';';
              }
            else if (*string == '\'')
              {
                *ptr++ = '&';
                *ptr++ = '#';
                *ptr++ = '3';
                *ptr++ = '9';
                *ptr++ = ';';
              }
            else
              {
                *ptr++ = *string;
              }
          }
        *ptr = '\0';
      }

    return(rv);
}

PRUnichar *
nsEscapeHTML2(const PRUnichar *aSourceBuffer, int32_t aSourceBufferLen)
{
  // Calculate the length, if the caller didn't.
  if (aSourceBufferLen < 0) {
    aSourceBufferLen = NS_strlen(aSourceBuffer);
  }

  /* XXX Hardcoded max entity len. */
  if (uint32_t(aSourceBufferLen) >=
      ((UINT32_MAX - sizeof(PRUnichar)) / (6 * sizeof(PRUnichar))) )
    return nullptr;

  PRUnichar *resultBuffer = (PRUnichar *)nsMemory::Alloc(aSourceBufferLen *
                            6 * sizeof(PRUnichar) + sizeof(PRUnichar('\0')));
  PRUnichar *ptr = resultBuffer;

  if (resultBuffer) {
    int32_t i;

    for(i = 0; i < aSourceBufferLen; i++) {
      if(aSourceBuffer[i] == '<') {
        *ptr++ = '&';
        *ptr++ = 'l';
        *ptr++ = 't';
        *ptr++ = ';';
      } else if(aSourceBuffer[i] == '>') {
        *ptr++ = '&';
        *ptr++ = 'g';
        *ptr++ = 't';
        *ptr++ = ';';
      } else if(aSourceBuffer[i] == '&') {
        *ptr++ = '&';
        *ptr++ = 'a';
        *ptr++ = 'm';
        *ptr++ = 'p';
        *ptr++ = ';';
      } else if (aSourceBuffer[i] == '"') {
        *ptr++ = '&';
        *ptr++ = 'q';
        *ptr++ = 'u';
        *ptr++ = 'o';
        *ptr++ = 't';
        *ptr++ = ';';
      } else if (aSourceBuffer[i] == '\'') {
        *ptr++ = '&';
        *ptr++ = '#';
        *ptr++ = '3';
        *ptr++ = '9';
        *ptr++ = ';';
      } else {
        *ptr++ = aSourceBuffer[i];
      }
    }
    *ptr = 0;
  }

  return resultBuffer;
}

//----------------------------------------------------------------------------------------

const int EscapeChars[256] =
/*      0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F */
{
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 0x */
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  	    /* 1x */
        0,1023,   0, 512,1023,   0,1023,   0,1023,1023,1023,1023,1023,1023, 953, 784,       /* 2x   !"#$%&'()*+,-./	 */
     1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1008,1008,   0,1008,   0, 768,       /* 3x  0123456789:;<=>?	 */
     1008,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,       /* 4x  @ABCDEFGHIJKLMNO  */
     1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023, 896, 896, 896, 896,1023,       /* 5x  PQRSTUVWXYZ[\]^_	 */
        0,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,       /* 6x  `abcdefghijklmno	 */
     1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023, 896,1012, 896,1023,   0,       /* 7x  pqrstuvwxyz{|}~	 */
        0    /* 8x  DEL               */
};

#define NO_NEED_ESC(C) (EscapeChars[((unsigned int) (C))] & (flags))

//----------------------------------------------------------------------------------------

/* returns an escaped string */

/* use the following flags to specify which 
   part of an URL you want to escape: 

   esc_Scheme        =     1
   esc_Username      =     2
   esc_Password      =     4
   esc_Host          =     8
   esc_Directory     =    16
   esc_FileBaseName  =    32
   esc_FileExtension =    64
   esc_Param         =   128
   esc_Query         =   256
   esc_Ref           =   512
*/

/* by default this function will not escape parts of a string
   that already look escaped, which means it already includes 
   a valid hexcode. This is done to avoid multiple escapes of
   a string. Use the following flags to force escaping of a 
   string:
 
   esc_Forced        =  1024
*/

bool NS_EscapeURL(const char *part,
                           int32_t partLen,
                           uint32_t flags,
                           nsACString &result)
{
    if (!part) {
        NS_NOTREACHED("null pointer");
        return false;
    }

    int i = 0;
    static const char hexChars[] = "0123456789ABCDEF";
    if (partLen < 0)
        partLen = strlen(part);
    bool forced = !!(flags & esc_Forced);
    bool ignoreNonAscii = !!(flags & esc_OnlyASCII);
    bool ignoreAscii = !!(flags & esc_OnlyNonASCII);
    bool writing = !!(flags & esc_AlwaysCopy);
    bool colon = !!(flags & esc_Colon);

    const unsigned char* src = (const unsigned char *) part;

    char tempBuffer[100];
    unsigned int tempBufferPos = 0;

    bool previousIsNonASCII = false;
    for (i = 0; i < partLen; i++)
    {
      unsigned char c = *src++;

      // if the char has not to be escaped or whatever follows % is 
      // a valid escaped string, just copy the char.
      //
      // Also the % will not be escaped until forced
      // See bugzilla bug 61269 for details why we changed this
      //
      // And, we will not escape non-ascii characters if requested.
      // On special request we will also escape the colon even when
      // not covered by the matrix.
      // ignoreAscii is not honored for control characters (C0 and DEL)
      //
      // And, we should escape the '|' character when it occurs after any
      // non-ASCII character as it may be part of a multi-byte character.
      //
      // 0x20..0x7e are the valid ASCII characters. We also escape spaces
      // (0x20) since they are not legal in URLs.
      if ((NO_NEED_ESC(c) || (c == HEX_ESCAPE && !forced)
                          || (c > 0x7f && ignoreNonAscii)
                          || (c > 0x20 && c < 0x7f && ignoreAscii))
          && !(c == ':' && colon)
          && !(previousIsNonASCII && c == '|' && !ignoreNonAscii))
      {
        if (writing)
          tempBuffer[tempBufferPos++] = c;
      }
      else /* do the escape magic */
      {
        if (!writing)
        {
          result.Append(part, i);
          writing = true;
        }
        tempBuffer[tempBufferPos++] = HEX_ESCAPE;
        tempBuffer[tempBufferPos++] = hexChars[c >> 4];	/* high nibble */
        tempBuffer[tempBufferPos++] = hexChars[c & 0x0f]; /* low nibble */
      }

      if (tempBufferPos >= sizeof(tempBuffer) - 4)
      {
        NS_ASSERTION(writing, "should be writing");
        tempBuffer[tempBufferPos] = '\0';
        result += tempBuffer;
        tempBufferPos = 0;
      }

      previousIsNonASCII = (c > 0x7f);
    }
    if (writing) {
      tempBuffer[tempBufferPos] = '\0';
      result += tempBuffer;
    }
    return writing;
}

#define ISHEX(c) memchr(hexChars, c, sizeof(hexChars)-1)

bool NS_UnescapeURL(const char *str, int32_t len, uint32_t flags, nsACString &result)
{
    if (!str) {
        NS_NOTREACHED("null pointer");
        return false;
    }

    if (len < 0)
        len = strlen(str);

    bool ignoreNonAscii = !!(flags & esc_OnlyASCII);
    bool ignoreAscii = !!(flags & esc_OnlyNonASCII);
    bool writing = !!(flags & esc_AlwaysCopy);
    bool skipControl = !!(flags & esc_SkipControl); 

    static const char hexChars[] = "0123456789ABCDEFabcdef";

    const char *last = str;
    const char *p = str;

    for (int i=0; i<len; ++i, ++p) {
        //printf("%c [i=%d of len=%d]\n", *p, i, len);
        if (*p == HEX_ESCAPE && i < len-2) {
            unsigned char *p1 = ((unsigned char *) p) + 1;
            unsigned char *p2 = ((unsigned char *) p) + 2;
            if (ISHEX(*p1) && ISHEX(*p2) && 
                ((*p1 < '8' && !ignoreAscii) || (*p1 >= '8' && !ignoreNonAscii)) &&
                !(skipControl && 
                  (*p1 < '2' || (*p1 == '7' && (*p2 == 'f' || *p2 == 'F'))))) {
                //printf("- p1=%c p2=%c\n", *p1, *p2);
                writing = true;
                if (p > last) {
                    //printf("- p=%p, last=%p\n", p, last);
                    result.Append(last, p - last);
                    last = p;
                }
                char u = (UNHEX(*p1) << 4) + UNHEX(*p2);
                //printf("- u=%c\n", u);
                result.Append(u);
                i += 2;
                p += 2;
                last += 3;
            }
        }
    }
    if (writing && last < str + len)
        result.Append(last, str + len - last);

    return writing;
}
