/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsUrl.h"
#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "prmem.h"
#include "prprf.h"

static NS_DEFINE_CID(kTypicalUrlCID, NS_TYPICALURL_CID);

////////////////////////////////////////////////////////////////////////////////
// nsUrl methods:

nsUrl::nsUrl(nsISupports* outer)
    : mScheme(nsnull),
      mPreHost(nsnull),
      mHost(nsnull),
      mPort(-1),
      mPath(nsnull),
      mRef(nsnull),
      mQuery(nsnull),
      mSpec(nsnull)
{
    NS_INIT_AGGREGATED(outer);
}

nsUrl::~nsUrl()
{
    if (mScheme)        delete[] mScheme;
    if (mPreHost)       delete[] mPreHost;
    if (mHost)          delete[] mHost;
    if (mRef)           delete[] mRef;
    if (mQuery)         delete[] mQuery;
    if (mSpec)          delete[] mSpec;
}

nsresult
nsUrl::Init(const char* aSpec,
            nsIUrl* aBaseUrl)
{
    return Parse(aSpec, aBaseUrl);
}

NS_IMPL_AGGREGATED(nsUrl);

NS_IMETHODIMP
nsUrl::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(kTypicalUrlCID) ||  // used by Equals
        aIID.Equals(nsIUrl::GetIID()) ||
        aIID.Equals(nsISupports::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIUrl*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

////////////////////////////////////////////////////////////////////////////////
// nsIUrl methods:

NS_IMETHODIMP
nsUrl::GetScheme(const char* *result)
{
    *result = mScheme;
    return NS_OK;
}

NS_IMETHODIMP
nsUrl::SetScheme(const char* scheme)
{
    mScheme = nsCRT::strdup(scheme);
    return NS_OK;
}

NS_IMETHODIMP
nsUrl::GetPreHost(const char* *result)
{
    *result = mPreHost;
    return NS_OK;
}

NS_IMETHODIMP
nsUrl::SetPreHost(const char* preHost)
{
    mPreHost = nsCRT::strdup(preHost);
    return NS_OK;
}

NS_IMETHODIMP
nsUrl::GetHost(const char* *result)
{
    *result = mHost;
    return NS_OK;
}

NS_IMETHODIMP
nsUrl::SetHost(const char* host)
{
    mHost = nsCRT::strdup(host);
    return NS_OK;
}

NS_IMETHODIMP
nsUrl::GetPort(PRInt32 *result)
{
    *result = mPort;
    return NS_OK;
}

NS_IMETHODIMP
nsUrl::SetPort(PRInt32 port)
{
    mPort = port;
    return NS_OK;
}

NS_IMETHODIMP
nsUrl::GetPath(const char* *result)
{
    *result = mPath;
    return NS_OK;
}

NS_IMETHODIMP
nsUrl::SetPath(const char* path)
{
    mPath = nsCRT::strdup(path);
    return NS_OK;
}

NS_IMETHODIMP
nsUrl::Equals(nsIUrl* other)
{
    PRBool eq = PR_FALSE;
    if (other) {
//        NS_LOCK_INSTANCE();
        nsUrl* otherUrl;
        if (NS_SUCCEEDED(other->QueryInterface(kTypicalUrlCID, (void**)&otherUrl))) {
            eq = PRBool((0 == PL_strcmp(mScheme, otherUrl->mScheme)) && 
                        (0 == PL_strcasecmp(mHost, otherUrl->mHost)) &&
                        (mPort == otherUrl->mPort) &&
                        (0 == PL_strcmp(mPath, otherUrl->mPath)));
            NS_RELEASE(otherUrl);
        }
//        NS_UNLOCK_INSTANCE();
    }
    return eq;
}

NS_IMETHODIMP
nsUrl::Clone(nsIUrl* *result)
{
    nsUrl* url = new nsUrl(nsnull);     // XXX outer?
    if (url == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    url->mScheme = nsCRT::strdup(mScheme);
    if (url->mScheme == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    url->mPreHost = nsCRT::strdup(mPreHost);
    if (url->mPreHost == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    url->mHost = nsCRT::strdup(mHost);
    if (url->mHost == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    url->mPort = mPort;
    url->mPath = nsCRT::strdup(mPath);
    if (url->mPath == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    url->mRef = nsCRT::strdup(mRef);
    if (url->mRef == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    url->mQuery = nsCRT::strdup(mQuery);
    if (url->mQuery == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    url->mSpec = nsCRT::strdup(mSpec);
    if (url->mSpec == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsUrl::ToNewCString(const char* *result)
{
    nsAutoString string;
//    NS_LOCK_INSTANCE();

    // XXX Special-case javascript: URLs for the moment.
    // This code will go away when we actually start doing
    // protocol-specific parsing.
    if (PL_strcmp(mScheme, "javascript") == 0) {
        string.SetString(mSpec);
    } else if (PL_strcmp(mScheme, "about") == 0) { 
        string.SetString(mScheme);
        string.Append(':');
        string.Append(mPath);
    } else {
        string.SetLength(0);
        string.Append(mScheme);
        string.Append("://");
        if (nsnull != mHost) {
            string.Append(mHost);
            if (0 < mPort) {
                string.Append(':');
                string.Append(mPort, 10);
            }
        }
        string.Append(mPath);
        if (nsnull != mRef) {
            string.Append('#');
            string.Append(mRef);
        }
        if (nsnull != mQuery) {
            string.Append('?');
            string.Append(mQuery);
        }
    }
//    NS_UNLOCK_INSTANCE();
    *result = string.ToNewCString();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

// XXX recode to use nsString api's
// XXX don't bother with port numbers
// XXX don't bother with ref's
// XXX null pointer checks are incomplete
nsresult
nsUrl::Parse(const char* spec, nsIUrl* aBaseUrl)
{
    // XXX hack!
    nsString specStr(spec);

    const char* uProtocol = nsnull;
    const char* uHost = nsnull;
    const char* uFile = nsnull;
    PRInt32 uPort;
    if (nsnull != aBaseUrl) {
        nsresult rslt = aBaseUrl->GetScheme(&uProtocol);
        if (rslt != NS_OK) return rslt;
        rslt = aBaseUrl->GetHost(&uHost);
        if (rslt != NS_OK) return rslt;
        rslt = aBaseUrl->GetPath(&uFile);
        if (rslt != NS_OK) return rslt;
        rslt = aBaseUrl->GetPort(&uPort);
        if (rslt != NS_OK) return rslt;
    }

//    NS_LOCK_INSTANCE();

    PR_FREEIF(mScheme);
    PR_FREEIF(mHost);
    PR_FREEIF(mPath);
    PR_FREEIF(mRef);
    PR_FREEIF(mQuery);
    mPort = -1;

    if (nsnull == spec) {
        if (nsnull == aBaseUrl) {
//            NS_UNLOCK_INSTANCE();
            return NS_ERROR_ILLEGAL_VALUE;
        }
        mScheme = (nsnull != uProtocol) ? nsCRT::strdup(uProtocol) : nsnull;
        mHost = (nsnull != uHost) ? nsCRT::strdup(uHost) : nsnull;
        mPort = uPort;
        mPath = (nsnull != uFile) ? nsCRT::strdup(uFile) : nsnull;

//        NS_UNLOCK_INSTANCE();
        return NS_OK;
    }

    // Strip out reference and search info
    char* ref = strpbrk(spec, "#?");
    if (nsnull != ref) {
        char* search = nsnull;
        if ('#' == *ref) {
            search = PL_strchr(ref + 1, '?');
            if (nsnull != search) {
                *search++ = '\0';
            }

            PRIntn hashLen = nsCRT::strlen(ref + 1);
            if (0 != hashLen) {
                mRef = (char*) PR_Malloc(hashLen + 1);
                PL_strcpy(mRef, ref + 1);
            }      
        }
        else {
            search = ref + 1;
        }

        if (nsnull != search) {
            // The rest is the search
            PRIntn searchLen = nsCRT::strlen(search);
            if (0 != searchLen) {
                mQuery = (char*) PR_Malloc(searchLen + 1);
                PL_strcpy(mQuery, search);
            }      
        }

        // XXX Terminate string at start of reference or search
        *ref = '\0';
    }

    // The URL is considered absolute if and only if it begins with a
    // protocol spec. A protocol spec is an alphanumeric string of 1 or
    // more characters that is terminated with a colon.
    PRBool isAbsolute = PR_FALSE;
    const char* cp;
    const char* ap = spec;
    char ch;
    while (0 != (ch = *ap)) {
        if (((ch >= 'a') && (ch <= 'z')) ||
            ((ch >= 'A') && (ch <= 'Z')) ||
            ((ch >= '0') && (ch <= '9'))) {
            ap++;
            continue;
        }
        if ((ch == ':') && (ap - spec >= 2)) {
            isAbsolute = PR_TRUE;
            cp = ap;
            break;
        }
        break;
    }

    if (!isAbsolute) {
        // relative spec
        if (nsnull == aBaseUrl) {
//            NS_UNLOCK_INSTANCE();
            return NS_ERROR_ILLEGAL_VALUE;
        }

        // keep protocol and host
        mScheme = (nsnull != uProtocol) ? nsCRT::strdup(uProtocol) : nsnull;
        mHost = (nsnull != uHost) ? nsCRT::strdup(uHost) : nsnull;
        mPort = uPort;

        // figure out file name
        PRInt32 len = nsCRT::strlen(spec) + 1;
        if ((len > 1) && (spec[0] == '/')) {
            // Relative spec is absolute to the server
            mPath = nsCRT::strdup(spec);
        } else {
            if (spec[0] != '\0') {
                // Strip out old tail component and put in the new one
                char* dp = PL_strrchr(uFile, '/');
                if (!dp) {
//                    NS_UNLOCK_INSTANCE();
                    return NS_ERROR_ILLEGAL_VALUE;
                }
                PRInt32 dirlen = (dp + 1) - uFile;
                mPath = (char*) PR_Malloc(dirlen + len);
                PL_strncpy(mPath, uFile, dirlen);
                PL_strcpy(mPath + dirlen, spec);
            }
            else {
                mPath = nsCRT::strdup(uFile);
            }
        }

        /* Stolen from netlib's mkparse.c.
         *
         * modifies a url of the form   /foo/../foo1  ->  /foo1
         *                       and    /foo/./foo1   ->  /foo/foo1
         */
        char *fwdPtr = mPath;
        char *urlPtr = mPath;
    
        for(; *fwdPtr != '\0'; fwdPtr++)
        {
    
            if(*fwdPtr == '/' && *(fwdPtr+1) == '.' && *(fwdPtr+2) == '/')
            {
                /* remove ./ 
                 */	
                fwdPtr += 1;
            }
            else if(*fwdPtr == '/' && *(fwdPtr+1) == '.' && *(fwdPtr+2) == '.' && 
                    (*(fwdPtr+3) == '/' || *(fwdPtr+3) == '\0'))
            {
                /* remove foo/.. 
                 */	
                /* reverse the urlPtr to the previous slash
                 */
                if(urlPtr != mPath) 
                    urlPtr--; /* we must be going back at least one */
                for(;*urlPtr != '/' && urlPtr != mPath; urlPtr--)
                    ;  /* null body */
        
                /* forward the fwd_prt past the ../
                 */
                fwdPtr += 2;
            }
            else
            {
                /* copy the url incrementaly 
                 */
                *urlPtr++ = *fwdPtr;
            }
        }
    
        *urlPtr = '\0';  /* terminate the url */

        // Now that we've resolved the relative URL, we need to reconstruct
        // a URL spec from the components.
        ReconstructSpec();
    } else {
        // absolute spec

        PR_FREEIF(mSpec);
        PRInt32 slen = specStr.Length();
        mSpec = (char *) PR_Malloc(slen + 1);
        specStr.ToCString(mSpec, slen+1);

        // get protocol first
        PRInt32 plen = cp - spec;
        mScheme = (char*) PR_Malloc(plen + 1);
        PL_strncpy(mScheme, spec, plen);
        mScheme[plen] = 0;
        cp++;                               // eat : in protocol

        // skip over one, two or three slashes if it isn't about:
        if (nsCRT::strcmp(mScheme, "about") != 0) {
            if (*cp == '/') {
                cp++;
                if (*cp == '/') {
                    cp++;
                    if (*cp == '/') {
                        cp++;
                    }
                }
            } else {
//                NS_UNLOCK_INSTANCE();
                return NS_ERROR_ILLEGAL_VALUE;
            }
        }


#if defined(XP_UNIX) || defined (XP_MAC)
        // Always leave the top level slash for absolute file paths under Mac and UNIX.
        // The code above sometimes results in stripping all of slashes
        // off. This only happens when a previously stripped url is asked to be
        // parsed again. Under Win32 this is not a problem since file urls begin
        // with a drive letter not a slash. This problem show's itself when 
        // nested documents such as iframes within iframes are parsed.

        if (nsCRT::strcmp(mScheme, "file") == 0) {
            if (*cp != '/') {
                cp--;
            }
        }
#endif /* XP_UNIX */

        const char* cp0 = cp;
        if ((nsCRT::strcmp(mScheme, "resource") == 0) ||
            (nsCRT::strcmp(mScheme, "file") == 0) ||
            (nsCRT::strcmp(mScheme, "about") == 0)) {
            // resource/file url's do not have host names.
            // The remainder of the string is the file name
            PRInt32 flen = nsCRT::strlen(cp);
            mPath = (char*) PR_Malloc(flen + 1);
            PL_strcpy(mPath, cp);
      
#ifdef NS_WIN32
            if (nsCRT::strcmp(mScheme, "file") == 0) {
                // If the filename starts with a "x|" where is an single
                // character then we assume it's a drive name and change the
                // vertical bar back to a ":"
                if ((flen >= 2) && (mPath[1] == '|')) {
                    mPath[1] = ':';
                }
            }
#endif /* NS_WIN32 */
        } else {
            // Host name follows protocol for http style urls
            cp = PL_strpbrk(cp, "/:");
      
            if (nsnull == cp) {
                // There is only a host name
                PRInt32 hlen = nsCRT::strlen(cp0);
                mHost = (char*) PR_Malloc(hlen + 1);
                PL_strcpy(mHost, cp0);
            }
            else {
                PRInt32 hlen = cp - cp0;
                mHost = (char*) PR_Malloc(hlen + 1);
                PL_strncpy(mHost, cp0, hlen); 
                mHost[hlen] = 0;

                if (':' == *cp) {
                    // We have a port number
                    cp0 = cp+1;
                    cp = PL_strchr(cp, '/');
                    mPort = strtol(cp0, (char **)nsnull, 10);
                }
            }

            if (nsnull == cp) {
                // There is no file name
                // Set filename to "/"
                mPath = (char*) PR_Malloc(2);
                mPath[0] = '/';
                mPath[1] = 0;
            }
            else {
                // The rest is the file name
                PRInt32 flen = nsCRT::strlen(cp);
                mPath = (char*) PR_Malloc(flen + 1);
                PL_strcpy(mPath, cp);
            }
        }
    }

    // printf("protocol='%s' host='%s' file='%s'\n", mScheme, mHost, mPath);

//    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

void
nsUrl::ReconstructSpec()
{
    PR_FREEIF(mSpec);

    char portBuffer[10];
    if (-1 != mPort) {
        PR_snprintf(portBuffer, 10, ":%d", mPort);
    }
    else {
        portBuffer[0] = '\0';
    }

    PRInt32 plen = PL_strlen(mScheme) + PL_strlen(mHost) +
        PL_strlen(portBuffer) + PL_strlen(mPath) + 4;
    if (mRef) {
        plen += 1 + PL_strlen(mRef);
    }
    if (mQuery) {
        plen += 1 + PL_strlen(mQuery);
    }

    mSpec = (char *) PR_Malloc(plen + 1);
    if (PL_strcmp(mScheme, "about") == 0) {
        PR_snprintf(mSpec, plen, "%s:%s", mScheme, mPath);
    } else {
        PR_snprintf(mSpec, plen, "%s://%s%s%s", 
                    mScheme, ((nsnull != mHost) ? mHost : ""), portBuffer,
                    mPath);
    }

    if (mRef) {
        PL_strcat(mSpec, "#");
        PL_strcat(mSpec, mRef);
    }
    if (mQuery) {
        PL_strcat(mSpec, "?");
        PL_strcat(mSpec, mQuery);
    }
}

////////////////////////////////////////////////////////////////////////////////
