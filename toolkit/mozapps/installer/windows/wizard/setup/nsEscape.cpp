/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Sean Su <ssu@netscape.com>
 *   Revision checked in on 01/03/30 by ssu@netscape.com, derived from:
 *     mozilla/xpcom/io/nsEscape.h
 *     mozilla/xpcom/io/nsEscape.cpp
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

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include "extern.h"
#include "extra.h"

#ifdef __cplusplus
}
#endif /* __cplusplus */

#include "nsEscape.h"

typedef int PRInt32;

const int netCharType[256] =
/*	Bit 0		xalpha		-- the alphas
**	Bit 1		xpalpha		-- as xalpha but 
**                             converts spaces to plus and plus to %20
**	Bit 3 ...	path		-- as xalphas but doesn't escape '/'
*/
    /*   0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 0x */
		 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 1x */
		 0,0,0,0,0,0,0,0,0,0,7,4,0,7,7,4,	/* 2x   !"#$%&'()*+,-./	 */
         7,7,7,7,7,7,7,7,7,7,0,0,0,7,0,0,	/* 3x  0123456789:;<=>?	 */
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
NS_COM char* nsEscape(const char * str, nsEscapeMask mask)
//----------------------------------------------------------------------------------------
{
    if(!str)
        return NULL;
    return nsEscapeCount(str, lstrlen(str), mask, NULL);
}

//----------------------------------------------------------------------------------------
NS_COM char* nsEscapeCount(
    const char * str,
    PRInt32 len,
    nsEscapeMask mask,
    PRInt32* out_len)
//----------------------------------------------------------------------------------------
{
	if (!str)
		return 0;

    int i, extra = 0;
    char* hexChars = "0123456789ABCDEF";

	register const unsigned char* src = (const unsigned char *) str;
    for (i = 0; i < len; i++)
	{
        if (!IS_OK(*src++))
            extra += 2; /* the escape, plus an extra byte for each nibble */
	}

	char* result = (char *)NS_GlobalAlloc(len + extra + 1);
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

    while (*src)
        if (*src != HEX_ESCAPE)
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

    *dst = 0;
    return (int)(dst - str);

} /* NET_UnEscapeCnt */

