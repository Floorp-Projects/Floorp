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

#include "nsAuthURLParser.h"
#include "nsURLHelper.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "prprf.h"
#include "prnetdb.h" // IPv6 support

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAuthURLParser, nsIURLParser)

nsAuthURLParser::~nsAuthURLParser()
{
}


NS_METHOD
nsAuthURLParser::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsAuthURLParser* p = new nsAuthURLParser();
    if (p == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(p);
    nsresult rv = p->QueryInterface(aIID, aResult);
    NS_RELEASE(p);
    return rv;
}

nsresult
nsAuthURLParser::ParseAtScheme(const char* i_Spec, char* *o_Scheme, 
                               char* *o_Username, char* *o_Password, 
                               char* *o_Host, PRInt32 *o_Port, char* *o_Path)
{
    nsresult rv = NS_OK;

    NS_PRECONDITION( (nsnull != i_Spec), "Parse called on empty url!");
    if (!i_Spec)
        return NS_ERROR_MALFORMED_URI;

    int len = PL_strlen(i_Spec);
    if (len >= 2 && *i_Spec == '/' && *(i_Spec+1) == '/') // No Scheme
    {
        rv = ParseAtPreHost(i_Spec, o_Username, o_Password, o_Host, o_Port, 
                            o_Path);
        return rv;
    }

    static const char delimiters[] = "/:@?#["; //this order is optimized.
    char* brk = PL_strpbrk(i_Spec, delimiters);

    if (!brk) // everything is a host
    {
        rv = ExtractString((char*)i_Spec, o_Host, len);
        ToLowerCase(*o_Host);
        return rv;
    } else
        len = PL_strlen(brk);

    switch (*brk)
    {
    case '/' :
    case '?' :
    case '#' :
        // If the URL starts with a slash then everything is a path
        if (brk == i_Spec)
        {
            rv = ParseAtPath(brk, o_Path);
            return rv;
        }
        else // The first part is host, so its host/path
        {
            rv = ExtractString((char*)i_Spec, o_Host, (brk - i_Spec));
            if (NS_FAILED(rv))
                return rv;
            ToLowerCase(*o_Host);
            rv = ParseAtPath(brk, o_Path);
            return rv;
        }
        break;
    case ':' :
        if (len >= 2 && *(brk+1) == '/')  {
            // Standard http://... or malformed http:/...
            rv = ExtractString((char*)i_Spec, o_Scheme, (brk - i_Spec));
            if (NS_FAILED(rv))
                return rv;
            ToLowerCase(*o_Scheme);
            rv = ParseAtPreHost(brk+1, o_Username, o_Password, o_Host,
                                o_Port, o_Path);
            return rv;
        } else {
            // Could be host:port, so try conversion to number
            PRInt32 port = ExtractPortFrom(brk+1);
            if (port > 0)
            {
                rv = ExtractString((char*)i_Spec, o_Host, (brk - i_Spec));
                if (NS_FAILED(rv))
                    return rv;
                ToLowerCase(*o_Host);
                rv = ParseAtPort(brk+1, o_Port, o_Path);
                return rv;
            } else {
                // No, it's not a number try scheme:host...    
                rv = ExtractString((char*)i_Spec, o_Scheme, 
                                       (brk - i_Spec));
                if (NS_FAILED(rv))
                    return rv;
                ToLowerCase(*o_Scheme);
                rv = ParseAtPreHost(brk+1, o_Username, o_Password, o_Host,
                                    o_Port, o_Path);
                return rv;
            }
        }
        break;
    case '@' :
        rv = ParseAtPreHost(i_Spec, o_Username, o_Password, o_Host,
                            o_Port, o_Path);
        return rv;
        break;
    case '[' :
        if (brk == i_Spec) {
            rv = ParseAtHost(i_Spec, o_Host, o_Port, o_Path);
            if (rv != NS_ERROR_MALFORMED_URI) return rv;

            // Try something else
            CRTFREEIF(*o_Host);
            *o_Port = -1;
        }
        rv = ParseAtPath(i_Spec, o_Path);
        return rv;
    default:
        NS_ASSERTION(0, "This just can't be!");
        break;
    }
    return NS_OK;
}

nsresult
nsAuthURLParser::ParseAtPreHost(const char* i_Spec, char* *o_Username, 
                                char* *o_Password, char* *o_Host,
                                PRInt32 *o_Port, char* *o_Path)
{
    nsresult rv = NS_OK;
    // Skip leading two slashes
    char* fwdPtr= (char*) i_Spec;
    if (fwdPtr && (*fwdPtr != '\0') && (*fwdPtr == '/'))
        fwdPtr++;
    if (fwdPtr && (*fwdPtr != '\0') && (*fwdPtr == '/'))
        fwdPtr++;

    static const char delimiters[] = "/:@?#["; 
    char* brk = PL_strpbrk(fwdPtr, delimiters);
	char* brk2 = nsnull;

    if (!brk) 
    {
        rv = ParseAtHost(fwdPtr, o_Host, o_Port, o_Path);
        return rv;
    }

    char* e_PreHost = nsnull;
    switch (*brk)
    {
    case ':' :
         // this maybe the : of host:port or username:password
         // look if the next special char is @
         brk2 = PL_strpbrk(brk+1, delimiters);
 
         if (!brk2) 
         {
             rv = ParseAtHost(fwdPtr, o_Host, o_Port, o_Path);
             return rv;
         }
         switch (*brk2)
         {
         case '@' :
             rv = ExtractString(fwdPtr, &e_PreHost, (brk2 - fwdPtr));
             if (NS_FAILED(rv)) {
                 CRTFREEIF(e_PreHost);
                 return rv;
             }
             rv = ParsePreHost(e_PreHost,o_Username,o_Password);
             CRTFREEIF(e_PreHost);
             if (NS_FAILED(rv))
                 return rv;
 
             rv = ParseAtHost(brk2+1, o_Host, o_Port, o_Path);
             break;
         default:
             rv = ParseAtHost(fwdPtr, o_Host, o_Port, o_Path);
             return rv;
         }
         break;
	case '@' :
        rv = ExtractString(fwdPtr, &e_PreHost, (brk - fwdPtr));
        if (NS_FAILED(rv)) {
            CRTFREEIF(e_PreHost);
            return rv;
        }
        rv = ParsePreHost(e_PreHost,o_Username,o_Password);
        CRTFREEIF(e_PreHost);
        if (NS_FAILED(rv))
            return rv;

        rv = ParseAtHost(brk+1, o_Host, o_Port, o_Path);
        break; 
    case '[' :
        if (brk == fwdPtr) {
            rv = ParseAtHost(fwdPtr, o_Host, o_Port, o_Path);
            if (rv != NS_ERROR_MALFORMED_URI) return rv;

            // Try something else
            CRTFREEIF(*o_Host);
            *o_Port = -1;
        }
        rv = ParseAtPath(fwdPtr, o_Path);
        return rv;
    default:
        rv = ParseAtHost(fwdPtr, o_Host, o_Port, o_Path);
    }
    return rv;
}

nsresult
nsAuthURLParser::ParseAtHost(const char* i_Spec, char* *o_Host,
                             PRInt32 *o_Port, char* *o_Path)
{
    nsresult rv = NS_OK;

    int len = PL_strlen(i_Spec);
    static const char delimiters[] = ":/?#"; //this order is optimized.

    const char* fwdPtr= i_Spec;
    PRNetAddr netaddr;
 
    if (fwdPtr && *fwdPtr == '[') {
        // Possible IPv6 address
        fwdPtr = strchr(fwdPtr+1, ']');
        if (fwdPtr && (fwdPtr[1] == '\0' || strchr(delimiters, fwdPtr[1]))) {
            rv = ExtractString((char*)i_Spec+1, o_Host, (fwdPtr - i_Spec - 1));
            if (NS_FAILED(rv))
                return rv;
            rv = PR_StringToNetAddr(*o_Host, &netaddr);
            if (rv != PR_SUCCESS || netaddr.raw.family != PR_AF_INET6) {
                // try something else
                CRTFREEIF(*o_Host);
            } else {
                ToLowerCase(*o_Host);
                fwdPtr++;
                switch (*fwdPtr)
                {
                    case '\0': // everything is a host
                        return NS_OK;
                    case '/' :
                    case '?' :
                    case '#' :
                        rv = ParseAtPath(fwdPtr, o_Path);
                        return rv;
                    case ':' :
                        rv = ParseAtPort(fwdPtr+1, o_Port, o_Path);
                        return rv;
                    default:
                        NS_ASSERTION(0, "This just can't be!");
                        break;
                }
            }
        }
    }

    char* brk = PL_strpbrk(i_Spec, delimiters);
    if (!brk) // everything is a host
    {
        rv = ExtractString((char*)i_Spec, o_Host, len);
        if (PL_strlen(*o_Host)==0) {
            return NS_ERROR_MALFORMED_URI;
        }
        ToLowerCase(*o_Host);
        return rv;
    }

    switch (*brk)
    {
    case '/' :
    case '?' :
    case '#' :
        // Get the Host, the rest is Path
        rv = ExtractString((char*)i_Spec, o_Host, (brk - i_Spec));
        if (NS_FAILED(rv))
            return rv;
        if (PL_strlen(*o_Host)==0) {
            return NS_ERROR_MALFORMED_URI;
        }
        ToLowerCase(*o_Host);
        rv = ParseAtPath(brk, o_Path);
        return rv;
        break;
    case ':' :
        // Get the Host
        rv = ExtractString((char*)i_Spec, o_Host, (brk - i_Spec));
        if (NS_FAILED(rv))
            return rv;
        if (PL_strlen(*o_Host)==0) {
            return NS_ERROR_MALFORMED_URI;
        }
        ToLowerCase(*o_Host);
        rv = ParseAtPort(brk+1, o_Port, o_Path);
        return rv;
        break;
    default:
        NS_ASSERTION(0, "This just can't be!");
        break;
    }
    return NS_OK;
}

nsresult
nsAuthURLParser::ParseAtPort(const char* i_Spec, PRInt32 *o_Port, 
                             char* *o_Path)
{
    nsresult rv = NS_OK;
    static const char delimiters[] = "/?#"; //this order is optimized.
    char* brk = PL_strpbrk(i_Spec, delimiters);
    if (!brk) // everything is a Port
    {
        if (PL_strlen(i_Spec)==0) {
            *o_Port = -1;
            return NS_OK;
        } else {
            *o_Port = ExtractPortFrom(i_Spec);
            if (*o_Port <= 0)
                return NS_ERROR_MALFORMED_URI;
            else
                return NS_OK;
        }
    }

    char* e_Port = nsnull;
    switch (*brk)
    {
    case '/' :
    case '?' :
    case '#' :
        // Get the Port, the rest is Path
        rv = ExtractString((char*)i_Spec, &e_Port, brk-i_Spec);
        if (NS_FAILED(rv)) {
            CRTFREEIF(e_Port);
            return rv;
        }
        if (PL_strlen(e_Port)==0) {
            *o_Port = -1;
        } else {
            *o_Port = ExtractPortFrom(e_Port);
            if (*o_Port <= 0)
                return NS_ERROR_MALFORMED_URI;
        }
        CRTFREEIF(e_Port);
        rv = ParseAtPath(brk, o_Path);
        return rv;
        break;
    default:
        NS_ASSERTION(0, "This just can't be!");
        break;
    }
    return NS_OK;
}

nsresult
nsAuthURLParser::ParseAtPath(const char* i_Spec, char* *o_Path)
{
    // Just write the path and check for a starting /
    nsCAutoString dir;
    if ('/' != *i_Spec)
        dir += "/";
    
    dir += i_Spec;

    *o_Path = ToNewCString(dir);
    return (*o_Path ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}

nsresult
nsAuthURLParser::ParseAtDirectory(const char* i_Path, char* *o_Directory, 
                           char* *o_FileBaseName, char* *o_FileExtension,
                           char* *o_Param, char* *o_Query, char* *o_Ref)
{
    // Cleanout
    CRTFREEIF(*o_Directory);
    CRTFREEIF(*o_FileBaseName);
    CRTFREEIF(*o_FileExtension);
    CRTFREEIF(*o_Param);
    CRTFREEIF(*o_Query);
    CRTFREEIF(*o_Ref);

    nsresult rv = NS_OK;

    // Parse the Path into its components
    if (!i_Path) 
    {
        DupString(o_Directory, "/");
        return (o_Directory ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
    }

    char* dirfile = nsnull;
    char* options = nsnull;

    int len = PL_strlen(i_Path);

    /* Factor out the optionpart with ;?# */
    static const char delimiters[] = "?#"; // for query and ref
    char* brk = PL_strpbrk(i_Path, delimiters);
    char* pointer;
    if (!brk)
        pointer = (char*)i_Path + len;
    else
        pointer = brk;
    /* Now look for a param ; right of a / from pointer backward */
    while ((pointer-i_Path) >= 0) {
        pointer--;
        if (*pointer == ';') {
            brk = pointer;
        }
        if (*pointer == '/')
            break;
    }

    if (!brk) // Everything is just path and filename
    {
        DupString(&dirfile, i_Path); 
    } 
    else 
    {
        int dirfileLen = brk - i_Path;
        ExtractString((char*)i_Path, &dirfile, dirfileLen);
        len -= dirfileLen;
        ExtractString((char*)i_Path + dirfileLen, &options, len);
        brk = options;
    }

    /* now that we have broken up the path treat every part differently */
    /* first dir+file */

    char* file;

    int dlen = PL_strlen(dirfile);
    if (dlen == 0)
    {
        DupString(o_Directory, "/");
        file = dirfile;
    } else {
        CoaleseDirs(dirfile);
        // Get length again
        dlen = PL_strlen(dirfile);

        // First find the last slash
        file = PL_strrchr(dirfile, '/');
        if (!file) 
        {
            DupString(o_Directory, "/");
            file = dirfile;
        }

        // If its not the same as the first slash then extract directory
        if (file != dirfile)
        {
            ExtractString(dirfile, o_Directory, (file - dirfile)+1);
            if (*dirfile != '/') {
                nsCAutoString dir;
                dir += "/" ;
                dir += *o_Directory;
                CRTFREEIF(*o_Directory);
                *o_Directory = ToNewCString(dir);
            }
        } else {
            DupString(o_Directory, "/");
        }
    }

    /* Extract FileBaseName and FileExtension */
    if (dlen > 0) {
        // Look again if there was a slash
        char* slash = PL_strrchr(dirfile, '/');
        char* e_FileName = nsnull;
        if (slash) {
            if (dirfile+dlen-1>slash)
                ExtractString(slash+1, &e_FileName, dlen-(slash-dirfile+1));
        } else {
            // Use the full String as Filename
            ExtractString(dirfile, &e_FileName, dlen);
        } 

        rv = ParseFileName(e_FileName,o_FileBaseName,o_FileExtension);

        CRTFREEIF(e_FileName);
    }

    // Now take a look at the options. "#" has precedence over "?"
    // which has precedence over ";"
    if (options) {
        // Look for "#" first. Everything following it is in the ref
        brk = PL_strchr(options, '#');
        if (brk) {
            int pieceLen = len - (brk + 1 - options);
            ExtractString(brk+1, o_Ref, pieceLen);
            len -= pieceLen + 1;
            *brk = '\0';
        }

        // Now look for "?"
        brk = PL_strchr(options, '?');
        if (brk) {
            int pieceLen = len - (brk + 1 - options);
            ExtractString(brk+1, o_Query, pieceLen);
            len -= pieceLen + 1;
            *brk = '\0';
        }

        // Now look for ';'
        brk = PL_strchr(options, ';');
        if (brk) {
            int pieceLen = len - (brk + 1 - options);
            ExtractString(brk+1, o_Param, pieceLen);
            len -= pieceLen + 1;
            *brk = '\0';
        }
    }

    nsCRT::free(dirfile);
    nsCRT::free(options);
    return rv;
}

nsresult
nsAuthURLParser::ParsePreHost(const char* i_PreHost, char* *o_Username, 
                              char* *o_Password)
{
    nsresult rv = NS_OK;

    if (!i_PreHost) {
        *o_Username = nsnull;
        *o_Password = nsnull;
        return rv;
    }

    // Search for :
    static const char delimiters[] = ":"; 
    char* brk = PL_strpbrk(i_PreHost, delimiters);
    if (brk) 
    {
        rv = ExtractString((char*)i_PreHost, o_Username, (brk - i_PreHost));
        if (NS_FAILED(rv))
            return rv;
        rv = ExtractString(brk+1, o_Password, 
             (i_PreHost+PL_strlen(i_PreHost) - brk - 1));
    } else {
        CRTFREEIF(*o_Password);
        rv = DupString(o_Username, i_PreHost);
    }
    return rv;
}

nsresult
nsAuthURLParser::ParseFileName(const char* i_FileName, char* *o_FileBaseName, 
                               char* *o_FileExtension)
{
    nsresult rv = NS_OK;

    if (!i_FileName) {
        *o_FileBaseName = nsnull;
        *o_FileExtension = nsnull;
        return rv;
    }

    // Search for FileExtension
    // Search for last .
    // Ignore . at the beginning
    char* brk = PL_strrchr(i_FileName+1, '.');
    if (brk) 
    {
        rv = ExtractString((char*)i_FileName, o_FileBaseName, 
                          (brk - i_FileName));
        if (NS_FAILED(rv))
            return rv;
        rv = ExtractString(brk + 1, o_FileExtension, 
                          (i_FileName+PL_strlen(i_FileName) - brk - 1));
     } else {
         rv = DupString(o_FileBaseName, i_FileName);
     }
    return rv;
}
