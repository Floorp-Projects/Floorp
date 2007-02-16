/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSimpleURI.h"
#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "prmem.h"
#include "prprf.h"
#include "nsURLHelper.h"
#include "nsNetCID.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsEscape.h"
#include "nsNetError.h"
#include "nsIProgrammingLanguage.h"

static NS_DEFINE_CID(kThisSimpleURIImplementationCID,
                     NS_THIS_SIMPLEURI_IMPLEMENTATION_CID);
static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

////////////////////////////////////////////////////////////////////////////////
// nsSimpleURI methods:

nsSimpleURI::nsSimpleURI(nsISupports* outer)
    : mMutable(PR_TRUE)
{
    NS_INIT_AGGREGATED(outer);
}

nsSimpleURI::~nsSimpleURI()
{
}

NS_IMPL_AGGREGATED(nsSimpleURI)

nsresult
nsSimpleURI::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);

    if (aIID.Equals(NS_GET_IID(nsISupports))) {
        *aInstancePtr = InnerObject();
    } else if (aIID.Equals(kThisSimpleURIImplementationCID) || // used by Equals
               aIID.Equals(NS_GET_IID(nsIURI))) {
        *aInstancePtr = NS_STATIC_CAST(nsIURI*, this);
    } else if (aIID.Equals(NS_GET_IID(nsISerializable))) {
        *aInstancePtr = NS_STATIC_CAST(nsISerializable*, this);
    } else if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
        *aInstancePtr = NS_STATIC_CAST(nsIClassInfo*, this);
    } else if (aIID.Equals(NS_GET_IID(nsIMutable))) {
        *aInstancePtr = NS_STATIC_CAST(nsIMutable*, this);
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

    rv = aStream->ReadBoolean(&mMutable);
    if (NS_FAILED(rv)) return rv;

    rv = aStream->ReadCString(mScheme);
    if (NS_FAILED(rv)) return rv;

    rv = aStream->ReadCString(mPath);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::Write(nsIObjectOutputStream* aStream)
{
    nsresult rv;

    rv = aStream->WriteBoolean(mMutable);
    if (NS_FAILED(rv)) return rv;

    rv = aStream->WriteStringZ(mScheme.get());
    if (NS_FAILED(rv)) return rv;

    rv = aStream->WriteStringZ(mPath.get());
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIURI methods:

NS_IMETHODIMP
nsSimpleURI::GetSpec(nsACString &result)
{
    result = mScheme + NS_LITERAL_CSTRING(":") + mPath;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetSpec(const nsACString &aSpec)
{
    NS_ENSURE_STATE(mMutable);
    
    const nsAFlatCString& flat = PromiseFlatCString(aSpec);
    const char* specPtr = flat.get();

    // filter out unexpected chars "\r\n\t" if necessary
    nsCAutoString filteredSpec;
    PRInt32 specLen;
    if (net_FilterURIString(specPtr, filteredSpec)) {
        specPtr = filteredSpec.get();
        specLen = filteredSpec.Length();
    } else
        specLen = flat.Length();

    // nsSimpleURI currently restricts the charset to US-ASCII
    nsCAutoString spec;
    NS_EscapeURL(specPtr, specLen, esc_OnlyNonASCII|esc_AlwaysCopy, spec);

    PRInt32 pos = spec.FindChar(':');
    if (pos == -1 || !net_IsValidScheme(spec.get(), pos))
        return NS_ERROR_MALFORMED_URI;

    mScheme.Truncate();
    mPath.Truncate();

    PRInt32 n = spec.Left(mScheme, pos);
    NS_ASSERTION(n == pos, "Left failed");

    PRInt32 count = spec.Length() - pos - 1;
    n = spec.Mid(mPath, pos + 1, count);
    NS_ASSERTION(n == count, "Mid failed");

    ToLowerCase(mScheme);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetScheme(nsACString &result)
{
    result = mScheme;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetScheme(const nsACString &scheme)
{
    NS_ENSURE_STATE(mMutable);

    const nsPromiseFlatCString &flat = PromiseFlatCString(scheme);
    if (!net_IsValidScheme(flat)) {
        NS_ERROR("the given url scheme contains invalid characters");
        return NS_ERROR_MALFORMED_URI;
    }

    mScheme = scheme;
    ToLowerCase(mScheme);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetPrePath(nsACString &result)
{
    result = mScheme + NS_LITERAL_CSTRING(":");
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetUserPass(nsACString &result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetUserPass(const nsACString &userPass)
{
    NS_ENSURE_STATE(mMutable);
    
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetUsername(nsACString &result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetUsername(const nsACString &userName)
{
    NS_ENSURE_STATE(mMutable);
    
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetPassword(nsACString &result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetPassword(const nsACString &password)
{
    NS_ENSURE_STATE(mMutable);
    
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetHostPort(nsACString &result)
{
    // Note: Audit all callers before changing this to return an empty
    // string -- CAPS and UI code may depend on this throwing.
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetHostPort(const nsACString &result)
{
    NS_ENSURE_STATE(mMutable);
    
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetHost(nsACString &result)
{
    // Note: Audit all callers before changing this to return an empty
    // string -- CAPS and UI code depend on this throwing.
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetHost(const nsACString &host)
{
    NS_ENSURE_STATE(mMutable);
    
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetPort(PRInt32 *result)
{
    // Note: Audit all callers before changing this to return an empty
    // string -- CAPS and UI code may depend on this throwing.
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetPort(PRInt32 port)
{
    NS_ENSURE_STATE(mMutable);
    
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetPath(nsACString &result)
{
    result = mPath;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetPath(const nsACString &path)
{
    NS_ENSURE_STATE(mMutable);
    
    mPath = path;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::Equals(nsIURI* other, PRBool *result)
{
    PRBool eq = PR_FALSE;
    if (other) {
        nsSimpleURI* otherUrl;
        nsresult rv =
            other->QueryInterface(kThisSimpleURIImplementationCID,
                                  (void**)&otherUrl);
        if (NS_SUCCEEDED(rv)) {
            eq = PRBool((0 == strcmp(mScheme.get(), otherUrl->mScheme.get())) && 
                        (0 == strcmp(mPath.get(), otherUrl->mPath.get())));
            NS_RELEASE(otherUrl);
        }
    }
    *result = eq;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SchemeIs(const char *i_Scheme, PRBool *o_Equals)
{
    NS_ENSURE_ARG_POINTER(o_Equals);
    if (!i_Scheme) return NS_ERROR_NULL_POINTER;

    const char *this_scheme = mScheme.get();

    // mScheme is guaranteed to be lower case.
    if (*i_Scheme == *this_scheme || *i_Scheme == (*this_scheme - ('a' - 'A')) ) {
        *o_Equals = PL_strcasecmp(this_scheme, i_Scheme) ? PR_FALSE : PR_TRUE;
    } else {
        *o_Equals = PR_FALSE;
    }

    return NS_OK;
}

/* virtual */ nsSimpleURI*
nsSimpleURI::StartClone()
{
    return new nsSimpleURI(nsnull);     // XXX outer?
}

NS_IMETHODIMP
nsSimpleURI::Clone(nsIURI* *result)
{
    nsSimpleURI* url = StartClone();
    if (url == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    // Note: |url| may well have mMutable false at this point, so
    // don't call any setter methods.
    url->mScheme = mScheme;
    url->mPath = mPath;

    *result = url;
    NS_ADDREF(url);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::Resolve(const nsACString &relativePath, nsACString &result) 
{
    result = relativePath;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetAsciiSpec(nsACString &result)
{
    nsCAutoString buf;
    nsresult rv = GetSpec(buf);
    if (NS_FAILED(rv)) return rv;
    NS_EscapeURL(buf, esc_OnlyNonASCII|esc_AlwaysCopy, result);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetAsciiHost(nsACString &result)
{
    result.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetOriginCharset(nsACString &result)
{
    result.Truncate();
    return NS_OK;
}

//----------------------------------------------------------------------------
// nsSimpleURI::nsIClassInfo
//----------------------------------------------------------------------------

NS_IMETHODIMP 
nsSimpleURI::GetInterfaces(PRUint32 *count, nsIID * **array)
{
    *count = 0;
    *array = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsSimpleURI::GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
{
    *_retval = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsSimpleURI::GetContractID(char * *aContractID)
{
    // Make sure to modify any subclasses as needed if this ever
    // changes.
    *aContractID = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsSimpleURI::GetClassDescription(char * *aClassDescription)
{
    *aClassDescription = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsSimpleURI::GetClassID(nsCID * *aClassID)
{
    // Make sure to modify any subclasses as needed if this ever
    // changes to not call the virtual GetClassIDNoAlloc.
    *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
    if (!*aClassID)
        return NS_ERROR_OUT_OF_MEMORY;
    return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP 
nsSimpleURI::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

NS_IMETHODIMP 
nsSimpleURI::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
    return NS_OK;
}

NS_IMETHODIMP 
nsSimpleURI::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    *aClassIDNoAlloc = kSimpleURICID;
    return NS_OK;
}

//----------------------------------------------------------------------------
// nsSimpleURI::nsISimpleURI
//----------------------------------------------------------------------------
NS_IMETHODIMP
nsSimpleURI::GetMutable(PRBool *value)
{
    *value = mMutable;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetMutable(PRBool value)
{
    NS_ENSURE_ARG(mMutable || !value);

    mMutable = value;
    return NS_OK;
}

