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

#include "nsSimpleURI.h"
#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "prmem.h"
#include "prprf.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kThisSimpleURIImplementationCID,
                     NS_THIS_SIMPLEURI_IMPLEMENTATION_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////////////
// nsSimpleURI methods:

nsSimpleURI::nsSimpleURI(nsISupports* outer)
    : mScheme(nsnull),
      mPath(nsnull)
{
    NS_INIT_AGGREGATED(outer);
}

nsSimpleURI::~nsSimpleURI()
{
    if (mScheme) nsCRT::free(mScheme);
    if (mPath)   nsCRT::free(mPath);
}

NS_IMPL_AGGREGATED(nsSimpleURI);

NS_IMETHODIMP
nsSimpleURI::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(kThisSimpleURIImplementationCID) ||        // used by Equals
        aIID.Equals(nsIURI::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsIURI*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

////////////////////////////////////////////////////////////////////////////////
// nsIURI methods:

NS_IMETHODIMP
nsSimpleURI::GetSpec(char* *result)
{
    nsAutoString string;
//    NS_LOCK_INSTANCE();

    string.SetString(mScheme);
    string.Append(':');
    string.Append(mPath);

//    NS_UNLOCK_INSTANCE();
    *result = string.ToNewCString();
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetSpec(char* aSpec)
{
    nsAutoString spec(aSpec);
    PRUint32 pos = spec.Find(":");
    if (pos == -1)
        return NS_ERROR_FAILURE;
    nsAutoString scheme;
    PRUint32 n = spec.Left(scheme, pos);
    NS_ASSERTION(n == pos, "Left failed");
    nsAutoString path;
    PRUint32 count = spec.Length() - pos - 1;
    n = spec.Mid(path, pos + 1, count);
    NS_ASSERTION(n == count, "Mid failed");
    mScheme = scheme.ToNewCString();
    if (mScheme == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    mPath = path.ToNewCString();
    if (mPath == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetScheme(char* *result)
{
    *result = nsCRT::strdup(mScheme);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetScheme(char* scheme)
{
    if (mScheme) nsCRT::free(mScheme);
    mScheme = nsCRT::strdup(scheme);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetPreHost(char* *result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetPreHost(char* preHost)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetHost(char* *result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetHost(char* host)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetPort(PRInt32 *result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetPort(PRInt32 port)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetPath(char* *result)
{
    *result = nsCRT::strdup(mPath);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetPath(char* path)
{
    if (mPath) nsCRT::free(mPath);
    mPath = nsCRT::strdup(path);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::Equals(nsIURI* other, PRBool *result)
{
    PRBool eq = PR_FALSE;
    if (other) {
//        NS_LOCK_INSTANCE();
        nsSimpleURI* otherUrl;
        nsresult rv =
            other->QueryInterface(kThisSimpleURIImplementationCID,
                                  (void**)&otherUrl);
        if (NS_SUCCEEDED(rv)) {
            eq = PRBool((0 == PL_strcmp(mScheme, otherUrl->mScheme)) && 
                        (0 == PL_strcmp(mPath, otherUrl->mPath)));
            NS_RELEASE(otherUrl);
        }
//        NS_UNLOCK_INSTANCE();
    }
    *result = eq;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::Clone(nsIURI* *result)
{
    nsSimpleURI* url = new nsSimpleURI(nsnull);     // XXX outer?
    if (url == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    url->mScheme = nsCRT::strdup(mScheme);
    if (url->mScheme == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    url->mPath = nsCRT::strdup(mPath);
    if (url->mPath == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    *result = url;
    NS_ADDREF(url);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////

NS_METHOD
nsSimpleURI::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsSimpleURI* url = new nsSimpleURI(aOuter);
    if (url == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(url);
    nsresult rv = url->QueryInterface(aIID, aResult);
    NS_RELEASE(url);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
