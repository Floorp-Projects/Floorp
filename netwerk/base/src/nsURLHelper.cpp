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
#include "nsMemory.h"
#include "nsIIOService.h"
#include "nsIURI.h"


#if defined(XP_PC) && !defined(XP_OS2)
#include <windows.h> // ::IsDBCSLeadByte need
#endif

/* This array tells which chars have to be escaped */

const int EscapeChars[256] =
/*      0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F */
{
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 0x */
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  	    /* 1x */
        0,1023,   0, 512,1023,   0,1023,   0,1023,1023,1023,1023,1023,1023, 959, 912,       /* 2x   !"#$%&'()*+,-./	 */
     1023,1023,1023,1023,1023,1023,1023,1023,1023,1023, 912, 896,   0,1008,   0, 768,       /* 3x  0123456789:;<=>?	 */
      992,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,       /* 4x  @ABCDEFGHIJKLMNO  */
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

/* use the following masks to specify which 
   part of an URL you want to escape: 

   url_Scheme        =     1
   url_Username      =     2
   url_Password      =     4
   url_Host          =     8
   url_Directory     =    16
   url_FileBaseName  =    32
   url_FileExtension =    64
   url_Param         =   128
   url_Query         =   256
   url_Ref           =   512
*/

/* by default this function will not escape parts of a string
   that already look escaped, which means it already includes 
   a valid hexcode. This is done to avoid multiple escapes of
   a string. Use the following mask to force escaping of a 
   string:
 
   url_Forced        =  1024
*/
NS_NET nsresult 
nsURLEscape(const char* str, PRInt16 mask, nsCString &result)
{
    if (!str) {
        result = "";
        return NS_OK;
    }

    int i = 0;
    char* hexChars = "0123456789ABCDEF";
    static const char CheckHexChars[] = "0123456789ABCDEFabcdef";
    int len = PL_strlen(str);
    PRBool forced = PR_FALSE;

    if (mask & nsIIOService::url_Forced)
        forced = PR_TRUE;

    register const unsigned char* src = (const unsigned char *) str;

    src = (const unsigned char *) str;

    char tempBuffer[100];
    unsigned int tempBufferPos = 0;

    char c1[] = " ";
    char c2[] = " ";
    char* const pc1 = c1;
    char* const pc2 = c2;

    for (i = 0; i < len; i++)
    {
      c1[0] = *(src+1);
      if (*(src+1) == '\0') 
          c2[0] = '\0';
      else
          c2[0] = *(src+2);
      unsigned char c = *src++;

      /* if the char has not to be escaped or whatever follows % is 
         a valid escaped string, just copy the char */
      if (IS_OK(c) || (c == HEX_ESCAPE && !(forced) && (pc1) && (pc2) &&
         PL_strpbrk(pc1, CheckHexChars) != 0 &&  
         PL_strpbrk(pc2, CheckHexChars) != 0)) {
		  tempBuffer[tempBufferPos++]=c;
      }
      else 
          /* do the escape magic */
      {
          tempBuffer[tempBufferPos++] = HEX_ESCAPE;
          tempBuffer[tempBufferPos++] = hexChars[c >> 4];	/* high nibble */
          tempBuffer[tempBufferPos++] = hexChars[c & 0x0f]; /* low nibble */
      }
 	  
      if(tempBufferPos >= sizeof(tempBuffer) - 4)
 	  {
          tempBuffer[tempBufferPos] = '\0';
          result += tempBuffer;
          tempBufferPos = 0;
 	  }
	}
 	
    tempBuffer[tempBufferPos] = '\0';
    result += tempBuffer;
    return NS_OK;
}

/* helper call function */
NS_NET nsresult
nsAppendURLEscapedString(nsCString& originalStr, const char* str, PRInt16 mask)
{
	return(nsURLEscape(str, mask, originalStr));
}

/* returns an unescaped string */
NS_NET nsresult 
nsURLUnescape(char* str, char **result)
{
    if (!str) {
        *result = nsnull;
        return NS_OK;
    }
    register char *src = str;
    static const char hexChars[] = "0123456789ABCDEFabcdef";
    int len = PL_strlen(str);

    *result = (char *)nsMemory::Alloc(len + 1);
    if (!*result)
        return NS_ERROR_OUT_OF_MEMORY;

    register unsigned char* dst = (unsigned char *) *result;

    char c1[] = " ";
    char c2[] = " ";
    char* const pc1 = c1;
    char* const pc2 = c2;

    while (*src) {

        c1[0] = *(src+1);
        if (*(src+1) == '\0') 
            c2[0] = '\0';
        else
            c2[0] = *(src+2);

        /* check for valid escaped sequence */
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

#if defined(XP_PC) && !defined(XP_OS2)
        // At first, If this is DBCS character, it skips next character.
        if (::IsDBCSLeadByte(*fwdPtr) && *(fwdPtr+1) != '\0') {
            *urlPtr++ = *fwdPtr++;
            *urlPtr++ = *fwdPtr;
            continue;
        }

        if (*fwdPtr == '\\')
            *fwdPtr = '/';
#endif
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

NS_NET void 
ToLowerCase(char* str)
{
    if (str) {
        char* lstr = str;
        PRInt8 shift = 'a' - 'A';
        for(; (*lstr != '\0'); ++lstr)
        {
            // lowercase these 
            if ( (*(lstr) <= 'Z') && (*(lstr) >= 'A') )
                *(lstr) = *(lstr) + shift;
        }
    }
}

/* Extract URI-Scheme if possible */
NS_NET nsresult ExtractURLScheme(const char* inURI, PRUint32 *startPos, 
                                 PRUint32 *endPos, char* *scheme)
{
    // search for something up to a colon, and call it the scheme
    NS_ENSURE_ARG_POINTER(inURI);

    const char* uri = inURI;

    // skip leading white space
    while (nsCRT::IsAsciiSpace(*uri))
        uri++;

    PRUint32 start = uri - inURI;
    if (startPos) {
        *startPos = start;
    }

    PRUint32 length = 0;
    char c;
    while ((c = *uri++) != '\0') {
        // First char must be Alpha
        if (length == 0 && nsCRT::IsAsciiAlpha(c)) {
            length++;
        } 
        // Next chars can be alpha + digit + some special chars
        else if (length > 0 && (nsCRT::IsAsciiAlpha(c) || 
                 nsCRT::IsAsciiDigit(c) || c == '+' || 
                 c == '.' || c == '-')) {
            length++;
        }
        // stop if colon reached but not as first char
        else if (c == ':' && length > 0) {
            if (endPos) {
                *endPos = start + length + 1;
            }

            if (scheme) {
                char* str = (char*)nsMemory::Alloc(length + 1);
                if (str == nsnull)
                    return NS_ERROR_OUT_OF_MEMORY;
                nsCRT::memcpy(str, &inURI[start], length);
                str[length] = '\0';
                *scheme = str;
            }
            return NS_OK;
        }
        else 
            break;
    }
    return NS_ERROR_MALFORMED_URI;
}

/* Convert the URI string to a known case (or nsIURI::UNKNOWN) */
NS_NET PRUint32 SchemeTypeFor(const char* i_scheme)
{
    // This order is a speculation on what gets used more (or needs more
    // help in optimizing) 
    if (!i_scheme)
        return nsIURI::UNKNOWN;
    if (0 == PL_strcasecmp("chrome", i_scheme))
        return nsIURI::ABOUT;
    else if (0 == PL_strcasecmp("resource", i_scheme))
        return nsIURI::CHROME;
    else if (0 == PL_strcasecmp("jar", i_scheme))
        return nsIURI::JAR;
    else if (0 == PL_strcasecmp("file", i_scheme))
        return nsIURI::FILE;
    else if (0 == PL_strcasecmp("http", i_scheme))
        return nsIURI::HTTP;
    else if (0 == PL_strcasecmp("ftp", i_scheme))
        return nsIURI::FTP;
    else if (0 == PL_strcasecmp("https", i_scheme))
        return nsIURI::HTTPS;
    else if (0 == PL_strcasecmp("mailbox", i_scheme))
        return nsIURI::MAILBOX;
    else if (0 == PL_strcasecmp("imap", i_scheme))
        return nsIURI::IMAP;
    else if (0 == PL_strcasecmp("javascript", i_scheme))
        return nsIURI::JAVASCRIPT;
    else if (0 == PL_strcasecmp("about", i_scheme))
        return nsIURI::RESOURCE;
    else if (0 == PL_strcasecmp("mailto", i_scheme))
        return nsIURI::MAILTO;
    else if (0 == PL_strcasecmp("news", i_scheme))
        return nsIURI::NEWS;
    else
        return nsIURI::UNKNOWN;
}
