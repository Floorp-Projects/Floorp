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

nsStdURL::nsStdURL(const char* i_Spec, nsISupports* outer)
    : mScheme(nsnull),
      mPreHost(nsnull),
      mHost(nsnull),
      mPort(-1),
      mPath(nsnull),
	  mDirectory(nsnull),
	  mFileName(nsnull),
      mQuery(nsnull),
      mRef(nsnull)
{
	mSpec = i_Spec ? nsCRT::strdup(i_Spec) : nsnull;
    NS_INIT_AGGREGATED(outer);
	if (i_Spec) 
		Parse();
}

nsStdURL::~nsStdURL()
{
    if (mScheme)
	{
		delete[] mScheme;
		mScheme = 0;
	}
    if (mPreHost)
	{
		delete[] mPreHost;
		mPreHost = 0;
	}
    if (mHost)
	{
		delete[] mHost;
		mHost = 0;
	}
    if (mPath)
	{
		delete[] mPath;
		mPath = 0;
	}
    if (mRef)
	{
		delete[] mRef;
		mRef = 0;
	}
    if (mQuery)
	{
		delete[] mQuery;
		mQuery = 0;
	}
    if (mSpec)
	{
		delete[] mSpec;
		mSpec = 0;
	}
	if (mDirectory)
	{
		delete[] mDirectory;
		mDirectory = 0;
	}
	if (mFileName)
	{
		delete[] mFileName;
		mFileName = 0;
	}
}

NS_IMPL_AGGREGATED(nsStdURL);

NS_IMETHODIMP
nsStdURL::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(kThisStdURLImplementationCID) ||        // used by Equals
        aIID.Equals(nsCOMTypeInfo<nsIURL>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsIURI>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIURL*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
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
	nsStdURL* url = new nsStdURL(mSpec); /// TODO check outer?
    if (url == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
	nsresult rv= NS_OK;
	if (mSpec)
		rv = url->Parse(); // Will build the elements again.

    *o_URI = url;
    NS_ADDREF(url);
    return rv;
}

NS_IMETHODIMP
nsStdURL::Parse()
{
    NS_PRECONDITION( (nsnull != mSpec), "Parse called on empty url!");
    if (!mSpec)
    {
		//TODO
        return NS_ERROR_FAILURE; // this should really be NS_ERROR_URL_PARSING
    }

    //Remember to remove leading/trailing spaces, etc. TODO
    int len = PL_strlen(mSpec);
    static const char delimiters[] = "/:@"; //this order is optimized.
    char* brk = PL_strpbrk(mSpec, delimiters);
    char* lastbrk = brk;
    if (brk) 
    {
        switch (*brk)
        {
        case '/' :
            // If the URL starts with a slash then everything is a path
            if (brk == mSpec)
            {
				ExtractString(&mPath, 0, len);
                return NS_OK;
            }
            else // The first part is host, so its host/path
            {
                ExtractString(&mHost, 0, (brk - mSpec));
                ExtractString(&mPath, (brk - mSpec), (len - (brk - mSpec)));
                return NS_OK;
            }
            break;
        case ':' :
            // If the first colon is followed by // then its definitely a spec
            if ((*(brk+1) == '/') && (*(brk+2) == '/')) // e.g. http://
            {
                ExtractString(&mScheme, 0, (brk - mSpec));    
                lastbrk = brk+3;
                brk = PL_strpbrk(lastbrk, delimiters);
                if (brk)
                {
                    switch (*brk)
                    {
                        case '/' : // standard case- http://host/path
                            ExtractString(&mHost, 
								(lastbrk - mSpec), (brk - lastbrk));
                            ExtractString(&mPath, 
								(brk - mSpec), (len - (brk - mSpec)));
                            return NS_OK;
                            break;
                        case ':' : 
                            {
                                // It could be http://user:pass@host/path 
								// or http://host:port/path
                                // For the first case, there has to be an 
								// @ after this colon, so...
                                char* atSign = PL_strchr(brk, '@');
                                if (atSign)
                                {
                                    ExtractString(&mPreHost, 
										(lastbrk - mSpec), (atSign - lastbrk));
                                    brk = PL_strpbrk(atSign+1, "/:");
                                    if (brk) // http://user:pass@host:port/path or http://user:pass@host/path
                                    {
                                        ExtractString(&mHost, 
											(atSign+1 - mSpec), 
											(brk - (atSign+1)));
                                        if (*brk == '/')
                                        {
                                            ExtractString(&mPath, 
												(brk - mSpec), 
												len - (brk - mSpec));
                                            return NS_OK;
                                        }
                                        else // we have a port since (brk == ':')
                                        {
                                            lastbrk = brk+1;
                                            brk = PL_strchr(lastbrk, '/');
                                            if (brk) // http://user:pass@host:port/path
                                            {
                                                mPort = ExtractPortFrom(mSpec, (lastbrk - mSpec), (brk-lastbrk));
                                                ExtractString(&mPath, (brk-mSpec), len - (brk-mSpec));
                                                return NS_OK;
                                            }
                                            else // http://user:pass@host:port
                                            {
                                                mPort = ExtractPortFrom(mSpec, (lastbrk - mSpec), len - (lastbrk - mSpec));
                                                return NS_OK;
                                            }
                                        }

                                    }
                                    else // its just http://user:pass@host
                                    {
                                        ExtractString(&mHost, 
											(atSign+1 - mSpec), 
											len - (atSign+1 - mSpec));
                                        return NS_OK;
                                    }
                                }
                                else // definitely the port option, i.e. http://host:port/path
                                {
                                    ExtractString(&mHost, 
										(lastbrk-mSpec), 
										(brk-lastbrk));
                                    lastbrk = brk+1;
                                    brk = PL_strchr(lastbrk, '/');
                                    if (brk)    // http://host:port/path
                                    {
                                        mPort = ExtractPortFrom(mSpec, (lastbrk-mSpec),(brk-lastbrk));
                                        ExtractString(&mPath, 
											(brk-mSpec), 
											len - (brk-mSpec));
                                        return NS_OK;
                                    }
                                    else        // http://host:port
                                    {
                                        mPort = ExtractPortFrom(mSpec, (lastbrk-mSpec),len - (lastbrk-mSpec));
                                        return NS_OK;
                                    }
                                }
                            }
                            break;
                        case '@' : 
                            // http://user@host...
                            {
                                ExtractString(&mPreHost, 
									(lastbrk-mSpec), (brk-lastbrk));
                                lastbrk = brk+1;
                                brk = PL_strpbrk(lastbrk, ":/");
                                if (brk)
                                {
                                    ExtractString(&mHost, 
										(lastbrk-mSpec), (brk - lastbrk));
                                    if (*brk == ':') // http://user@host:port...
                                    {
                                        lastbrk = brk+1;
                                        brk = PL_strchr(lastbrk, '/');
                                        if (brk)    // http://user@host:port/path
                                        {
                                            mPort = ExtractPortFrom(mSpec, (lastbrk-mSpec),(brk-lastbrk));
                                            ExtractString(&mPath, 
												(brk-mSpec), 
												len - (brk-mSpec));
                                            return NS_OK;
                                        }
                                        else        // http://user@host:port
                                        {
                                            mPort = ExtractPortFrom(mSpec, (lastbrk-mSpec),len - (lastbrk-mSpec));
                                            return NS_OK;
                                        }

                                    }
                                    else // (*brk == '/') so no port just path i.e. http://user@host/path
                                    {
                                        ExtractString(&mPath, 
											(brk - mSpec), 
											len - (brk - mSpec));
                                        return NS_OK;
                                    }
                                }
                                else // its just http://user@host
                                {
                                    ExtractString(&mHost, 
										(lastbrk+1 - mSpec), len - (lastbrk+1 - mSpec));
                                    return NS_OK;
                                }

                            }
                            break;
                        default: NS_POSTCONDITION(0, "This just can't be!");
                            break;
                    }

                }
                else // everything else is a host, as in http://host
                {
                    ExtractString(&mHost, 
						(lastbrk - mSpec), 
						len - (lastbrk - mSpec));
                    return NS_OK;
                }

            }
            else // host:port...
            {
                ExtractString(&mHost, 0, (brk - mSpec));
                lastbrk = brk+1;
                brk = PL_strpbrk(lastbrk, delimiters);
                if (brk)
                {
                    switch (*brk)
                    {
                        case '/' : // The path, so its host:port/path
                            mPort = ExtractPortFrom(mSpec, lastbrk-mSpec, brk-lastbrk);
                            ExtractString(&mPath, brk- mSpec, len - (brk-mSpec));
                            return NS_OK;
                            break;
                        case ':' : 
                            return NS_ERROR_FAILURE;//TODO NS_ERROR_URL_PARSING; 
                            break;
                        case '@' :
                            // This is a special case of user:pass@host... so 
                            // Cleanout our earliar knowledge of host
                            ExtractString(&mHost, -1, -1);

                            ExtractString(&mPreHost, 0, (brk-mSpec));
                            lastbrk = brk+1;
                            brk = PL_strpbrk(lastbrk, ":/");
                            if (brk)
                            {
                                ExtractString(&mHost, 
									(lastbrk-mSpec), (brk-lastbrk));
                                if (*brk == ':') // user:pass@host:port...
                                {
                                    lastbrk = brk+1;
                                    brk = PL_strchr(lastbrk, '/');
                                    if (brk)    // user:pass@host:port/path
                                    {
                                        mPort = ExtractPortFrom(mSpec, (lastbrk-mSpec),(brk-lastbrk));
                                        ExtractString(&mPath, 
											(brk-mSpec), len - (brk-mSpec));
                                        return NS_OK;
                                    }
                                    else        // user:pass@host:port
                                    {
                                        mPort = ExtractPortFrom(mSpec, (lastbrk-mSpec),len - (lastbrk-mSpec));
                                        return NS_OK;
                                    }
                                }
                                else // (*brk == '/') so user:pass@host/path
                                {
                                    ExtractString(&mPath, (brk - mSpec), len - (brk - mSpec));
                                    return NS_OK;
                                }
                            }
                            else // its user:pass@host so everthing else is just the host
                            {
                                ExtractString(&mHost, 
									(lastbrk-mSpec), len - (lastbrk-mSpec));
                                return NS_OK;
                            }

                            break;
                        default: NS_POSTCONDITION(0, "This just can't be!");
                            break;
                    }
                }
                else // Everything else is just the port
                {
                    mPort = ExtractPortFrom(mSpec, lastbrk-mSpec, len - (lastbrk-mSpec));
                    return NS_OK;
                }
            }
            break;
        case '@' :
            //Everything before the @ is the prehost stuff
            ExtractString(&mPreHost, 0, brk-mSpec);
            lastbrk = brk+1;
            brk = PL_strpbrk(lastbrk, ":/");
            if (brk)
            {
                ExtractString(&mHost, (lastbrk-mSpec), (brk-lastbrk));
                if (*brk == ':') // user@host:port...
                {
                    lastbrk = brk+1;
                    brk = PL_strchr(lastbrk, '/');
                    if (brk)    // user@host:port/path
                    {
                        mPort = ExtractPortFrom(mSpec, (lastbrk-mSpec),(brk-lastbrk));
                        ExtractString(&mPath, (brk-mSpec), len - (brk-mSpec));
                        return NS_OK;
                    }
                    else        // user@host:port
                    {
                        mPort = ExtractPortFrom(mSpec, (lastbrk-mSpec),len - (lastbrk-mSpec));
                        return NS_OK;
                    }
                }
                else // (*brk == '/') so user@host/path
                {
                    ExtractString(&mPath, (brk - mSpec), len - (brk - mSpec));
                    return NS_OK;
                }
            }
            else // its user@host so everything else is just the host
            {
                ExtractString(&mHost, (lastbrk-mSpec), (len - (lastbrk-mSpec)));
                return NS_OK;
            }
            break;
        default:
            NS_POSTCONDITION(0, "This just can't be!");
            break;
        }
    }
    else // everything is a host
    {
        ExtractString(&mHost, 0, len);
    }
    return NS_OK;
}

nsresult
nsStdURL::ReconstructSpec()
{
    PR_FREEIF(mSpec);

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
		finalSpec += ':';
		finalSpec += portBuffer;
	}
	if (mPath)
	{
		finalSpec += mPath;
	}

	mSpec = finalSpec.ToNewCString();
	return NS_OK;
}

NS_METHOD
nsStdURL::Create(nsISupports *aOuter, 
	REFNSIID aIID, 
	void **aResult)
{
    nsStdURL* url = new nsStdURL(nsnull, aOuter);
    if (url == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(url);
    nsresult rv = url->QueryInterface(aIID, aResult);
    NS_RELEASE(url);
    return rv;
}

nsresult 
nsStdURL::ExtractString(char* *io_Destination, PRUint32 start, PRUint32 length)
{
    NS_PRECONDITION( (nsnull != mSpec), "Exract called on empty url!");
	if (*io_Destination)
		nsCRT::free(*io_Destination);
	*io_Destination = new char[length + 1];
	if (!*io_Destination)
		return NS_ERROR_OUT_OF_MEMORY;
	char* startPosition = mSpec + start;
	PL_strncpyz(*io_Destination, startPosition, length+1);
	return NS_OK;
}

PRInt32 ExtractPortFrom(char* src, int start, int length)
{
    char* port = new char[length +1];
	PRInt32 returnValue = -1;
    if (!port)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
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
	return NS_ERROR_NOT_IMPLEMENTED;
	//return DupString(&mDirectory, i_Directory);
}

NS_IMETHODIMP
nsStdURL::SetFileName(char* i_FileName)
{
	return NS_ERROR_NOT_IMPLEMENTED;
	//return DupString(&mFileName, i_FileName);
}

NS_IMETHODIMP
nsStdURL::SetRef(char* i_Ref)
{
	return NS_ERROR_NOT_IMPLEMENTED;
	//return DupString(&mRef, i_Ref);
}

NS_IMETHODIMP
nsStdURL::SetQuery(char* i_Query)
{
	return NS_ERROR_NOT_IMPLEMENTED;
	//return DupString(&mQuery, i_Query);
}

