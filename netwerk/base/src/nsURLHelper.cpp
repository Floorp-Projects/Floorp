/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Andreas Otte.
 *
 * Contributor(s): 
 */
 
#include "nsURLHelper.h"
#include "prprf.h"
#include "nsCRT.h"
#include "nsIAllocator.h"

/* This array tells which chars has to be escaped */

const int EscapeChars[256] =
/*      0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F */
{
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 0x */
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  	    /* 1x */
        0,1023,   0, 512,   0,   0,1023,   0,1023,1023,1023,1023,1023,1023, 959,1016,       /* 2x   !"#$%&'()*+,-./	 */
     1023,1023,1023,1023,1023,1023,1023,1023,1023,1023, 896, 896,   0, 896,   0, 768,       /* 3x  0123456789:;<=>?	 */
      896,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,       /* 4x  @ABCDEFGHIJKLMNO  */
     1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023, 896, 896, 896, 896,1023,       /* 5x  PQRSTUVWXYZ[\]^_	 */
        0,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,       /* 6x  `abcdefghijklmno	 */
     1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023, 896,1012, 896,1008,   0,       /* 7x  pqrstuvwxyz{|}~	 */
        0    /* 8x  DEL               */
};

/* decode % escaped hex codes into character values
 */
#define UNHEX(C) \
((C >= '0' && C <= '9') ? C - '0' : \
((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))

/* check if char has to be escaped */
#define IS_OK(C) (EscapeChars[((unsigned int) (C))] & (mask))

/* HEX mask char */
#define HEX_ESCAPE '%'

/* returns an escaped string */
NS_NET nsresult 
nsURLEscape(const char* str, PRInt16 mask, char **result)
{
    if (!str) 
        return NS_OK;

    int i, extra = 0;
    char* hexChars = "0123456789ABCDEF";
    static const char CheckHexChars[] = "0123456789ABCDEFabcdef";
    int len = PL_strlen(str);

    register const unsigned char* src = (const unsigned char *) str;
    for (i = 0; i < len; i++)
	{
        if (!IS_OK(*src++)) {
            extra += 2; /* the escape, plus an extra byte for each nibble */
        }
	}

    *result = (char *)nsAllocator::Alloc(len + extra + 1);
    if (!*result)
        return NS_ERROR_OUT_OF_MEMORY;

    register unsigned char* dst = (unsigned char *) *result;
    src = (const unsigned char *) str;

    for (i = 0; i < len; i++)
    {
        const char* checker = (char*) src;
        unsigned char c = *src++;
        /* if the char has not to be escaped or whatever follows % is 
           a valid escaped string, just copy the char */
        if (IS_OK(c) || (c == HEX_ESCAPE && (checker+1) && (checker+2) &&
           PL_strpbrk(((checker+1)), CheckHexChars) != 0 &&  
           PL_strpbrk(((checker+2)), CheckHexChars) != 0))
            *dst++ = c;
        else 
            /* do the escape magic */
        {
            *dst++ = HEX_ESCAPE;
            *dst++ = hexChars[c >> 4];	/* high nibble */
            *dst++ = hexChars[c & 0x0f];	/* low nibble */
        }
	}
    *dst = '\0';
    return NS_OK;
}

/* returns an unescaped string */
NS_NET nsresult 
nsURLUnescape(char* str, char **result)
{
    if (!str) 
        return NS_OK;

    register char *src = str;
    static const char hexChars[] = "0123456789ABCDEF";
    int len = PL_strlen(str);

    *result = (char *)nsAllocator::Alloc(len + 1);
    if (!*result)
        return NS_ERROR_OUT_OF_MEMORY;

    src = str;
    register unsigned char* dst = (unsigned char *) *result;

    while (*src)
        /* check for valid escaped sequence */
        if (*src != HEX_ESCAPE || PL_strpbrk(((src+1)), hexChars) == 0 || 
            PL_strpbrk(((src+2)), hexChars) == 0 )
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

    *dst = '\0';
    return NS_OK;
}

/* extract portnumber from string */
NS_NET PRInt32 
ExtractPortFrom(const char* src)
{
    PRInt32 returnValue = -1;
    return (0 < PR_sscanf(src, "%d", &returnValue)) ? returnValue : -1;
}

/* extract string from other string */
NS_NET nsresult 
ExtractString(char* i_Src, char* *o_Dest, PRUint32 length)
{
    NS_PRECONDITION( (nsnull != i_Src), "Extract called on empty string!");
    CRTFREEIF(*o_Dest);
    *o_Dest = PL_strndup(i_Src, length);
    return (*o_Dest ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}

/* duplicate string */
NS_NET nsresult 
DupString(char* *o_Dest, const char* i_Src)
{
    if (!o_Dest)
        return NS_ERROR_NULL_POINTER;
    if (i_Src)
    {
        *o_Dest = nsCRT::strdup(i_Src);
        return (*o_Dest == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    else
    {
        *o_Dest = nsnull;
        return NS_OK;
    }
}

// Replace all /./ with a /
// Also changes all \ to / 
// But only till #?; 
NS_NET void 
CoaleseDirs(char* io_Path)
{
    /* Stolen from the old netlib's mkparse.c.
     *
     * modifies a url of the form   /foo/../foo1  ->  /foo1
     *                       and    /foo/./foo1   ->  /foo/foo1
     *                       and    /foo/foo1/..  ->  /foo/
     */
    char *fwdPtr = io_Path;
    char *urlPtr = io_Path;
    
    for(; (*fwdPtr != '\0') && 
            (*fwdPtr != ';') && 
            (*fwdPtr != '?') && 
            (*fwdPtr != '#'); ++fwdPtr)
    {
        if (*fwdPtr == '\\')
            *fwdPtr = '/';
        if (*fwdPtr == '/' && *(fwdPtr+1) == '.' && 
            (*(fwdPtr+2) == '/' || *(fwdPtr+2) == '\\'))
        {
            // remove . followed by slash or a backslash
            fwdPtr += 1;
        }
        else if(*fwdPtr == '/' && *(fwdPtr+1) == '.' && *(fwdPtr+2) == '.' && 
                (*(fwdPtr+3) == '/' || 
                    *(fwdPtr+3) == '\0' ||
                    *(fwdPtr+3) == ';' ||   // This will take care of likes of
                    *(fwdPtr+3) == '?' ||   //    foo/bar/..#sometag
                    *(fwdPtr+3) == '#' ||
                    *(fwdPtr+3) == '\\'))
        {
            // remove foo/.. 
            // reverse the urlPtr to the previous slash 
            if(urlPtr != io_Path) 
                urlPtr--; // we must be going back at least by one 
            for(;*urlPtr != '/' && urlPtr != io_Path; urlPtr--)
                ;  // null body 

            // forward the fwd_prt past the ../
            fwdPtr += 2;
            // special case if we have reached the end to preserve the last /
            if (*fwdPtr == '.' && *(fwdPtr+1) == '\0')
                urlPtr +=1;
        }
        else
        {
            // copy the url incrementaly 
            *urlPtr++ = *fwdPtr;
        }
    }
    // Copy remaining stuff past the #?;
    for (; *fwdPtr != '\0'; ++fwdPtr)
    {
        *urlPtr++ = *fwdPtr;
    }
    *urlPtr = '\0';  // terminate the url 

    /* 
     *  Now lets remove trailing . case
     *     /foo/foo1/.   ->  /foo/foo1/
     */

    if ((urlPtr > (io_Path+1)) && (*(urlPtr-1) == '.') && (*(urlPtr-2) == '/'))
        *(urlPtr-1) = '\0';
}
