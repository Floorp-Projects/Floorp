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

static NS_DEFINE_CID(kStdURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kThisStdURLImplementationCID,
                     NS_THIS_STANDARDURL_IMPLEMENTATION_CID);

PRInt32 ExtractPortFrom(char* src, int start, int length);
void ReplaceDotMess(char* io_Path);

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
    mSpec = i_Spec ? nsCRT::strdup(i_Spec) : nsnull;
    if (mSpec)
        ReplaceDotMess(mSpec);
    NS_INIT_AGGREGATED(outer);
    if (i_Spec) 
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

     if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
         *aInstancePtr = GetInner();
    else if (aIID.Equals(kThisStdURLImplementationCID) ||   // used by Equals
        aIID.Equals(nsCOMTypeInfo<nsIURL>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsIURI>::GetIID()))
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
    // Parse the path into its individual elements 
    // when we are done from here.
    nsParsePath pp(this);
    NS_PRECONDITION( (nsnull != mSpec), "Parse called on empty url!");
    if (!mSpec)
        return NS_ERROR_MALFORMED_URI;

    //Remember to remove leading/trailing spaces, etc. TODO
    int len = PL_strlen(mSpec);
    static const char delimiters[] = "/:@?"; //this order is optimized.
    char* brk = PL_strpbrk(mSpec, delimiters);
    char* lastbrk = brk;
    if (!brk) // everything is a host
    {
        ExtractString(mSpec, &mHost, 0, len);
        return NS_OK;
    }
    switch (*brk)
    {
    case '/' :
    case '?' :
        // If the URL starts with a slash then everything is a path
        if (brk == mSpec)
        {
            ExtractString(mSpec, &mPath, 0, len);
            return NS_OK;
        }
        else // The first part is host, so its host/path
        {
            ExtractString(mSpec, &mHost, 0, (brk - mSpec));
            ExtractString(mSpec, &mPath, (brk - mSpec), (len - (brk - mSpec)));
            return NS_OK;
        }
        break;
    case ':' :
        if (*(brk+1) == '/') 
        {
            ExtractString(mSpec, &mScheme, 0, (brk - mSpec));

            if (*(brk+2) == '/') // e.g. http://
            // If the first colon is followed by // then its definitely a spec
            {
                lastbrk = brk+3;
                brk = PL_strpbrk(lastbrk, delimiters);
                if (!brk) // everything else is a host, as in http://host
                {
                    ExtractString(mSpec, &mHost, 
                        (lastbrk - mSpec), 
                        len - (lastbrk - mSpec));
                    return NS_OK;
                }
                switch (*brk)
                {
                    case '/' : // standard case- http://host/path
                    case '?' : // missing path cases
                        ExtractString(mSpec, &mHost, 
                            (lastbrk - mSpec), (brk - lastbrk));
                        ExtractString(mSpec, &mPath, 
                            (brk - mSpec), (len - (brk - mSpec)));
                        return NS_OK;
                        break;
                    case ':' : // http://user:... or http://host:...
                    {
// It could be http://user:pass@host/path 
// or http://host:port/path we find that by checking further...
char* nextbrk = PL_strpbrk(brk+1, delimiters);
if (!nextbrk) // http://host:port
{
    ExtractString(mSpec, &mHost, 
        (lastbrk-mSpec), (brk-lastbrk));
    mPort = ExtractPortFrom(mSpec, 
        (brk-mSpec +1), len - (brk-mSpec +1));
    return NS_OK;
}
switch (*nextbrk)
{
    case '/': // http://host:port/path
    case '?': // http://host:port?path
        ExtractString(mSpec, &mHost, 
            (lastbrk-mSpec), (brk-lastbrk));
        mPort = ExtractPortFrom(mSpec, (brk-mSpec+1),(nextbrk-brk-1));
        ExtractString(mSpec, &mPath, 
                (nextbrk-mSpec), len - (nextbrk-mSpec));
        return NS_OK;
        break;
    case '@': // http://user:pass@host...
        ExtractString(mSpec, &mPreHost, 
            (lastbrk - mSpec), (nextbrk - lastbrk));
        brk = PL_strpbrk(nextbrk+1, delimiters);
        if (!brk) // its just http://user:pass@host
        {
            ExtractString(mSpec, &mHost, 
                (nextbrk+1 - mSpec), 
                len - (nextbrk+1 - mSpec));
            return NS_OK;
        }

        ExtractString(mSpec, &mHost, 
            (nextbrk+1 - mSpec), brk - (nextbrk+1));

        switch (*brk)
        {
        case '/': // http://user:pass@host/path
        case '?':
            ExtractString(mSpec, &mPath,
                (nextbrk+1 - mSpec),
                len - (nextbrk+1 - mSpec));
            return NS_OK;
            break;
        case ':': // http://user:pass@host:port...
            lastbrk = brk+1;
            brk = PL_strpbrk(lastbrk, "/?");
            if (brk)    // http://user:pass@host:port/path
            {
                mPort = ExtractPortFrom(mSpec, (lastbrk-mSpec),(brk-lastbrk));
                ExtractString(mSpec, &mPath, (brk-mSpec), len - (brk-mSpec));
                return NS_OK;
            }
            else        // http://user:pass@host:port
            {
                mPort = ExtractPortFrom(mSpec, (lastbrk-mSpec),
                    len - (lastbrk-mSpec));
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
                            ExtractString(mSpec, &mPreHost, 
                                (lastbrk-mSpec), (brk-lastbrk));
                            lastbrk = brk+1;
                            brk = PL_strpbrk(lastbrk, delimiters);
                            if (!brk) // its just http://user@host
                            {
                                ExtractString(mSpec, &mHost, 
                                    (lastbrk+1 - mSpec), 
                                    len - (lastbrk+1 - mSpec));
                                return NS_OK;
                            }
                            ExtractString(mSpec, &mHost, 
                                (lastbrk-mSpec), (brk - lastbrk));
                            switch (*brk)
                            {
                                case ':' : // http://user@host:port...
                                    lastbrk = brk+1;
                                    brk = PL_strpbrk(lastbrk, "/?");
                                    if (brk)    // http://user@host:port/path
                                    {
                                        mPort = ExtractPortFrom(mSpec, 
                                            (lastbrk-mSpec),(brk-lastbrk));
                                        ExtractString(mSpec, &mPath, 
                                            (brk-mSpec), 
                                            len - (brk-mSpec));
                                        return NS_OK;
                                    }
                                    else        // http://user@host:port
                                    {
                                        mPort = ExtractPortFrom(mSpec, 
                                            (lastbrk-mSpec),
                                            len - (lastbrk-mSpec));
                                        return NS_OK;
                                    }
                                    break;
                                case '/' : // http://user@host/path
                                case '?' : // http://user@host?path
                                    ExtractString(mSpec, &mPath, 
                                        (brk - mSpec), 
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
                ExtractString(mSpec, &mPath, (brk-mSpec+1), 
                    len - (brk-mSpec+1)); 
                return NS_OK;
            }
        }
        else // scheme:host or host:port...
        {
            lastbrk = brk+1;

            if ((*lastbrk >= '0') && (*lastbrk <= '9')) //host:port...
            {
                ExtractString(mSpec, &mHost, 0, (brk - mSpec));
                brk = PL_strpbrk(lastbrk, delimiters);
                if (!brk) // Everything else is just the port
                {
                    mPort = ExtractPortFrom(mSpec, lastbrk-mSpec, 
                        len - (lastbrk-mSpec));
                    return NS_OK;
                }
                switch (*brk)
                {
                    case '/' : // The path, so its host:port/path
                    case '?' : // The path, so its host:port?path
                        mPort = ExtractPortFrom(mSpec, lastbrk-mSpec, 
                            brk-lastbrk);
                        ExtractString(mSpec, &mPath, brk- mSpec, 
                            len - (brk-mSpec));
                        return NS_OK;
                        break;
                    case ':' : 
                        return NS_ERROR_MALFORMED_URI;
                        break;
                    case '@' :
                        // This is a special case of user:pass@host... so 
                        // Cleanout our earliar knowledge of host
                        ExtractString(mSpec, &mHost, -1, -1);

                        ExtractString(mSpec, &mPreHost, 0, (brk-mSpec));
                        lastbrk = brk+1;
                        brk = PL_strpbrk(lastbrk, ":/?");
                    // its user:pass@host so everthing else is just the host
                        if (!brk)
                        {
                            ExtractString(mSpec, &mHost, 
                                (lastbrk-mSpec), len - (lastbrk-mSpec));
                            return NS_OK;
                        }
                        ExtractString(mSpec, &mHost, 
                            (lastbrk-mSpec), (brk-lastbrk));
                        if (*brk == ':') // user:pass@host:port...
                        {
                            lastbrk = brk+1;
                            brk = PL_strpbrk(lastbrk, "/?");
                            if (brk)    // user:pass@host:port/path
                            {
                                mPort = ExtractPortFrom(mSpec, 
                                    (lastbrk-mSpec),(brk-lastbrk));
                                ExtractString(mSpec, &mPath, 
                                    (brk-mSpec), len - (brk-mSpec));
                                return NS_OK;
                            }
                            else        // user:pass@host:port
                            {
                                mPort = ExtractPortFrom(mSpec, 
                                    (lastbrk-mSpec),len - (lastbrk-mSpec));
                                return NS_OK;
                            }
                        }
                        else // (*brk == '/') || (*brk == '?') 
                            // so user:pass@host/path
                        {
                            ExtractString(mSpec, &mPath, (brk - mSpec), 
                                len - (brk - mSpec));
                            return NS_OK;
                        }
                        break;
                    default: NS_POSTCONDITION(0, "This just can't be!");
                        break;
                }
            }
            else // scheme:host...
            {
                ExtractString(mSpec, &mScheme, 0, (brk - mSpec));
                brk = PL_strpbrk(lastbrk, delimiters);
                if (!brk) // its just scheme:host
                {
                    ExtractString(mSpec, &mHost, (lastbrk-mSpec), 
                        len - (lastbrk-mSpec));
                    return NS_OK;
                }
                switch (*brk)
                {
                    case '/' : // The path, so its scheme:host/path
                    case '?' : // The path, so its scheme:host?path
                        ExtractString(mSpec, &mHost, (lastbrk-mSpec), 
                            (brk-lastbrk));
                        ExtractString(mSpec, &mPath, (brk - mSpec), 
                            len - (brk - mSpec));
                        return NS_OK;
                        break;
                    case '@' : // scheme:user@host...
                        ExtractString(mSpec, &mPreHost, (lastbrk-mSpec), 
                            (brk-lastbrk));
                        lastbrk = brk+1;
                        brk = PL_strpbrk(lastbrk, delimiters);
                        if (!brk) // scheme:user@host only
                        {
                            ExtractString(mSpec, &mHost, (lastbrk - mSpec), 
                                len - (lastbrk-mSpec));
                            return NS_OK;
                        }
                        ExtractString(mSpec, &mHost, (lastbrk-mSpec),
                            (brk - lastbrk));
                        switch (*brk)
                        {
                            case ':' : // scheme:user@host:port...
                                lastbrk = brk+1;
                                brk = PL_strpbrk(lastbrk, "/?");
                                if (brk)    // user:pass@host:port/path
                                {
                                    mPort = ExtractPortFrom(mSpec, 
                                        (lastbrk-mSpec),(brk-lastbrk));
                                    ExtractString(mSpec, &mPath, 
                                        (brk-mSpec), len - (brk-mSpec));
                                    return NS_OK;
                                }
                                else        // user:pass@host:port
                                {
                                    mPort = ExtractPortFrom(mSpec, 
                                        (lastbrk-mSpec),len - (lastbrk-mSpec));
                                    return NS_OK;
                                }
                                break;
                            case '/' :
                            case '?' :
                                ExtractString(mSpec, &mPath, 
                                    (brk-mSpec), len - (brk-mSpec));
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
                            ExtractString(mSpec, &mHost, (lastbrk-mSpec), 
                                (brk-lastbrk));
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
        ExtractString(mSpec, &mPreHost, 0, brk-mSpec);
        lastbrk = brk+1;
        brk = PL_strpbrk(lastbrk, ":/");
        if (!brk) // its user@host so everything else is just the host
        {
            ExtractString(mSpec, &mHost, (lastbrk-mSpec), 
                (len - (lastbrk-mSpec)));
            return NS_OK;
        }
        ExtractString(mSpec, &mHost, (lastbrk-mSpec), (brk-lastbrk));
        if (*brk == ':') // user@host:port...
        {
            lastbrk = brk+1;
            brk = PL_strpbrk(lastbrk, "/?");
            if (brk)    // user@host:port/path
            {
                mPort = ExtractPortFrom(mSpec, (lastbrk-mSpec),(brk-lastbrk));
                ExtractString(mSpec, &mPath, (brk-mSpec), len - (brk-mSpec));
                return NS_OK;
            }
            else        // user@host:port
            {
                mPort = ExtractPortFrom(mSpec, (lastbrk-mSpec),
                    len - (lastbrk-mSpec));
                return NS_OK;
            }
        }
        else // (*brk == '/') so user@host/path
        {
            ExtractString(mSpec, &mPath, (brk - mSpec), len - (brk - mSpec));
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

    char portBuffer[10];
    if (-1 != mPort) {
        PR_snprintf(portBuffer, 10, ":%d", mPort);
    }
    else {
        portBuffer[0] = '\0';
    }
    
    nsString finalSpec;
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
    }
    if (-1 != mPort)
    {
        finalSpec += portBuffer;
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
nsStdURL::ExtractString(char* i_Source, char* *o_Destination, PRUint32 start, PRUint32 length)
{
    NS_PRECONDITION( (nsnull != i_Source), "Exract called on empty string!");
    if (*o_Destination)
        nsCRT::free(*o_Destination);
    *o_Destination = nsnull;
    if (0 == length)
        return NS_OK;
    char* startPosition = i_Source + start;
    *o_Destination = PL_strndup(startPosition, length);
    return (*o_Destination ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}

PRInt32 ExtractPortFrom(char* src, int start, int length)
{
    char* port = new char[length +1];
    PRInt32 returnValue = -1;
    if (!port)
        return returnValue; // ERROR!
    PL_strncpyz(port, src+start, length+1);
    returnValue = atoi(port);
    delete[] port;
    return returnValue;
}

nsresult 
nsStdURL::DupString(char* *o_Destination, char* i_Source)
{
    if (!o_Destination)
        return NS_ERROR_NULL_POINTER;
    if (i_Source)
    {
        *o_Destination = nsCRT::strdup(i_Source);
        return (*o_Destination == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    else
    {
        *o_Destination = nsnull;
        return NS_OK;
    }
}

NS_IMETHODIMP
nsStdURL::SetDirectory(char* i_Directory)
{
    if (!i_Directory)
        return NS_ERROR_NULL_POINTER;
    
    if (mDirectory)
        nsCRT::free(mDirectory);

    nsString dir;
    if ('/' != *i_Directory)
        dir += "/";
    
    dir += i_Directory;
    dir.Trim("/",PR_FALSE); // Removes trailing slash if any...

    mDirectory = dir.ToNewCString();
    if (!mDirectory)
        return NS_ERROR_OUT_OF_MEMORY;
    return ReconstructPath();
}

NS_IMETHODIMP
nsStdURL::SetFileName(char* i_FileName)
{
    nsParsePath pp(this); // Someone mayhave set .. in the name
    if (!i_FileName)
        return NS_ERROR_NULL_POINTER;
    
    //Cleanout param, query and ref
    CRTFREEIF(mParam);
    CRTFREEIF(mQuery);
    CRTFREEIF(mRef);

    //If it starts with a / then everything is the path.
    if ('/' == *i_FileName)
        return SetPath(i_FileName);
 
    if (mFileName) nsCRT::free(mFileName);
    nsresult status = DupString(&mFileName, i_FileName);
    return (NS_FAILED(status) ? status : ReconstructPath());
}

NS_IMETHODIMP
nsStdURL::SetRef(char* i_Ref)
{
    nsresult status;
    if (i_Ref && (*i_Ref == '#'))
        status = DupString(&mRef, i_Ref+1);
    else
        status = DupString(&mRef, i_Ref);
    return (NS_FAILED(status) ? status : ReconstructPath());
}

NS_IMETHODIMP
nsStdURL::SetParam(char* i_Param)
{
    nsresult status;
    if (i_Param && (*i_Param == ';'))
        status = DupString(&mParam, i_Param+1);
    else
        status = DupString(&mParam, i_Param);
    return (NS_FAILED(status) ? status : ReconstructPath());
}

NS_IMETHODIMP
nsStdURL::SetQuery(char* i_Query)
{
    nsresult status;
    if (i_Query && (*i_Query == '?'))
        status = DupString(&mQuery, i_Query+1);
    else
        status = DupString(&mQuery, i_Query);
    return (NS_FAILED(status) ? status : ReconstructPath());
}

NS_IMETHODIMP
nsStdURL::SetRelativePath(const char* i_Relative)
{
    if (!i_Relative)
        return NS_ERROR_NULL_POINTER;
    // If it has a scheme already then its an absolute URL
    nsStdURL temp(i_Relative);

    if (temp.mScheme)
    {
        //we should perhaps clone temp to this
        return SetSpec((char*)i_Relative);
    }
    
    switch (*i_Relative) 
    {
        case '/': return SetPath((char*) i_Relative);
            break;
        case '?': return SetQuery((char*) i_Relative);
            break;
        case '#' : return SetRef((char*) i_Relative);
            break;
        default: return SetFileName((char*)i_Relative);
            break;
    }
}

nsresult
nsStdURL::ReconstructPath(void)
{
    //Take all the elements of the path and construct it
    if (mPath) nsCRT::free(mPath);
    nsString path;
    if (mDirectory)
    {
        path = mDirectory;
        // if we have anything in the dir besides just the / 
        if (PL_strlen(mDirectory)>1)
            path += '/'; 
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
    ReplaceDotMess(mPath);
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

    int len = PL_strlen(mPath);
    if (len == 1)
    {
        mDirectory = nsCRT::strdup("/");
        return (mDirectory ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
    }

    ReplaceDotMess(mPath);

    // if the previous cleanup cleaned up everything then reset it to /
    if (PL_strlen(mPath) == 0)
    {
        DupString(&mDirectory, "/");
        return (mDirectory ? ReconstructPath() : NS_ERROR_OUT_OF_MEMORY);
    }
    // First find the last slash
    char* file = PL_strrchr(mPath, '/');
    if (!file) 
    {
        // Treat the whole mPath as file -- this could still have ?, # etc.
        file = mPath;
    }
    // If its not the same as the first slash then extract directory
    if (file != mPath)
    {
        ExtractString(mPath, &mDirectory, 0, (file - mPath));
    }
    else
        DupString(&mDirectory, "/");

    len = PL_strlen(file);
    if (len==1) 
        return NS_OK; // Optimize for www.foo.com/path/

    static const char delimiters[] = ";?#"; // for param, query and ref
    char* brk = PL_strpbrk(file, delimiters);
    if (!brk) // Everything in the file is just the filename
    {
        return DupString(&mFileName, file+1); //+1 to skip the leading slash
    }
    ExtractString(file, &mFileName, 1 /* skip the leading / */, (brk-file-1));
    //Keep pulling out other pieces...
    while (brk)
    {
        char* lastbrk = brk;
        brk = PL_strpbrk(lastbrk+1, delimiters);
        switch (*lastbrk)
        {
            case ';' : ExtractString(lastbrk, &mParam, 1, (brk ? (brk-lastbrk-1) : (len - (lastbrk-file) -1)));
                break;
            case '?' : ExtractString(lastbrk, &mQuery, 1, (brk ? (brk-lastbrk-1) : (len - (lastbrk-file) -1)));
                break;
            case '#' : ExtractString(lastbrk, &mRef, 1, (brk ? (brk-lastbrk-1) : (len - (lastbrk-file) -1)));
                break;
            default:
                NS_ASSERTION(0, "This just can't be!");
                break;
        }
    }
    return NS_OK;
}

NS_METHOD
nsStdURL::SetSpec(char* i_Spec)
{
    CRTFREEIF(mSpec);
    nsresult status = DupString(&mSpec, i_Spec);
    //If spec is being rewritten clean up everything-
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
nsStdURL::SetPath(char* i_Path)
{
    if (mPath) nsCRT::free(mPath);
    nsresult status = DupString(&mPath, i_Path);
    ParsePath();
    ReconstructSpec();
    return status;
}

void ReplaceDotMess(char* io_Path)
{
    // Replace all /./ with a /
    // Also changes all \ to /
    /* Stolen from netlib's mkparse.c.
     *
     * modifies a url of the form   /foo/../foo1  ->  /foo1
     *                       and    /foo/./foo1   ->  /foo/foo1
     */
    char *fwdPtr = io_Path;
    char *urlPtr = io_Path;
    
    for(; *fwdPtr != '\0'; ++fwdPtr)
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
        }
        else
        {
            // copy the url incrementaly 
            *urlPtr++ = *fwdPtr;
        }
    }
    *urlPtr = '\0';  // terminate the url 
}
    
NS_METHOD
nsStdURL::DirFile(char **o_DirFile)
{
    if (!o_DirFile)
        return NS_ERROR_NULL_POINTER;
    
    nsString temp;
    if (mDirectory)
    {
        temp = mDirectory;
        // if we have anything in the dir besides just the / 
        if (mDirectory[1])
            temp += '/'; 
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
