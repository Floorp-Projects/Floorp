/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsStdURL.h"
#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "prmem.h"
#include "prprf.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kStdURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kThisStdURLImplementationCID,
                     NS_THIS_STANDARDURL_IMPLEMENTATION_CID);

//----------------------------------------

// Helper function to extract the port # from a string
// PR_sscanf handles spaces and non-digits correctly
static PRInt32 ExtractPortFrom(char* src)
{
    PRInt32 returnValue = -1;
    return (0 < PR_sscanf(src, "%d", &returnValue)) ? returnValue : -1;
}

// Replace all /./ with a /
// Also changes all \ to / 
// But only till #?; 
static void ReplaceMess(char* io_Path)
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



//----------------------------------------

class nsParsePath
{
public:
    nsParsePath(nsStdURL* i_URL): mURL(i_URL) {}
    virtual ~nsParsePath() {mURL->ParsePath();}
private:
    nsStdURL* mURL;
};

nsStdURL::nsStdURL(const char* i_Spec, nsISupports* outer)
    : mScheme(nsnull),
      mPreHost(nsnull),
      mHost(nsnull),
      mPort(-1),
      mPath(nsnull),
      mDirectory(nsnull),
      mFileName(nsnull),
      mParam(nsnull),
      mQuery(nsnull),
      mRef(nsnull)
{
    // Skip leading spaces
    char* fwdPtr= (char*) i_Spec;
    while (fwdPtr && (*fwdPtr != '\0') && (*fwdPtr == ' '))
        fwdPtr++;
    mSpec = fwdPtr ? nsCRT::strdup(fwdPtr) : nsnull;
    NS_INIT_AGGREGATED(outer);
    if (fwdPtr)
        Parse();
}

nsStdURL::nsStdURL(const nsStdURL& otherURL)
    : mPort(otherURL.mPort)
{
    mSpec = otherURL.mSpec ? nsCRT::strdup(otherURL.mSpec) : nsnull;
    mScheme = otherURL.mScheme ? nsCRT::strdup(otherURL.mScheme) : nsnull;
    mPreHost = otherURL.mPreHost ? nsCRT::strdup(otherURL.mPreHost) : nsnull;
    mHost = otherURL.mHost ? nsCRT::strdup(otherURL.mHost) : nsnull;
    mPath = otherURL.mPath ? nsCRT::strdup(otherURL.mPath) : nsnull;
    mDirectory = otherURL.mDirectory ? nsCRT::strdup(otherURL.mDirectory) : nsnull;
    mFileName = otherURL.mFileName ? nsCRT::strdup(otherURL.mFileName) : nsnull;
    mParam = otherURL.mParam ? nsCRT::strdup(otherURL.mParam) : nsnull;
    mQuery = otherURL.mQuery ? nsCRT::strdup(otherURL.mQuery) : nsnull;
    mRef= otherURL.mRef ? nsCRT::strdup(otherURL.mRef) : nsnull;
    NS_INIT_AGGREGATED(nsnull); // Todo! How?
}

nsStdURL& 
nsStdURL::operator=(const nsStdURL& otherURL)
{
    mSpec = otherURL.mSpec ? nsCRT::strdup(otherURL.mSpec) : nsnull;
    mScheme = otherURL.mScheme ? nsCRT::strdup(otherURL.mScheme) : nsnull;
    mPreHost = otherURL.mPreHost ? nsCRT::strdup(otherURL.mPreHost) : nsnull;
    mHost = otherURL.mHost ? nsCRT::strdup(otherURL.mHost) : nsnull;
    mPath = otherURL.mPath ? nsCRT::strdup(otherURL.mPath) : nsnull;
    mDirectory = otherURL.mDirectory ? nsCRT::strdup(otherURL.mDirectory) : nsnull;
    mFileName = otherURL.mFileName ? nsCRT::strdup(otherURL.mFileName) : nsnull;
    mParam = otherURL.mParam ? nsCRT::strdup(otherURL.mParam) : nsnull;
    mQuery = otherURL.mQuery ? nsCRT::strdup(otherURL.mQuery) : nsnull;
    mRef= otherURL.mRef ? nsCRT::strdup(otherURL.mRef) : nsnull;
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
    CRTFREEIF(mPreHost);
    CRTFREEIF(mHost);
    CRTFREEIF(mPath);
    CRTFREEIF(mRef);
    CRTFREEIF(mParam);
    CRTFREEIF(mQuery);
    CRTFREEIF(mSpec);
    CRTFREEIF(mDirectory);
    CRTFREEIF(mFileName);
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
            aIID.Equals(NS_GET_IID(nsIURI)))
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
        nsresult rv = i_OtherURI->GetSpec(getter_Copies(spec));
        if (NS_FAILED(rv)) return rv;
        eq = nsAutoString(spec).Equals(this->mSpec);
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
nsStdURL::Parse(void)
{

    NS_PRECONDITION( (nsnull != mSpec), "Parse called on empty url!");
    if (!mSpec)
        return NS_ERROR_MALFORMED_URI;

    // Parse the path into its individual elements 
    // when we are done from here.
    nsParsePath pp(this);

    // Leading spaces are now removed by SetSpec. 
    int len = PL_strlen(mSpec);
    static const char delimiters[] = "/:@?"; //this order is optimized.
    char* brk = PL_strpbrk(mSpec, delimiters);
    char* lastbrk = brk;
    if (!brk) // everything is a host
    {
        ExtractString(mSpec, &mHost, len);
        return NS_OK;
    }

    switch (*brk)
    {
    case '/' :
    case '?' :
        // If the URL starts with a slash then everything is a path
        if (brk == mSpec)
        {
            ExtractString(mSpec, &mPath, len);
            return NS_OK;
        }
        else // The first part is host, so its host/path
        {
            ExtractString(mSpec, &mHost, (brk - mSpec));
            ExtractString(brk, &mPath, (len - (brk - mSpec)));
            return NS_OK;
        }
        break;
    case ':' :
        if (*(brk+1) == '/') 
        {
            ExtractString(mSpec, &mScheme, (brk - mSpec));

            if (*(brk+2) == '/') // e.g. http://
            // If the first colon is followed by // then its definitely a spec
            {
                lastbrk = brk+3;
                brk = PL_strpbrk(lastbrk, delimiters);
                if (!brk) // everything else is a host, as in http://host
                {
                    ExtractString(lastbrk, &mHost, len - (lastbrk - mSpec));
                    return NS_OK;
                }
                switch (*brk)
                {
                    case '/' : // standard case- http://host/path
                    case '?' : // missing path cases
                        ExtractString(lastbrk, &mHost, (brk - lastbrk));
                        ExtractString(brk, &mPath, (len - (brk - mSpec)));

                        return NS_OK;
                        break;
                    case ':' : // http://user:... or http://host:...
                    {
                        // It could be http://user:pass@host/path 
                        // or http://host:port/path we find that by checking further...
                        char* nextbrk = PL_strpbrk(brk+1, delimiters);
                        if (!nextbrk) // http://host:port
                        {
                            ExtractString(lastbrk, &mHost, (brk-lastbrk));
                            mPort = ExtractPortFrom(brk+1);
                            if (mPort <= 0) 
                                return NS_ERROR_MALFORMED_URI;
                            else
                                return NS_OK;
                        }

                        switch (*nextbrk)
                        {
                            case '/': // http://host:port/path
                            case '?': // http://host:port?path
                                ExtractString(lastbrk, &mHost, 
                                    (brk-lastbrk));
                                mPort = ExtractPortFrom(brk+1);
                                if (mPort <= 0)
                                    return NS_ERROR_MALFORMED_URI;
                                ExtractString(nextbrk, &mPath, 
                                    len - (nextbrk-mSpec));
                                return NS_OK;
                                break;
                            case '@': // http://user:pass@host...
                                ExtractString(lastbrk, &mPreHost, 
                                    (nextbrk - lastbrk));

                                brk = PL_strpbrk(nextbrk+1, delimiters);
                                if (!brk) // its just http://user:pass@host
                                {
                                    ExtractString(nextbrk+1, &mHost, 
                                        len - (nextbrk+1 - mSpec));
                                    return NS_OK;
                                }

                                ExtractString(nextbrk+1, &mHost, 
                                    brk - (nextbrk+1));

                                switch (*brk)
                                {
                                    case '/': // http://user:pass@host/path
                                    case '?':
                                        ExtractString(brk, &mPath,
                                            len - (brk - mSpec));
                                        return NS_OK;
                                        break;
                                    case ':': // http://user:pass@host:port...
                                        lastbrk = brk+1;
                                        brk = PL_strpbrk(lastbrk, "/?");
                                        if (brk) // http://user:pass@host:port/path
                                        {
                                            mPort = ExtractPortFrom(lastbrk);
                                            if (mPort <= 0)
                                                return NS_ERROR_MALFORMED_URI;
                                            ExtractString(brk, &mPath, 
                                                len - (brk-mSpec));
                                            return NS_OK;
                                         }
                                         else   // http://user:pass@host:port
                                         {
                                            mPort = ExtractPortFrom(lastbrk);
                                            if (mPort <= 0)
                                                return NS_ERROR_MALFORMED_URI;
                                            return NS_OK;
                                         }
                                         break;
                                    default: NS_POSTCONDITION(0, "This just can't be!");
                                         break;
                                }
                                break;
                            case ':': // three colons!
                                return NS_ERROR_MALFORMED_URI;
                                break;
                            default: NS_POSTCONDITION(0, "This just can't be!");
                                break;
                            }
                        }
                        break;
                    case '@' : // http://user@host...
                        {
                            ExtractString(lastbrk, &mPreHost, 
                                (brk-lastbrk));
                            lastbrk = brk+1;
                            brk = PL_strpbrk(lastbrk, delimiters);
                            if (!brk) // its just http://user@host
                            {
                                ExtractString(lastbrk, &mHost, 
                                    len - (lastbrk - mSpec));
                                return NS_OK;
                            }
                            ExtractString(lastbrk, &mHost, 
                                (brk - lastbrk));
                            switch (*brk)
                            {
                                case ':' : // http://user@host:port...
                                    lastbrk = brk+1;
                                    brk = PL_strpbrk(lastbrk, "/?");
                                    if (brk)    // http://user@host:port/path
                                    {
                                        mPort = ExtractPortFrom(lastbrk);
                                        if (mPort <= 0)
                                            return NS_ERROR_MALFORMED_URI;
                                        ExtractString(brk, &mPath, 
                                            len - (brk-mSpec));
                                        return NS_OK;
                                    }
                                    else        // http://user@host:port
                                    {
                                        mPort = ExtractPortFrom(lastbrk);
                                        if (mPort <= 0)
                                            return NS_ERROR_MALFORMED_URI;
                                        return NS_OK;
                                    }
                                    break;
                                case '/' : // http://user@host/path
                                case '?' : // http://user@host?path
                                    ExtractString(brk, &mPath, 
                                        len - (brk - mSpec));
                                    return NS_OK;
                                    break;
                                case '@' :
                                    return NS_ERROR_MALFORMED_URI;
                                default : NS_POSTCONDITION(0, 
                                    "This just can't be!");
                                    break;
                            }
                        }
                        break;
                    default: NS_POSTCONDITION(0, "This just can't be!");
                        break;
                }
            }
            else // This is a no // path alone case like file:/path, 
                // there is never a prehost/host in this case.
            {
                ExtractString(brk+1, &mPath, len - (brk-mSpec+1)); 
                return NS_OK;
            }
        }
        else // scheme:host or host:port...
        {
            lastbrk = brk+1;

            if ((*lastbrk >= '0') && (*lastbrk <= '9')) //host:port...
            {
                ExtractString(mSpec, &mHost, (brk - mSpec));
                brk = PL_strpbrk(lastbrk, delimiters);
                if (!brk) // Everything else is just the port
                {
                    mPort = ExtractPortFrom(lastbrk);
                    if (mPort <= 0)
                        return NS_ERROR_MALFORMED_URI;
                    return NS_OK;
                }
                switch (*brk)
                {
                    case '/' : // The path, so its host:port/path
                    case '?' : // The path, so its host:port?path
                        mPort = ExtractPortFrom(lastbrk);
                        if (mPort <= 0)
                            return NS_ERROR_MALFORMED_URI;
                        ExtractString(brk, &mPath, len - (brk-mSpec));
                        return NS_OK;
                        break;
                    case ':' : 
                        return NS_ERROR_MALFORMED_URI;
                        break;
                    case '@' :
                        // This is a special case of user:pass@host... so 
                        // Cleanout our earliar knowledge of host
                        CRTFREEIF(mHost);

                        ExtractString(mSpec, &mPreHost, (brk-mSpec));
                        lastbrk = brk+1;
                        brk = PL_strpbrk(lastbrk, ":/?");
                    // its user:pass@host so everthing else is just the host
                        if (!brk)
                        {
                            ExtractString(lastbrk, &mHost, len - (lastbrk-mSpec));
                            return NS_OK;
                        }
                        ExtractString(lastbrk, &mHost, (brk-lastbrk));
                        if (*brk == ':') // user:pass@host:port...
                        {
                            lastbrk = brk+1;
                            brk = PL_strpbrk(lastbrk, "/?");
                            if (brk)    // user:pass@host:port/path
                            {
                                mPort = ExtractPortFrom(lastbrk);
                                if (mPort <= 0)
                                    return NS_ERROR_MALFORMED_URI;
                                ExtractString(brk, &mPath, len - (brk-mSpec));
                                return NS_OK;
                            }
                            else        // user:pass@host:port
                            {
                                mPort = ExtractPortFrom(lastbrk);
                                if (mPort <= 0)
                                    return NS_ERROR_MALFORMED_URI;
                                return NS_OK;
                            }
                        }
                        else // (*brk == '/') || (*brk == '?') 
                            // so user:pass@host/path
                        {
                            ExtractString(brk, &mPath, len - (brk - mSpec));
                            return NS_OK;
                        }
                        break;
                    default: NS_POSTCONDITION(0, "This just can't be!");
                        break;
                }
            }
            else // scheme:host...
            {
                ExtractString(mSpec, &mScheme, (brk - mSpec));
                brk = PL_strpbrk(lastbrk, delimiters);
                if (!brk) // its just scheme:host
                {
                    ExtractString(lastbrk, &mHost, len - (lastbrk-mSpec));
                    return NS_OK;
                }
                switch (*brk)
                {
                    case '/' : // The path, so its scheme:host/path
                    case '?' : // The path, so its scheme:host?path
                        ExtractString(lastbrk, &mHost, (brk-lastbrk));
                        ExtractString(brk, &mPath, len - (brk - mSpec));
                        return NS_OK;
                        break;
                    case '@' : // scheme:user@host...
                        ExtractString(lastbrk, &mPreHost, (brk-lastbrk));
                        lastbrk = brk+1;
                        brk = PL_strpbrk(lastbrk, delimiters);
                        if (!brk) // scheme:user@host only
                        {
                            ExtractString(lastbrk, &mHost, len - (lastbrk-mSpec));
                            return NS_OK;
                        }
                        ExtractString(lastbrk, &mHost, (brk - lastbrk));
                        switch (*brk)
                        {
                            case ':' : // scheme:user@host:port...
                                lastbrk = brk+1;
                                brk = PL_strpbrk(lastbrk, "/?");
                                if (brk)    // user:pass@host:port/path
                                {
                                    mPort = ExtractPortFrom(lastbrk);
                                    if (mPort <= 0)
                                        return NS_ERROR_MALFORMED_URI;
                                    ExtractString(brk, &mPath, len - (brk-mSpec));
                                    return NS_OK;
                                }
                                else        // user:pass@host:port
                                {
                                    mPort = ExtractPortFrom(lastbrk);
                                    if (mPort <= 0)
                                        return NS_ERROR_MALFORMED_URI;
                                    return NS_OK;
                                }
                                break;
                            case '/' :
                            case '?' :
                                ExtractString(brk, &mPath, len - (brk-mSpec));
                                return NS_OK;
                                break;
                            case '@' : // bad case
                                return NS_ERROR_MALFORMED_URI;
                                break;
                            default: NS_POSTCONDITION(0, "This just can't be!");
                                break;
                        }
                        break;
                    case ':' : // scheme:user:pass@host...or scheme:host:port...
                        /* TODO 
                        if you find @ in the remaining string 
                        then // scheme:user:pass@host...
                        {
                            

                        }
                        else // scheme:host:port
                        {
                            ExtractString(lastbrk, &mHost, (brk-lastbrk));
                        }
                        */
                        break;
                    default: NS_POSTCONDITION(0, "This just can't be!");
                        break;
                }
            }
        }
        break;
    case '@' :
        //Everything before the @ is the prehost stuff
        ExtractString(mSpec, &mPreHost, brk-mSpec);
        lastbrk = brk+1;
        brk = PL_strpbrk(lastbrk, ":/");
        if (!brk) // its user@host so everything else is just the host
        {
            ExtractString(lastbrk, &mHost, (len - (lastbrk-mSpec)));
            return NS_OK;
        }
        ExtractString(lastbrk, &mHost, (brk-lastbrk));
        if (*brk == ':') // user@host:port...
        {
            lastbrk = brk+1;
            brk = PL_strpbrk(lastbrk, "/?");
            if (brk)    // user@host:port/path
            {
                mPort = ExtractPortFrom(lastbrk);
                if (mPort <= 0)
                    return NS_ERROR_MALFORMED_URI;
                ExtractString(brk, &mPath, len - (brk-mSpec));
                return NS_OK;
            }
            else        // user@host:port
            {
                mPort = ExtractPortFrom(lastbrk);
                if (mPort <= 0)
                    return NS_ERROR_MALFORMED_URI;
                return NS_OK;
            }
        }
        else // (*brk == '/') so user@host/path
        {
            ExtractString(brk, &mPath, len - (brk - mSpec));
            return NS_OK;
        }
        break;
    default:
        NS_ASSERTION(0, "This just can't be!");
        break;
    }

    return NS_OK;
}

nsresult
nsStdURL::ReconstructSpec()
{
    if (mSpec) nsCRT::free(mSpec);

    nsCAutoString finalSpec; // guaranteed to be singlebyte.
    finalSpec.SetCapacity(64);
    if (mScheme)
    {
        finalSpec = mScheme;
        finalSpec += "://";
    }
    if (mPreHost)
    {
        finalSpec += mPreHost;
        finalSpec += '@';
    }
    if (mHost)
    {
        finalSpec += mHost;
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
    if (mPath)
    {
        finalSpec += mPath;
    }
    mSpec = finalSpec.ToNewCString();
    return (mSpec ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
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

nsresult 
nsStdURL::ExtractString(char* i_Src, char* *o_Dest, PRUint32 length)
{
    NS_PRECONDITION( (nsnull != i_Src), "Exract called on empty string!");
    CRTFREEIF(*o_Dest);
    if (0 == length)
        return NS_OK;
    *o_Dest = PL_strndup(i_Src, length);
    return (*o_Dest ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}

nsresult 
nsStdURL::DupString(char* *o_Dest, const char* i_Src)
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

NS_IMETHODIMP
nsStdURL::SetDirectory(const char* i_Directory)
{
    if (!i_Directory)
        return NS_ERROR_NULL_POINTER;
    
    if (mDirectory)
        nsCRT::free(mDirectory);

    nsAutoString dir;
    if ('/' != *i_Directory)
        dir += "/";
    
    dir += i_Directory;

    mDirectory = dir.ToNewCString();
    if (!mDirectory)
        return NS_ERROR_OUT_OF_MEMORY;
    return ReconstructPath();
}

NS_IMETHODIMP
nsStdURL::SetFileName(const char* i_FileName)
{
    if (!i_FileName)
        return NS_ERROR_NULL_POINTER;
    
    //Cleanout param, query and ref
    CRTFREEIF(mParam);
    CRTFREEIF(mQuery);
    CRTFREEIF(mRef);

    //If it starts with a / then everything is the path.
    if ('/' == *i_FileName) {
        return SetPath(i_FileName);
    }
 
    CRTFREEIF(mFileName);
    nsresult status = DupString(&mFileName, i_FileName);

    // XXX This is ineffecient
    ReconstructPath();
    ParsePath();

    return status;
}

NS_IMETHODIMP
nsStdURL::SetRef(const char* i_Ref)
{
    /* 
        no check for i_Ref = nsnull becuz you can remove # by using it that way
        So SetRef(nsnull) removed any existing ref values whereas
        SetRef("") will ensure that there is a # at the end.  These apply to 
        ? and ; as well. 
    */
    nsresult status = DupString(&mRef, 
           (i_Ref && (*i_Ref == '#')) ?  (i_Ref+1) : i_Ref);
    return (NS_FAILED(status) ? status : ReconstructPath());
}

NS_IMETHODIMP
nsStdURL::SetParam(const char* i_Param)
{
    nsresult status = DupString(&mParam, 
            (i_Param && (*i_Param == ';')) ? (i_Param+1) : i_Param);
    return (NS_FAILED(status) ? status : ReconstructPath());
}

NS_IMETHODIMP
nsStdURL::SetQuery(const char* i_Query)
{
    nsresult status = DupString(&mQuery, 
            (i_Query && (*i_Query == '?')) ? (i_Query+1) : i_Query);
    return (NS_FAILED(status) ? status : ReconstructPath());
}

NS_IMETHODIMP
nsStdURL::SetRelativePath(const char* i_Relative)
{
    nsresult rv = NS_OK;
    nsCAutoString options;
    char* ref;
    char* query;
    char* file;

    if (!i_Relative)
        return NS_ERROR_NULL_POINTER;

    // Make sure that if there is a : its before other delimiters
    // If not then its an absolute case
    static const char delimiters[] = "/;?#:";
    char* brk = PL_strpbrk(i_Relative, delimiters);
    if (brk && (*brk == ':')) // This is an absolute case
    {
        rv = SetSpec((char*) i_Relative);
        return rv;
    }

    switch (*i_Relative) 
    {
        case '/':
            return SetPath((char*) i_Relative);

        case ';': 
            // Append to Filename add then call SetFilePath
            options = mFileName;
            options += (char*)i_Relative;
            file = (char*)options.GetBuffer();
            rv = SetFileName(file);
            return rv;

        case '?': 
            // check for ref part
            ref = PL_strrchr(i_Relative, '#');
            if (!ref) {
                CRTFREEIF(mRef);
                return SetQuery((char*)i_Relative);
            } else {
                DupString(&query,nsnull);
                ExtractString((char*)i_Relative, &query, 
                    (PL_strlen(i_Relative)-(ref-i_Relative)));

                rv = SetQuery(query);
                nsCRT::free(query);
                if (NS_FAILED(rv)) return rv;
                rv = SetRef(ref);
                return rv;
            }
            break;

        case '#':
            return SetRef((char*)i_Relative);

        default:
            return SetFileName((char*)i_Relative);
    }
}

NS_IMETHODIMP
nsStdURL::Resolve(const char *relativePath, char **result) 
{
    nsresult rv;

    if (!relativePath) return NS_ERROR_NULL_POINTER;

    // Make sure that if there is a : its before other delimiters
    // If not then its an absolute case
    static const char delimiters[] = "/;?#:";
    char* brk = PL_strpbrk(relativePath, delimiters);
    if (brk && (*brk == ':')) // This is an absolute case
    {
        rv = DupString(result, relativePath);
        ReplaceMess(*result);
        return rv;
    }

    nsCAutoString finalSpec; // guaranteed to be singlebyte.

    if (mScheme)
    {
        finalSpec = mScheme;
        finalSpec += "://";
    }
    if (mPreHost)
    {
        finalSpec += mPreHost;
        finalSpec += '@';
    }
    if (mHost)
    {
        finalSpec += mHost;
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

    switch (*relativePath) 
    {
        case '/':
            finalSpec += (char*)relativePath;
            break;
        case ';': 
            finalSpec += mDirectory;
            finalSpec += mFileName;
            finalSpec += (char*)relativePath;
            break;
        case '?': 
            finalSpec += mDirectory;
            finalSpec += mFileName;
            if (mParam)
            {
                finalSpec += ';';
                finalSpec += mParam;
            }
            finalSpec += (char*)relativePath;
            break;
        case '#':
            finalSpec += mDirectory;
            finalSpec += mFileName;
            if (mParam)
            {
                finalSpec += ';';
                finalSpec += mParam;
            }
            if (mQuery)
            {
                finalSpec += '?';
                finalSpec += mQuery;
            }
            finalSpec += (char*)relativePath;
            break;
        default:
            finalSpec += mDirectory;
            finalSpec += (char*)relativePath;
    }
    *result = finalSpec.ToNewCString();
    if (*result) {
        ReplaceMess(*result);
        return NS_OK;
    } else
        return NS_ERROR_OUT_OF_MEMORY;
}

nsresult
nsStdURL::ReconstructPath(void)
{
    if (mPath) nsCRT::free(mPath);

    //Take all the elements of the path and construct it
    nsCAutoString path;
    path.SetCapacity(64);
    if (mDirectory)
    {
        path = mDirectory;
    }
    if (mFileName)
    {
        path += mFileName;
    }
    if (mParam)
    {
        path += ';';
        path += mParam;
    }
    if (mQuery)
    {
        path += '?';
        path += mQuery;
    }
    if (mRef)
    {
        path += '#';
        path += mRef;
    }
    mPath = path.ToNewCString();

    return (mPath ? ReconstructSpec() : NS_ERROR_OUT_OF_MEMORY);
}

/** Extract the elements like directory/filename, query, or ref from
 * the path.
 */
nsresult
nsStdURL::ParsePath(void)
{
    CRTFREEIF(mDirectory);
    CRTFREEIF(mFileName);
    CRTFREEIF(mParam);
    CRTFREEIF(mQuery);
    CRTFREEIF(mRef);

    if (!mPath) 
    {
        DupString(&mDirectory, "/");
        return (mDirectory ? ReconstructPath() : NS_ERROR_OUT_OF_MEMORY);
    }

    char* dirfile = nsnull;
    char* options = nsnull;

    int len = PL_strlen(mPath);

    /* Factor out the optionpart with ;?# */
    static const char delimiters[] = ";?#"; // for param, query and ref
    char* brk = PL_strpbrk(mPath, delimiters);

    if (!brk) // Everything is just path and filename
    {
        DupString(&dirfile, mPath); 
    } 
    else 
    {
        int dirfileLen = brk - mPath;
        ExtractString(mPath, &dirfile, dirfileLen);
        len -= dirfileLen;
        ExtractString(mPath + dirfileLen, &options, len);
    }

    /* now that we have broken up the path treat every part differently */
    /* first dir+file */

    char* file;

    int dlen = PL_strlen(dirfile);
    if (dlen == 0)
    {
        DupString(&mDirectory, "/");
        file = dirfile;
    } else {
        ReplaceMess(dirfile);
        // check new length
        dlen = PL_strlen(dirfile);

        // First find the last slash
        file = PL_strrchr(dirfile, '/');
        if (!file) 
        {
            DupString(&mDirectory, "/");
            file = dirfile;
        }

        // If its not the same as the first slash then extract directory
        if (file != dirfile)
        {
            ExtractString(dirfile, &mDirectory, (file - dirfile)+1);
        } else {
            DupString(&mDirectory, "/");
        }
    }

    /* Extract Filename */
    if (dlen > 0) {
        // Look again if there was a slash
        char* slash = PL_strrchr(dirfile, '/');
        if (slash) {
            ExtractString(file+1, &mFileName, dlen-(file-dirfile-1));
        } else {
            // Use the full String as Filename
            ExtractString(dirfile, &mFileName, dlen);
        } 
    }

#if 0
    // Now take a look at the options. "#" has precedence over "?"
    // which has precedence over ";"
    if (options) {
        // Look for "#" first. Everything following it is in the ref
        brk = PL_strchr(options, '#');
        if (brk) {
            *brk = 0;
            int pieceLen = len - (brk + 1 - options);
            ExtractString(brk+1, &mRef, pieceLen);
            len -= pieceLen + 1;
        }

        // Now look for "?"
        brk = PL_strchr(options, '?');
        if (brk) {
            *brk = 0;
            int pieceLen = len - (brk + 1 - options);
            ExtractString(brk+1, &mQuery, pieceLen);
            len -= pieceLen + 1;
        }

        // Now look for ';'
        brk = PL_strchr(options, ';');
        if (brk) {
            int pieceLen = len - (brk + 1 - options);
            ExtractString(brk+1, &mParam, pieceLen);
            len -= pieceLen + 1;
        }
    }

#endif
    while (brk)
    {
        int pieceLen;
        char* lastbrk = brk;
        brk = PL_strpbrk(lastbrk+1, delimiters);
        switch (*lastbrk)
        {
            case ';': // handles cases of ;foo?bar#baz correctly
              pieceLen = (brk ? (brk-lastbrk-1) : len);
              ExtractString(lastbrk+1, &mParam, pieceLen);
              len -= pieceLen;
              break;
            case '?': // Only # takes higher precedence than this
              // so changing brk to only check for #
              brk = PL_strpbrk(lastbrk+1 , "#");
              pieceLen = (brk ? (brk-lastbrk-1) : len);
              ExtractString(lastbrk+1, &mQuery, pieceLen);
              len -= pieceLen;
              break;
            case '#':
              // Since this has the highest precedence everything following it
              // is a ref. So...
              pieceLen = len;
              ExtractString(lastbrk+1, &mRef, pieceLen);
              len -= pieceLen;
              break;
            default:
              NS_ASSERTION(0, "This just can't be!");
              break;
        }
    }

    nsCRT::free(dirfile);
    nsCRT::free(options);

    ReconstructPath();
    return NS_OK;
}

NS_METHOD
nsStdURL::SetSpec(const char* i_Spec)
{
    // Skip leading spaces
    char* fwdPtr= (char*) i_Spec;
    while (fwdPtr && (*fwdPtr != '\0') && (*fwdPtr == ' '))
        fwdPtr++;

    CRTFREEIF(mSpec);
    nsresult status = DupString(&mSpec, fwdPtr);
    // If spec is being rewritten clean up everything-
    CRTFREEIF(mScheme);
    CRTFREEIF(mPreHost);
    CRTFREEIF(mHost);
    mPort = -1;
    CRTFREEIF(mPath);
    CRTFREEIF(mDirectory);
    CRTFREEIF(mFileName);
    CRTFREEIF(mParam);
    CRTFREEIF(mQuery);
    CRTFREEIF(mRef);
    return (NS_FAILED(status) ? status : Parse());
}

NS_METHOD
nsStdURL::SetPath(const char* i_Path)
{
    if (mPath) nsCRT::free(mPath);
    nsresult status = DupString(&mPath, i_Path);
    ParsePath();
    ReconstructSpec();
    return status;
}
    
NS_METHOD
nsStdURL::GetFilePath(char **o_DirFile)
{
    if (!o_DirFile)
        return NS_ERROR_NULL_POINTER;
    
    nsAutoString temp;
    if (mDirectory)
    {
        temp = mDirectory;
    }
    if (mFileName)
    {
        temp += mFileName;
    }
    *o_DirFile = temp.ToNewCString();
    if (!*o_DirFile)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_METHOD
nsStdURL::SetFilePath(const char *filePath)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsStdURL::GetFileBaseName(char **o_name)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsStdURL::SetFileBaseName(const char *name)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
