/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Contributor(s): Judson Valeski
 */

#include "nsMIMEInfoImpl.h"
#include "nsXPIDLString.h"

// nsISupports methods
NS_IMPL_THREADSAFE_ISUPPORTS1(nsMIMEInfoImpl, nsIMIMEInfo);

// nsMIMEInfoImpl methods
nsMIMEInfoImpl::nsMIMEInfoImpl(const char *aMIMEType) {
    NS_INIT_REFCNT();
    mMIMEType = getter_AddRefs(NS_NewAtom(aMIMEType));
}

PRUint32
nsMIMEInfoImpl::GetExtCount() {
    return mExtensions.Count();
}

NS_IMETHODIMP
nsMIMEInfoImpl::GetFileExtensions(PRInt32 *elementCount, char ***extensions) {
    *elementCount = mExtensions.Count();
    if (*elementCount < 1) return NS_OK;;

    char **_retExts = (char**)nsAllocator::Alloc((*elementCount)*2*sizeof(char*));
    if (!_retExts) return NS_ERROR_OUT_OF_MEMORY;

    for (PRUint8 i=0; i < *elementCount; i++) {
        nsString* ext = (nsString*)mExtensions.CStringAt(i);
        _retExts[i] = ext->ToNewCString();
        if (!_retExts[i]) return NS_ERROR_OUT_OF_MEMORY;
    }

    *extensions   = _retExts;

    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::ExtensionExists(const char *aExtension, PRBool *_retval) {
    NS_ASSERTION(aExtension, "no extension");
    PRBool found = PR_FALSE;
    PRUint32 extCount = mExtensions.Count();
    if (extCount < 1) return NS_ERROR_NOT_INITIALIZED;

    if (!aExtension) return NS_ERROR_NULL_POINTER;

    for (PRUint8 i=0; i < extCount; i++) {
        nsCString* ext = mExtensions.CStringAt(i);
        if (ext->Equals(aExtension)) {
            found = PR_TRUE;
            break;
        }
    }

    *_retval = found;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::FirstExtension(char **_retval) {
    PRUint32 extCount = mExtensions.Count();
    if (extCount < 1) return NS_ERROR_NOT_INITIALIZED;

    *_retval = (mExtensions.CStringAt(0))->ToNewCString();
    if (!*_retval) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;    
}

NS_IMETHODIMP
nsMIMEInfoImpl::GetMIMEType(char * *aMIMEType) {
    if (!aMIMEType) return NS_ERROR_NULL_POINTER;

    nsAutoString strMIMEType;
    mMIMEType->ToString(strMIMEType);
    *aMIMEType = strMIMEType.ToNewCString();
    if (!*aMIMEType) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::GetDescription(PRUnichar * *aDescription) {
    if (!aDescription) return NS_ERROR_NULL_POINTER;

    *aDescription = mDescription.ToNewUnicode();
    if (!*aDescription) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::GetDataURI(nsIURI * *aDataURI) {
    return mURI->Clone(aDataURI);
}

NS_IMETHODIMP
nsMIMEInfoImpl::Equals(nsIMIMEInfo *aMIMEInfo, PRBool *_retval) {
    if (!aMIMEInfo) return NS_ERROR_NULL_POINTER;

    nsXPIDLCString type;
    nsresult rv = aMIMEInfo->GetMIMEType(getter_Copies(type));
    if (NS_FAILED(rv)) return rv;

      // STRING USE WARNING: perhaps |type1| should be an |nsCAutoString|? -- scc
    nsAutoString type1;
    mMIMEType->ToString(type1);
    *_retval = type1.EqualsWithConversion(type);
    return NS_OK;
}
