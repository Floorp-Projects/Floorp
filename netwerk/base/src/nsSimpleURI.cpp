/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Gagan Saksena <gagan@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSimpleURI.h"
#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "prmem.h"
#include "prprf.h"
#include "nsURLHelper.h"
#include "nsNetCID.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"

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
    NS_ENSURE_ARG_POINTER(aInstancePtr);

    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = GetInner();
    } else if (aIID.Equals(kThisSimpleURIImplementationCID) || // used by Equals
               aIID.Equals(NS_GET_IID(nsIURI))) {
        *aInstancePtr = NS_STATIC_CAST(nsIURI*, this);
    } else if (aIID.Equals(NS_GET_IID(nsISerializable))) {
        *aInstancePtr = NS_STATIC_CAST(nsISerializable*, this);
    } else {
        *aInstancePtr = nsnull;
        return NS_NOINTERFACE;
    }
    NS_ADDREF((nsISupports*)*aInstancePtr);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsISerializable methods:

NS_IMETHODIMP
nsSimpleURI::Read(nsIObjectInputStream* aStream)
{
    nsresult rv;

    rv = aStream->ReadStringZ(&mScheme);
    if (NS_FAILED(rv)) return rv;

    rv = aStream->ReadStringZ(&mPath);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::Write(nsIObjectOutputStream* aStream)
{
    nsresult rv;

    rv = aStream->WriteStringZ(mScheme);
    if (NS_FAILED(rv)) return rv;

    rv = aStream->WriteStringZ(mPath);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIURI methods:

NS_IMETHODIMP
nsSimpleURI::GetSpec(char* *result)
{
    nsAutoString string;
//    NS_LOCK_INSTANCE();

      // STRING USE WARNING: perhaps |string| should be |nsCAutoString|? -- scc
    string.AssignWithConversion(mScheme);
    string.AppendWithConversion(':');
    string.AppendWithConversion(mPath);

//    NS_UNLOCK_INSTANCE();
    *result = string.ToNewCString();
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetSpec(const char* aSpec)
{
    nsAutoString spec;
    spec.AssignWithConversion(aSpec);

    PRInt32 pos = spec.Find(":");
    if (pos == -1)
        return NS_ERROR_FAILURE;
    nsAutoString scheme;
    PRInt32 n = spec.Left(scheme, pos);
    NS_ASSERTION(n == pos, "Left failed");
    nsAutoString path;
    PRInt32 count = spec.Length() - pos - 1;
    n = spec.Mid(path, pos + 1, count);
    NS_ASSERTION(n == count, "Mid failed");
    if (mScheme) 
        nsCRT::free(mScheme);
    mScheme = scheme.ToNewCString();
    if (mScheme == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    ToLowerCase(mScheme);
    if (mPath)   
        nsCRT::free(mPath);
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
nsSimpleURI::SetScheme(const char* scheme)
{
    if (mScheme) nsCRT::free(mScheme);
    mScheme = nsCRT::strdup(scheme);
    ToLowerCase(mScheme);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetPrePath(char* *result)
{
    nsCAutoString prePath(mScheme);
    prePath += ":";
    *result = prePath.ToNewCString();
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsSimpleURI::SetPrePath(const char* scheme)
{
    NS_NOTREACHED("nsSimpleURI::SetPrePath");
    return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsSimpleURI::GetPreHost(char* *result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetPreHost(const char* preHost)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetUsername(char* *result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetUsername(const char* userName)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetPassword(char* *result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetPassword(const char* password)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetHost(char* *result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetHost(const char* host)
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
nsSimpleURI::SetPath(const char* path)
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
nsSimpleURI::SchemeIs(const char *i_Scheme, PRBool *o_Equals)
{
    NS_ENSURE_ARG_POINTER(o_Equals);
    if (!i_Scheme) return NS_ERROR_NULL_POINTER;

    // mScheme is guaranteed to be lower case.
    if (*i_Scheme == *mScheme || *i_Scheme == (*mScheme - ('a' - 'A')) ) {
        *o_Equals = PL_strcasecmp(mScheme, i_Scheme) ? PR_FALSE : PR_TRUE;
    } else {
        *o_Equals = PR_FALSE;
    }

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

NS_IMETHODIMP
nsSimpleURI::Resolve(const char *relativePath, char **result) 
{
    return DupString(result,(char*)relativePath);
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD
nsSimpleURI::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
     NS_ENSURE_PROPER_AGGREGATION(aOuter, aIID);

    nsSimpleURI* url = new nsSimpleURI(aOuter);
    if (url == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = url->AggregatedQueryInterface(aIID, aResult);

     if (NS_FAILED(rv))
         delete url;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
