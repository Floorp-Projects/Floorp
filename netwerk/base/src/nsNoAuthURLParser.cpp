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

#include "nsNoAuthURLParser.h"
#include "nsURLHelper.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "prprf.h"
#include "prnetdb.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsNoAuthURLParser, nsIURLParser)

nsNoAuthURLParser::~nsNoAuthURLParser()
{
}


NS_METHOD
nsNoAuthURLParser::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsNoAuthURLParser* p = new nsNoAuthURLParser();
    if (p == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(p);
    nsresult rv = p->QueryInterface(aIID, aResult);
    NS_RELEASE(p);
    return rv;
}

nsresult
nsNoAuthURLParser::ParseAtScheme(const char* i_Spec, char* *o_Scheme, 
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

    static const char delimiters[] = ":"; //this order is optimized.
    char* brk = PL_strpbrk(i_Spec, delimiters);

    if (!brk) // everything is a path
    {
        rv = ParseAtPath((char*)i_Spec, o_Path);
        return rv;
    }

    switch (*brk)
    {
    case ':' :
        rv = ExtractString((char*)i_Spec, o_Scheme, (brk - i_Spec));
        if (NS_FAILED(rv))
            return rv;
        ToLowerCase(*o_Scheme);
        rv = ParseAtPreHost(brk+1, o_Username, o_Password, o_Host,
                            o_Port, o_Path);
        return rv;
        break;
    default:
        NS_ASSERTION(0, "This just can't be!");
        break;
    }
    return NS_OK;
}

nsresult
nsNoAuthURLParser::ParseAtPreHost(const char* i_Spec, char* *o_Username, 
                                  char* *o_Password, char* *o_Host,
                                  PRInt32 *o_Port, char* *o_Path)
{
    nsresult rv = NS_OK;
    // Skip leading two slashes if possible
    char* fwdPtr= (char*) i_Spec;
    if (fwdPtr) {
        if (*fwdPtr != '\0')
            if (*fwdPtr != '/')
                // No Host, go directly to path
                return ParseAtPath(fwdPtr,o_Path);
            else
                // Move over the slash
                fwdPtr++;
        if (*fwdPtr != '\0') 
            if (*fwdPtr != '/')
                // No Host, go directly to path
                return ParseAtPath(fwdPtr,o_Path);
            else
                // Move over the slash
                fwdPtr++;
    }
    // There maybe is a host
    rv = ParseAtHost(fwdPtr, o_Host, o_Port, o_Path);
    return rv;

}

nsresult
nsNoAuthURLParser::ParseAtHost(const char* i_Spec, char* *o_Host,
                     PRInt32 *o_Port, char* *o_Path)
{
    // Usually there is no Host, but who knows ...
    nsresult rv = NS_OK;

    PRInt32 len = PL_strlen(i_Spec);
    if ((*i_Spec == '/') 
#ifdef XP_PC
        // special case for XP_PC: look for a drive
       || (len > 1 && (*(i_Spec+1) == ':' || *(i_Spec+1) == '|')) 
#endif
       ){
        // No host, okay ...
        rv = ParseAtPath(i_Spec, o_Path);
    } else {
        // There seems be a host ...
        if (len > 1 && *i_Spec == '[') {
            // Possible IPv6 address
            PRNetAddr netaddr;
            char* fwdPtr = PL_strchr(i_Spec+1, ']');
            if (fwdPtr) {
                rv = ExtractString((char*)i_Spec+1, o_Host, 
                                   (fwdPtr - i_Spec - 1));
                if (NS_FAILED(rv)) return rv;
                rv = PR_StringToNetAddr(*o_Host, &netaddr);
                if (rv != PR_SUCCESS || netaddr.raw.family != PR_AF_INET6) {
                    // try something else
                    CRTFREEIF(*o_Host);
                }
            }
        }
        static const char delimiters[] = "/?#"; 
        char* brk = PL_strpbrk(i_Spec, delimiters);
        if (!brk) {
            // do we already have a host?
            if (!*o_Host) {
                // everything is the host
                rv = DupString(o_Host, i_Spec);
                if (NS_FAILED(rv)) return rv;
                ToLowerCase(*o_Host);
            }
            // parse after the host
            rv = ParseAtPath(i_Spec+len, o_Path);
        } else {
            // do we already have a host?
            if (!*o_Host) {
                rv = ExtractString((char*)i_Spec, o_Host, (brk - i_Spec));
                if (NS_FAILED(rv)) return rv;
                ToLowerCase(*o_Host);
            }
            // parse after the host
            rv = ParseAtPath(brk, o_Path);
        }
    }
    return rv;
}

nsresult
nsNoAuthURLParser::ParseAtPort(const char* i_Spec, PRInt32 *o_Port, char* *o_Path)
{
    nsresult rv = NS_OK;
    // There is no Port
    rv = ParseAtPath(i_Spec, o_Path);
    return NS_OK;
}

nsresult
nsNoAuthURLParser::ParseAtPath(const char* i_Spec, char* *o_Path)
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
nsNoAuthURLParser::ParseAtDirectory(const char* i_Path, char* *o_Directory, 
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
#ifdef XP_PC
        // if it contains only a drive then add a /
        if (PL_strlen(dirfile) == 3 && *dirfile == '/' && 
            (*(dirfile+2) == ':' || *(dirfile+2) == '|')) {
            nsCAutoString tempdir;
            tempdir += dirfile;
            tempdir += "/";
            CRTFREEIF(dirfile);
            dirfile = ToNewCString(tempdir);
            if (!dirfile) return NS_ERROR_OUT_OF_MEMORY;
        }
#if 0
        else {
          // if it is not beginning with a drive and 
          // there are no three slashes then add them and
          // take it as an unc path 
            if (PL_strlen(dirfile) < 3 ||
                !(*(dirfile+2) == ':' || *(dirfile+2) == '|')) {
                nsCAutoString tempdir;
                if (PL_strlen(dirfile) < 2 || (PL_strlen(dirfile) >= 2 &&
                   *(dirfile+1) != '/' && *(dirfile+1) != '\\')) {
                    tempdir += "/";
                }
                if (PL_strlen(dirfile) < 3 || (PL_strlen(dirfile) >= 3 &&
                   *(dirfile+2) != '/' && *(dirfile+2) != '\\')) {
                    tempdir += "/";
                }
                tempdir += dirfile;
                CRTFREEIF(dirfile);
                dirfile = ToNewCString(tempdir);
                if (!dirfile) return NS_ERROR_OUT_OF_MEMORY;
            }
        } 
#endif
#endif
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
nsNoAuthURLParser::ParsePreHost(const char* i_PreHost, char* *o_Username, 
                                char* *o_Password)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsNoAuthURLParser::ParseFileName(const char* i_FileName, 
                                 char* *o_FileBaseName, 
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
