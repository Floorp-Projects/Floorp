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
#include "nsEscape.h"

#if defined(XP_WIN)
#include <windows.h> // ::IsDBCSLeadByte need
#endif

/* helper call function */
nsresult
nsAppendURLEscapedString(nsCString& originalStr, const char* str, PRInt16 mask)
{
    nsCAutoString result;
    nsresult rv = nsStdEscape(str, mask, result);
    originalStr += result;
    return rv;
}

/* extracts first number from a string and assumes that this is the port number*/
PRInt32 
ExtractPortFrom(const char* src)
{
    // search for digits up to a slash or the string ends
    const char* port = src;
    PRInt32 returnValue = -1;

    // skip leading white space
    while (nsCRT::IsAsciiSpace(*port))
        port++;

    char c;
    while ((c = *port++) != '\0') {
        // stop if slash or ? or # reached
        if (c == '/' || c == '?' || c == '#')
            break;
        else if (!nsCRT::IsAsciiDigit(c))
            return returnValue;
    }
    return (0 < PR_sscanf(src, "%d", &returnValue)) ? returnValue : -1;
}

/* extract string from other string */
nsresult 
ExtractString(char* i_Src, char* *o_Dest, PRUint32 length)
{
    NS_PRECONDITION( (nsnull != i_Src), "Extract called on empty string!");
    CRTFREEIF(*o_Dest);
    *o_Dest = PL_strndup(i_Src, length);
    return (*o_Dest ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}

/* duplicate string */
nsresult 
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
// But only till #? 
void 
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
            (*fwdPtr != '?') && 
            (*fwdPtr != '#'); ++fwdPtr)
    {

#if defined(XP_WIN)
        // At first, If this is DBCS character, it skips next character.
        if (::IsDBCSLeadByte(*fwdPtr) && *(fwdPtr+1) != '\0') {
            *urlPtr++ = *fwdPtr++;
            *urlPtr++ = *fwdPtr;
            continue;
        }
#endif

        if (*fwdPtr == '/' && *(fwdPtr+1) == '.' && *(fwdPtr+2) == '/' )
        {
            // remove . followed by slash
            fwdPtr += 1;
        }
        else if(*fwdPtr == '/' && *(fwdPtr+1) == '.' && *(fwdPtr+2) == '.' && 
                (*(fwdPtr+3) == '/' || 
                    *(fwdPtr+3) == '\0' || // This will take care of 
                    *(fwdPtr+3) == '?' ||  // something like foo/bar/..#sometag
                    *(fwdPtr+3) == '#'))
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

void 
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
nsresult ExtractURLScheme(const char* inURI, PRUint32 *startPos, 
                                 PRUint32 *endPos, char* *scheme)
{
    // search for something up to a colon, and call it the scheme
    NS_ENSURE_ARG_POINTER(inURI);
    if (scheme)
       *scheme = nsnull;
    
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
