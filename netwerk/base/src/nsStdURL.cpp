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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIIOService.h"
#include "nsURLHelper.h"
#include "nsStdURL.h"
#include "nsStdURLParser.h"
#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "prmem.h"
#include "prprf.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsILocalFile.h"
#include "nsEscape.h"

#ifdef XP_PC
#include <windows.h>
#endif

static NS_DEFINE_CID(kStdURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kThisStdURLImplementationCID,
                     NS_THIS_STANDARDURL_IMPLEMENTATION_CID);
static NS_DEFINE_CID(kStdURLParserCID, NS_STANDARDURLPARSER_CID);

#if defined (XP_MAC)
void SwapSlashColon(char * s)
{
	while (*s)
	{
		if (*s == '/')
			*s++ = ':';
		else if (*s == ':')
			*s++ = '/';
		else
			*s++;
	}
} 
#endif

nsStdURL::nsStdURL()
    : mScheme(nsnull),
      mUsername(nsnull),
      mPassword(nsnull),
      mHost(nsnull),
      mPort(-1),
      mDirectory(nsnull),
      mFileBaseName(nsnull),
      mFileExtension(nsnull),
      mParam(nsnull),
      mQuery(nsnull),
      mRef(nsnull)
{
    NS_INIT_REFCNT();
    /* Create the standard URLParser */
    nsComponentManager::CreateInstance(kStdURLParserCID, 
                                       nsnull, NS_GET_IID(nsIURLParser),
                                       getter_AddRefs(mURLParser));

}

nsStdURL::nsStdURL(const char* i_Spec, nsISupports* outer)
    : mScheme(nsnull),
      mUsername(nsnull),
      mPassword(nsnull),
      mHost(nsnull),
      mPort(-1),
      mDirectory(nsnull),
      mFileBaseName(nsnull),
      mFileExtension(nsnull),
      mParam(nsnull),
      mQuery(nsnull),
      mRef(nsnull)
{
    NS_INIT_REFCNT();

    NS_INIT_AGGREGATED(outer);

    char* eSpec = nsnull;
    nsresult rv = DupString(&eSpec,i_Spec);
    if (NS_SUCCEEDED(rv)){

        // Skip leading spaces and control-characters
        char* fwdPtr= (char*) eSpec;
        while (fwdPtr && (*fwdPtr > '\0') && (*fwdPtr <= ' '))
            fwdPtr++;
        // Remove trailing spaces and control-characters
        if (fwdPtr) {
            char* bckPtr= (char*)fwdPtr + PL_strlen(fwdPtr) -1;
            if (*bckPtr > '\0' && *bckPtr <= ' ') {
                while ((bckPtr-fwdPtr) >= 0 && (*bckPtr <= ' ')) {
                    bckPtr--;
                }
                *(bckPtr+1) = '\0';
            }
        }

        /* Create the standard URLParser */
        nsComponentManager::CreateInstance(kStdURLParserCID, 
                                           nsnull, NS_GET_IID(nsIURLParser),
                                           getter_AddRefs(mURLParser));

        if (fwdPtr && mURLParser)
            Parse((char*)fwdPtr);
    }
    CRTFREEIF(eSpec);
}

nsStdURL::nsStdURL(const nsStdURL& otherURL)
    : mPort(otherURL.mPort)
{
    NS_INIT_REFCNT();

    mScheme = otherURL.mScheme ? nsCRT::strdup(otherURL.mScheme) : nsnull;
    mUsername = otherURL.mUsername ? nsCRT::strdup(otherURL.mUsername) : nsnull;
    mPassword = otherURL.mPassword ? nsCRT::strdup(otherURL.mPassword) : nsnull;
    mHost = otherURL.mHost ? nsCRT::strdup(otherURL.mHost) : nsnull;
    mDirectory = otherURL.mDirectory ? nsCRT::strdup(otherURL.mDirectory) : nsnull;
    mFileBaseName = otherURL.mFileBaseName ? nsCRT::strdup(otherURL.mFileBaseName) : nsnull;
    mFileExtension = otherURL.mFileExtension ? nsCRT::strdup(otherURL.mFileExtension) : nsnull;
    mParam = otherURL.mParam ? nsCRT::strdup(otherURL.mParam) : nsnull;
    mQuery = otherURL.mQuery ? nsCRT::strdup(otherURL.mQuery) : nsnull;
    mRef= otherURL.mRef ? nsCRT::strdup(otherURL.mRef) : nsnull;
    mURLParser = otherURL.mURLParser;

    NS_INIT_AGGREGATED(nsnull); // Todo! How?
}

nsStdURL& 
nsStdURL::operator=(const nsStdURL& otherURL)
{
    mScheme = otherURL.mScheme ? nsCRT::strdup(otherURL.mScheme) : nsnull;
    mUsername = otherURL.mUsername ? nsCRT::strdup(otherURL.mUsername) : nsnull;
    mPassword = otherURL.mPassword ? nsCRT::strdup(otherURL.mPassword) : nsnull;
    mHost = otherURL.mHost ? nsCRT::strdup(otherURL.mHost) : nsnull;
    mDirectory = otherURL.mDirectory ? nsCRT::strdup(otherURL.mDirectory) : nsnull;
    mFileBaseName = otherURL.mFileBaseName ? nsCRT::strdup(otherURL.mFileBaseName) : nsnull;
    mFileExtension = otherURL.mFileExtension ? nsCRT::strdup(otherURL.mFileExtension) : nsnull;
    mParam = otherURL.mParam ? nsCRT::strdup(otherURL.mParam) : nsnull;
    mQuery = otherURL.mQuery ? nsCRT::strdup(otherURL.mQuery) : nsnull;
    mRef= otherURL.mRef ? nsCRT::strdup(otherURL.mRef) : nsnull;
    mURLParser = otherURL.mURLParser;

    NS_INIT_AGGREGATED(nsnull); // Todo! How?
    return *this;
}

PRBool
nsStdURL::operator==(const nsStdURL& otherURL) const
{
    PRBool retValue = PR_FALSE;
    ((nsStdURL*)(this))->Equals((nsIURI*)&otherURL,&retValue);
    return retValue;
}

nsStdURL::~nsStdURL()
{
    CRTFREEIF(mScheme);
    CRTFREEIF(mUsername);
    CRTFREEIF(mPassword);
    CRTFREEIF(mHost);
    CRTFREEIF(mDirectory);
    CRTFREEIF(mFileBaseName);
    CRTFREEIF(mFileExtension);
    CRTFREEIF(mParam);
    CRTFREEIF(mQuery);
    CRTFREEIF(mRef);
}

NS_IMPL_AGGREGATED(nsStdURL);

NS_IMETHODIMP
nsStdURL::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if(!aInstancePtr)
        return NS_ERROR_INVALID_POINTER;

    if (aIID.Equals(NS_GET_IID(nsISupports)))
        *aInstancePtr = GetInner();
    else if (aIID.Equals(kThisStdURLImplementationCID) ||   // used by Equals
            aIID.Equals(NS_GET_IID(nsIURL)) ||
            aIID.Equals(NS_GET_IID(nsIURI)) ||
            aIID.Equals(NS_GET_IID(nsIFileURL)))
        *aInstancePtr = NS_STATIC_CAST(nsIURL*, this);
     else {
        *aInstancePtr = nsnull;
          return NS_NOINTERFACE;
    }
    NS_ADDREF((nsISupports*)*aInstancePtr);
    return NS_OK;
}

NS_IMETHODIMP
nsStdURL::Equals(nsIURI *i_OtherURI, PRBool *o_Equals)
{
    PRBool eq = PR_FALSE;
    if (i_OtherURI) {
        nsXPIDLCString spec;
        nsXPIDLCString spec2;
        nsresult rv = i_OtherURI->GetSpec(getter_Copies(spec));
        if (NS_FAILED(rv)) return rv;
        rv = this->GetSpec(getter_Copies(spec2));
        if (NS_FAILED(rv)) return rv;
        eq = nsCAutoString(spec).Equals(spec2);
    }
    *o_Equals = eq;
    return NS_OK;
}

NS_IMETHODIMP
nsStdURL::Clone(nsIURI **o_URI)
{
    nsStdURL* url = new nsStdURL(*this); /// TODO check outer?
    if (url == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv= NS_OK;

    *o_URI = url;
    NS_ADDREF(url);
    return rv;
}

nsresult 
nsStdURL::GetURLParser(nsIURLParser* *aURLParser)
{
    *aURLParser = mURLParser.get();
    NS_ADDREF(*aURLParser);
    return NS_OK;
}

nsresult 
nsStdURL::SetURLParser(nsIURLParser* aURLParser)
{
    mURLParser = aURLParser;
    return NS_OK;
}

nsresult
nsStdURL::Parse(const char* i_Spec)
{
    // Main parser
    NS_PRECONDITION( (nsnull != i_Spec), "Parse called on empty url!");
    if (!i_Spec)
        return NS_ERROR_MALFORMED_URI;

    NS_PRECONDITION( (nsnull != mURLParser), "Parse called without parser!");
    if (!mURLParser) return NS_ERROR_NULL_POINTER;

    // Parse the spec
    char* ePath = nsnull;
    nsresult rv = mURLParser->ParseAtScheme(i_Spec, &mScheme, &mUsername, 
                                            &mPassword, &mHost, &mPort, 
                                            &ePath);
    if (NS_SUCCEEDED(rv)) {
        // Now parse the path
        rv = mURLParser->ParseAtDirectory(ePath, &mDirectory, &mFileBaseName, 
                                          &mFileExtension, &mParam, 
                                          &mQuery, &mRef);
    }
    CRTFREEIF(ePath);

    return rv;
}

nsresult
nsStdURL::GetString(char** result, char* fromEscapedStr, Format toFormat)
{
    // Given str "foo%20bar", gets "foo bar" if UNESCAPED
    nsresult rv = NS_OK;
    if (toFormat == UNESCAPED) {
        rv = nsURLUnescape(fromEscapedStr, result);
    } else
        rv = DupString(result, fromEscapedStr);
    return rv;
}

nsresult
nsStdURL::AppendString(nsCString& buffer, char * fromUnescapedStr,
                       Format toFormat, PRInt16 mask)
{
    // Given str "foo bar", appends "foo%20bar" to buffer if ESCAPED
    nsresult rv = NS_OK; 

    if (!fromUnescapedStr)
        return NS_ERROR_FAILURE;

    if (toFormat == ESCAPED) {
        rv = nsAppendURLEscapedString(buffer, fromUnescapedStr, mask);
    } else {
        buffer += fromUnescapedStr;
    }

    return rv;
}

nsresult
nsStdURL::AppendPreHost(nsCString& buffer, char* i_Username, 
                        char* i_Password, Format toFormat)
{
    nsresult rv = NS_OK;
    if (i_Username)
    {
        rv = AppendString(buffer,i_Username,ESCAPED,
                          nsIIOService::url_Username);
        if (NS_FAILED(rv))
            return rv;
        if (i_Password) 
        {
           buffer += ':';
           rv = AppendString(buffer,i_Password,ESCAPED,
                             nsIIOService::url_Password);
           if (NS_FAILED(rv))
               return rv;
        }
    }
    return rv;
}

nsresult
nsStdURL::AppendFileName(nsCString& buffer, char* i_FileBaseName, 
                         char* i_FileExtension, Format toFormat)
{
    nsresult rv = NS_OK;
    if (i_FileBaseName)
    {
        rv = AppendString(buffer,i_FileBaseName,ESCAPED,
                          nsIIOService::url_FileBaseName);
        if (NS_FAILED(rv))
            return rv;
    }
    if (i_FileExtension) 
    {
        buffer += '.';
        rv = AppendString(buffer,i_FileExtension,ESCAPED,
                          nsIIOService::url_FileExtension);
    }
    return rv;
}

nsresult
nsStdURL::GetSpec(char **o_Spec)
{
    nsresult rv = NS_OK;
    nsCAutoString finalSpec; // guaranteed to be singlebyte.
    if (mScheme)
    {
        rv = AppendString(finalSpec,mScheme,ESCAPED,nsIIOService::url_Scheme);
        finalSpec += "://";
    }

    rv = AppendPreHost(finalSpec,mUsername,mPassword,ESCAPED);
    if (mUsername)
    {
        finalSpec += "@";
    }

    if (mHost)
    {
        rv = AppendString(finalSpec,mHost,ESCAPED,nsIIOService::url_Host);
        if (-1 != mPort)
        {
            char* portBuffer = PR_smprintf(":%d", mPort);
            if (!portBuffer)
                return NS_ERROR_OUT_OF_MEMORY;
            finalSpec += portBuffer;
            PR_smprintf_free(portBuffer);
            portBuffer = 0;
        }
    }
    char* ePath = nsnull;
    rv = GetPath(&ePath);
    if NS_FAILED(rv) {
        CRTFREEIF(ePath);
        return rv;
    }

    if (ePath)
    {
        finalSpec += ePath;
    }
    *o_Spec = finalSpec.ToNewCString();
    CRTFREEIF(ePath);

    return (*o_Spec ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}

NS_METHOD
nsStdURL::Create(nsISupports *aOuter, 
    REFNSIID aIID, 
    void **aResult)
{
    if (!aResult)
         return NS_ERROR_INVALID_POINTER;

     if (aOuter && !aIID.Equals(NS_GET_IID(nsISupports)))
         return NS_ERROR_INVALID_ARG;

    nsStdURL* url = new nsStdURL(nsnull, aOuter);
    if (url == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = url->AggregatedQueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {
        delete url;
        return rv;
    }

    return rv;
}

NS_METHOD
nsStdURL::GetPreHost(char **o_PreHost)
{
    if (!o_PreHost)
        return NS_ERROR_NULL_POINTER;
    
    if (!mUsername) {
        *o_PreHost = nsnull;
    } else {
        nsCAutoString temp;

        nsresult rv = AppendPreHost(temp,mUsername,mPassword,ESCAPED);

        if (NS_FAILED(rv))
            return rv;

        *o_PreHost = temp.ToNewCString();
        if (!*o_PreHost)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStdURL::SetDirectory(const char* i_Directory)
{
    if (!i_Directory)
        return NS_ERROR_NULL_POINTER;
    
    if (mDirectory)
        nsCRT::free(mDirectory);

    nsCAutoString dir;
    if ('/' != *i_Directory)
        dir += "/";
    
    dir += i_Directory;

    // if the last slash is missing then attach it
    char* last = (char*)i_Directory+PL_strlen(i_Directory)-1;
    if ('/' != *last) {
        dir += "/";
    }

    CRTFREEIF(mDirectory);
    mDirectory = dir.ToNewCString();
    if (!mDirectory)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsStdURL::SetFileName(const char* i_FileName)
{
    if (!i_FileName)
        return NS_ERROR_NULL_POINTER;
    
    NS_PRECONDITION( (nsnull != mURLParser), "Parse called without parser!");
    if (!mURLParser) return NS_ERROR_NULL_POINTER;

    //If it starts with a / then everything is the path.
    if ('/' == *i_FileName) {
        return SetPath(i_FileName);
    }
 
    // Otherwise concatenate Directory and Filename and the call SetPath
    nsCAutoString dir;
    nsresult status = AppendString(dir,mDirectory,ESCAPED,
                                   nsIIOService::url_Directory);
    dir += i_FileName;
    char *eNewPath = dir.ToNewCString();
    if (!eNewPath) 
        return NS_ERROR_OUT_OF_MEMORY;
    status = SetPath(eNewPath);
    CRTFREEIF(eNewPath);
    return status;
}

NS_IMETHODIMP
nsStdURL::SetParam(const char* i_Param)
{
    CRTFREEIF(mParam);
    return DupString(&mParam, (i_Param && (*i_Param == ';')) ? 
                     (i_Param+1) : i_Param);
}

NS_IMETHODIMP
nsStdURL::SetQuery(const char* i_Query)
{
    CRTFREEIF(mQuery);
    return DupString(&mQuery, (i_Query && (*i_Query == '?')) ? 
                     (i_Query+1) : i_Query);
}

NS_IMETHODIMP
nsStdURL::SetRef(const char* i_Ref)
{
    CRTFREEIF(mRef);
    return DupString(&mRef, (i_Ref && (*i_Ref == '#')) ? 
                     (i_Ref+1) : i_Ref);
}

NS_IMETHODIMP
nsStdURL::SetRelativePath(const char* i_Relative)
{
    nsresult rv = NS_OK;
    nsCAutoString options;
    char* ref;
    char* query;
    char* file;
    char* i_Path;
    char* ePath = nsnull;

    if (!i_Relative)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION( (nsnull != mURLParser), "Parse called without parser!");
    if (!mURLParser) return NS_ERROR_NULL_POINTER;

    // Make sure that if there is a : its before other delimiters
    // If not then its an absolute case
    static const char delimiters[] = "/;?#:";
    char* brk = PL_strpbrk(i_Relative, delimiters);
    if (brk && (*brk == ':')) // This is an absolute case
    {
        rv = SetSpec((char*) i_Relative);
        return rv;
    }

    if (*i_Relative == '/' && *(i_Relative+1) != '\0' && 
        *(i_Relative+1) == '/') {
        CRTFREEIF(mUsername);
        CRTFREEIF(mPassword);
        CRTFREEIF(mHost);
        mPort = -1;
        rv = mURLParser->ParseAtPreHost((char*)i_Relative, &mUsername, 
                                        &mPassword, &mHost, &mPort, &ePath);
        if (NS_FAILED(rv))
            return rv;
        i_Path = ePath;
    } else {
        i_Path = (char*)i_Relative;
    } 

    char* eFileName = nsnull;

    switch (*i_Path) 
    {
        case '/':
            rv = SetPath((char*) i_Path);
            CRTFREEIF(ePath);
            return rv;

        case ';': 
            // Append to Filename add then call SetFileName
            rv = GetFileName(&eFileName);
            options = eFileName;
            CRTFREEIF(eFileName);
            options += (char*)i_Path;
            file = options.ToNewCString();
            rv = SetFileName(file);
            CRTFREEIF(ePath);
            return rv;

        case '?': 
            // check for ref part
            ref = PL_strrchr(i_Path, '#');
            if (!ref) {
                CRTFREEIF(mRef);
                rv = SetQuery((char*)i_Path);
                CRTFREEIF(ePath);
                return rv;
            } else {
                DupString(&query,nsnull);
                ExtractString((char*)i_Path, &query, 
                    (PL_strlen(i_Path)-(ref-i_Path)));
                CRTFREEIF(ePath);
                rv = SetQuery(query);
                CRTFREEIF(query);
                if (NS_FAILED(rv)) return rv;
                rv = SetRef(ref);
                return rv;
            }
            break;

        case '#':
            rv = SetRef((char*)i_Path);
            CRTFREEIF(ePath);
            return rv;

        default:
            rv = SetFileName((char*)i_Path);
            CRTFREEIF(ePath);
            return rv;
    }
}

NS_IMETHODIMP
nsStdURL::Resolve(const char *relativePath, char **result) 
{
    nsresult rv = NS_OK;

    if (!relativePath) return NS_ERROR_NULL_POINTER;

    // Make sure that if there is a : its before other delimiters
    // If not then its an absolute case
    static const char delimiters[] = "/;?#:";
    char* brk = PL_strpbrk(relativePath, delimiters);
    if (brk && (*brk == ':')) // This is an absolute case
    {
        rv = DupString(result, relativePath);
        char* path = PL_strstr(*result,"://");
        if (path) {
            path = PL_strstr((char*)(path+3),"/");
            if (path) 
                CoaleseDirs(path);
        }
        return rv;
    }

    nsCAutoString finalSpec; // guaranteed to be singlebyte.

    // This is another case of an almost absolute URL 
    if (*relativePath == '/' && *(relativePath+1) != '\0' && 
        *(relativePath+1) == '/') {

        if (mScheme)
        {
            rv = AppendString(finalSpec,mScheme,ESCAPED,
                              nsIIOService::url_Scheme);
            finalSpec += ":";
        }

        finalSpec += relativePath;
        *result = finalSpec.ToNewCString();
        if (*result) {
            char* path = PL_strstr(*result,"://");
            if (path) {
                path = PL_strstr((char*)(path+3),"/");
                if (path)
                    CoaleseDirs(path);
            }
            return NS_OK;
        } else
            return NS_ERROR_OUT_OF_MEMORY;
    } 

    const char *start = relativePath;

    if (mScheme)
    {
        rv = AppendString(finalSpec,mScheme,ESCAPED,nsIIOService::url_Scheme);
        finalSpec += "://";
    }

    rv = AppendPreHost(finalSpec,mUsername,mPassword,ESCAPED);
    if (mUsername)
    {
        finalSpec += '@';
    }

    if (mHost)
    {
        rv = AppendString(finalSpec,mHost,ESCAPED,nsIIOService::url_Host);
        if (-1 != mPort)
        {
            char* portBuffer = PR_smprintf(":%d", mPort);
            if (!portBuffer)
                return NS_ERROR_OUT_OF_MEMORY;
            finalSpec += portBuffer;
            PR_smprintf_free(portBuffer);
            portBuffer = 0;
        }
    }

    if (start) {
      switch (*start) 
      {
        case '/':
            finalSpec += (char*)start;
            break;
        case ';': 
            rv = AppendString(finalSpec,mDirectory,ESCAPED,
                              nsIIOService::url_Directory);
            rv = AppendFileName(finalSpec,mFileBaseName,mFileExtension,
                                ESCAPED);
            finalSpec += (char*)start;
            break;
        case '?': 
            rv = AppendString(finalSpec,mDirectory,ESCAPED,
                              nsIIOService::url_Directory);
            rv = AppendFileName(finalSpec,mFileBaseName,mFileExtension,
                                ESCAPED);
            if (mParam)
            {
                finalSpec += ';';
                rv = AppendString(finalSpec,mParam,ESCAPED,
                                  nsIIOService::url_Param);
            }
            finalSpec += (char*)start;
            break;
        case '#':
            rv = AppendString(finalSpec,mDirectory,ESCAPED,
                              nsIIOService::url_Directory);
            rv = AppendFileName(finalSpec,mFileBaseName,mFileExtension,
                                ESCAPED);
            if (mParam)
            {
                finalSpec += ';';
                rv = AppendString(finalSpec,mParam,ESCAPED,
                                  nsIIOService::url_Param);
            }
            if (mQuery)
            {
                finalSpec += '?';
                rv = AppendString(finalSpec,mQuery,ESCAPED,
                                  nsIIOService::url_Query);
            }
            finalSpec += (char*)start;
            break;
        default:
            rv = AppendString(finalSpec,mDirectory,ESCAPED,
                              nsIIOService::url_Directory);
            finalSpec += (char*)start;
      }
    }
    *result = finalSpec.ToNewCString();

    if (*result) {
        char* path = PL_strstr(*result,"://");
        if (path) {
            path = PL_strstr((char*)(path+3),"/");
            if (path)
                CoaleseDirs(path);
        }
        return NS_OK;
    } else
        return NS_ERROR_OUT_OF_MEMORY;
}

nsresult
nsStdURL::GetPath(char** o_Path)
{
    //Take all the elements of the path and construct it
    nsCAutoString path;
    nsresult rv = NS_OK;
    if (mDirectory)
    {
        rv = AppendString(path,mDirectory,ESCAPED,nsIIOService::url_Directory);
        if (NS_FAILED(rv))
            return rv;
    }

    rv = AppendFileName(path,mFileBaseName,mFileExtension, ESCAPED);
    if (NS_FAILED(rv))
        return rv;

    if (mParam)
    {
        path += ';';
        rv = AppendString(path,mParam,ESCAPED,nsIIOService::url_Param);
        if (NS_FAILED(rv))
            return rv;
    }
    if (mQuery)
    {
        path += '?';
        rv = AppendString(path,mQuery,ESCAPED,nsIIOService::url_Query);
        if (NS_FAILED(rv))
            return rv;
    }
    if (mRef)
    {
        path += '#';
        rv = AppendString(path,mRef,ESCAPED,nsIIOService::url_Ref);
        if (NS_FAILED(rv))
            return rv;
    }
    *o_Path = path.ToNewCString();
    return (*o_Path ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}

nsresult
nsStdURL::GetDirectory(char** o_Directory)
{
    nsCAutoString directory;
    nsresult rv = NS_OK;
    rv = AppendString(directory,mDirectory,ESCAPED,
                      nsIIOService::url_Directory);
    if (NS_FAILED(rv))
        return rv;
    *o_Directory = directory.ToNewCString();
    return (*o_Directory ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}

NS_METHOD
nsStdURL::SetSpec(const char* i_Spec)
{
    char* eSpec = nsnull;
    nsresult rv = DupString(&eSpec,i_Spec);
    if (NS_FAILED(rv)){
        CRTFREEIF(eSpec);
        return rv;
    }

    // Skip leading spaces and control-characters
    char* fwdPtr= (char*) eSpec;
    while (fwdPtr && (*fwdPtr > '\0') && (*fwdPtr <= ' '))
        fwdPtr++;
    // Remove trailing spaces and control-characters
    if (fwdPtr) {
        char* bckPtr= (char*)fwdPtr + PL_strlen(fwdPtr) -1;
        if (*bckPtr > '\0' && *bckPtr <= ' ') {
            while ((bckPtr-fwdPtr) >= 0 && (*bckPtr <= ' ')) {
                bckPtr--;
            }
            *(bckPtr+1) = '\0';
        }
    }

    // If spec is being rewritten clean up everything-
    CRTFREEIF(mScheme);
    CRTFREEIF(mUsername);
    CRTFREEIF(mPassword);
    CRTFREEIF(mHost);
    mPort = -1;
    CRTFREEIF(mDirectory);
    CRTFREEIF(mFileBaseName);
    CRTFREEIF(mFileExtension);
    CRTFREEIF(mParam);
    CRTFREEIF(mQuery);
    CRTFREEIF(mRef);
    rv = Parse(fwdPtr);
    CRTFREEIF(eSpec);
    return rv;
}

NS_METHOD
nsStdURL::SetPreHost(const char* i_PreHost)
{
    NS_PRECONDITION( (nsnull != mURLParser), "Parse called without parser!");
    if (!mURLParser) return NS_ERROR_NULL_POINTER;

    CRTFREEIF(mUsername);
    CRTFREEIF(mPassword);

    return mURLParser->ParsePreHost(i_PreHost,&mUsername,&mPassword);
}

NS_METHOD
nsStdURL::SetPath(const char* i_Path)
{
    NS_PRECONDITION( (nsnull != mURLParser), "Parse called without parser!");
    if (!mURLParser) return NS_ERROR_NULL_POINTER;

    CRTFREEIF(mDirectory);
    CRTFREEIF(mFileBaseName);
    CRTFREEIF(mFileExtension);
    CRTFREEIF(mParam);
    CRTFREEIF(mQuery);
    CRTFREEIF(mRef);

    return mURLParser->ParseAtDirectory((char*)i_Path, &mDirectory, 
                                        &mFileBaseName, 
                                        &mFileExtension, &mParam, 
                                        &mQuery, &mRef);
}

NS_METHOD
nsStdURL::GetFilePath(char **o_DirFile)
{
    if (!o_DirFile)
        return NS_ERROR_NULL_POINTER;

    nsresult rv = NS_OK;
    nsCAutoString temp;
    if (mDirectory)
    {
        rv = AppendString(temp,mDirectory,ESCAPED,nsIIOService::url_Directory);
    }

    rv = AppendFileName(temp,mFileBaseName,mFileExtension,ESCAPED);
    if (NS_FAILED(rv))
        return rv;

    *o_DirFile = temp.ToNewCString();
    if (!*o_DirFile)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_METHOD
nsStdURL::GetFileName(char **o_FileName)
{
    if (!o_FileName)
        return NS_ERROR_NULL_POINTER;

    nsresult rv = NS_OK;    
    if (mFileBaseName || mFileExtension) {
        nsCAutoString temp;

        rv = AppendFileName(temp,mFileBaseName,mFileExtension,ESCAPED);
        if (NS_FAILED(rv))
            return rv;

        *o_FileName = temp.ToNewCString();
        if (!*o_FileName)
            return NS_ERROR_OUT_OF_MEMORY;
    } else {
        *o_FileName = nsnull;
    }
    return NS_OK;
}

NS_METHOD
nsStdURL::SetFilePath(const char *filePath)
{
    return SetPath(filePath);
}

NS_IMETHODIMP
nsStdURL::GetFile(nsIFile * *aFile)
{
    nsresult rv = NS_OK;

    if (nsCRT::strcasecmp(mScheme, "file") != 0) {
        // if this isn't a file: URL, then we can't return an nsIFile
        return NS_ERROR_FAILURE;
    }

    NS_ASSERTION(mUsername == nsnull, "file: with mUsername");

	// we do not use the path cause it can contain the # char
    nsCAutoString path;
    if (mDirectory)
    {
        rv = AppendString(path,mDirectory,ESCAPED,nsIIOService::url_Directory);
    }

    rv = AppendFileName(path,mFileBaseName,mFileExtension,ESCAPED);

#ifdef XP_PC
    if (path.CharAt(2) == '|')
        path.SetCharAt(':', 2);

    // cut off the leading '/'
    if (path.CharAt(0) == '/')
        path.Cut(0, 1);
    else if (mHost) {
        // Deal with the case where the user type file://C|/ (2 slashes instead of 3).
        // In this case, the C| ends up as the host name (ugh).
        nsCAutoString host(mHost);
        PRInt32 len = host.Length();
        if (len == 2 && host.CharAt(1) == '|') {
             host.SetCharAt(':', 1);
             path.Insert(host, 0);
         }
    }
    path.ReplaceChar('/', '\\');
#endif

    nsUnescape((char*)path.GetBuffer());
#if defined( XP_MAC )
 	// Now Swap the / and colons to convert back to a mac path
    SwapSlashColon( (char*)path.GetBuffer() );
// and wack off leading :'s
    if (path.CharAt(0) == ':')
        path.Cut(0, 1);
#endif
    nsCOMPtr<nsILocalFile> localFile;
    rv = NS_NewLocalFile(path, getter_AddRefs(localFile));

    mFile = localFile;
	*aFile = mFile;
	NS_IF_ADDREF(*aFile);
    return rv;
}

NS_IMETHODIMP
nsStdURL::SetFile(nsIFile * aFile)
{
    nsresult rv;

    mFile = aFile;

    // set up this URL to denote the nsIFile
    SetScheme("file");
    CRTFREEIF(mUsername);
    CRTFREEIF(mPassword);
    CRTFREEIF(mHost);
    mPort = -1;

    char* ePath = nsnull;
    nsCAutoString escPath;

    rv = aFile->GetPath(&ePath);
    if (NS_SUCCEEDED(rv)) {
#if defined (XP_PC)
        // Replace \ with / to convert to an url
        char* s = ePath;
	    while (*s)
	    {
                    // We need to call IsDBCSLeadByte because
                    // Japanese windows can have 0x5C in the sencond byte 
                    // of a Japanese character, for example 0x8F 0x5C is
                    // one Japanese character
                    if(::IsDBCSLeadByte(*s) && *(s+1) != nsnull) {
                        s++;
		    } else if (*s == '\\')
			    *s = '/';
		    s++;
	    }
#endif
#if defined( XP_MAC )
   	    // Swap the / and colons to convert to an url
        SwapSlashColon(ePath);
#endif
        // Escape the path with the directory mask
        rv = nsURLEscape(ePath,nsIIOService::url_Directory+
                               nsIIOService::url_Forced,escPath);
        if (NS_SUCCEEDED(rv)) {
            rv = SetPath(escPath.GetBuffer());
        }
    }
    CRTFREEIF(ePath);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
