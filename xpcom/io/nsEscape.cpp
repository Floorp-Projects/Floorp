/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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


#define IS_OK(C) (netCharType[((unsigned int) (C))] & (mask))
#define HEX_ESCAPE '%'

//----------------------------------------------------------------------------------------
static char* nsEscapeCount(
    const char * str,
    PRInt32 len,
    nsEscapeMask mask,
    PRInt32* out_len)
//----------------------------------------------------------------------------------------
{
	if (!str)
		return 0;

    int i, extra = 0;
    static const char hexChars[] = "0123456789ABCDEF";

	register const unsigned char* src = (const unsigned char *) str;
    for (i = 0; i < len; i++)
	{
        if (!IS_OK(*src++))
            extra += 2; /* the escape, plus an extra byte for each nibble */
	}

	char* result = (char *)nsMemory::Alloc(len + extra + 1);
    if (!result)
        return 0;

    register unsigned char* dst = (unsigned char *) result;
	src = (const unsigned char *) str;
	if (mask == url_XPAlphas)
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
NS_COM char* nsEscape(const char * str, nsEscapeMask mask)
//----------------------------------------------------------------------------------------
{
    if(!str)
        return NULL;
    return nsEscapeCount(str, (PRInt32)strlen(str), mask, NULL);
}

//----------------------------------------------------------------------------------------
NS_COM char* nsUnescape(char * str)
//----------------------------------------------------------------------------------------
{
	nsUnescapeCount(str);
	return str;
}

//----------------------------------------------------------------------------------------
NS_COM PRInt32 nsUnescapeCount(char * str)
//----------------------------------------------------------------------------------------
{
    register char *src = str;
    register char *dst = str;
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


NS_COM char *
nsEscapeHTML(const char * string)
{
	/* XXX Hardcoded max entity len. The +1 is for the trailing null. */
	char *rv = (char *) nsMemory::Alloc(strlen(string) * 6 + 1);
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

NS_COM PRUnichar *
nsEscapeHTML2(const PRUnichar *aSourceBuffer, PRInt32 aSourceBufferLen)
{
  // if the caller didn't calculate the length
  if (aSourceBufferLen == -1) {
    aSourceBufferLen = nsCRT::strlen(aSourceBuffer); // ...then I will
  }

  /* XXX Hardcoded max entity len. */
  PRUnichar *resultBuffer = (PRUnichar *)nsMemory::Alloc(aSourceBufferLen *
                            6 * sizeof(PRUnichar) + sizeof(PRUnichar('\0')));
  PRUnichar *ptr = resultBuffer;

  if (resultBuffer) {
    PRInt32 i;

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
        0,1023,   0, 512,1023,   0,1023,1023,1023,1023,1023,1023,1023,1023, 953, 784,       /* 2x   !"#$%&'()*+,-./	 */
     1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1008, 912,   0,1008,   0, 768,       /* 3x  0123456789:;<=>?	 */
     1008,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,       /* 4x  @ABCDEFGHIJKLMNO  */
     1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023, 896, 896, 896, 896,1023,       /* 5x  PQRSTUVWXYZ[\]^_	 */
        0,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,       /* 6x  `abcdefghijklmno	 */
     1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023, 896,1012, 896,1023,   0,       /* 7x  pqrstuvwxyz{|}~	 */
        0    /* 8x  DEL               */
};

#define NO_NEED_ESC(C) (EscapeChars[((unsigned int) (C))] & (mask))

//----------------------------------------------------------------------------------------

/* returns an escaped string */

/* use the following masks to specify which 
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
   a string. Use the following mask to force escaping of a 
   string:
 
   esc_Forced        =  1024
*/

NS_COM PRBool NS_EscapeURL(const char *part,
                           PRInt32 partLen,
                           PRInt16 mask,
                           nsACString &result)
{
    if (!part) {
        NS_NOTREACHED("null pointer");
        return PR_FALSE;
    }

    int i = 0;
    static const char hexChars[] = "0123456789ABCDEF";
    if (partLen < 0)
        partLen = strlen(part);
    PRBool forced = (mask & esc_Forced);
    PRBool ignoreNonAscii = (mask & esc_OnlyASCII);
    PRBool ignoreAscii = (mask & esc_OnlyNonASCII);
    PRBool writing = (mask & esc_AlwaysCopy);
    PRBool colon = (mask & esc_Colon);

    register const unsigned char* src = (const unsigned char *) part;

    char tempBuffer[100];
    unsigned int tempBufferPos = 0;

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
      if ((NO_NEED_ESC(c) || (c == HEX_ESCAPE && !forced)
                          || (c > 0x7f && ignoreNonAscii)
                          || (c > 0x1f && c < 0x7f && ignoreAscii))
          && !(c == ':' && colon))
      {
        if (writing)
          tempBuffer[tempBufferPos++] = c;
      }
      else /* do the escape magic */
      {
        if (!writing)
        {
          result.Append(part, i);
          writing = PR_TRUE;
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
	}
    if (writing) {
      tempBuffer[tempBufferPos] = '\0';
      result += tempBuffer;
    }
    return writing;
}

#define ISHEX(c) memchr(hexChars, c, sizeof(hexChars)-1)

NS_COM PRBool NS_UnescapeURL(const char *str, PRInt32 len, PRInt16 flags, nsACString &result)
{
    if (!str) {
        NS_NOTREACHED("null pointer");
        return PR_FALSE;
    }

    if (len < 0)
        len = strlen(str);

    PRBool ignoreNonAscii = (flags & esc_OnlyASCII);
    PRBool ignoreAscii = (flags & esc_OnlyNonASCII);
    PRBool writing = (flags & esc_AlwaysCopy);
    PRBool skipControl = (flags & esc_SkipControl); 

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
                writing = PR_TRUE;
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
