/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsMIMEInfoImpl.h"
#include "nsCRT.h"


// nsISupports methods
NS_IMPL_ISUPPORTS(nsMIMEInfoImpl, nsCOMTypeInfo<nsIMIMEInfo>::GetIID());

// nsMIMEInfoImpl methods
nsMIMEInfoImpl::nsMIMEInfoImpl(const char *aMIMEType, const char *aFileExtensions, const char *aDescription) {
    NS_INIT_REFCNT();
    mMIMEType = NS_NewAtom(aMIMEType);
    mFileExtensions.SetString(aFileExtensions);
    mDescription.SetString(aDescription);
}

nsMIMEInfoImpl::~nsMIMEInfoImpl() {
    NS_RELEASE(mMIMEType);
}

PRBool
nsMIMEInfoImpl::InExtensions(nsIAtom* anIAtom) {
    nsAutoString extension;
    anIAtom->ToString(extension);
    
    // XXX this is broken. need to use gessner's tokenizer stuff to delimit the commas
    if (mFileExtensions.Find(extension) == -1)
        return PR_FALSE;
    return PR_TRUE;
}

// nsIMIMEInfo methods
NS_IMETHODIMP
nsMIMEInfoImpl::GetFileExtensions(char * *aFileExtensions) {
    if (!aFileExtensions) return NS_ERROR_NULL_POINTER;

    *aFileExtensions = mFileExtensions.ToNewCString();
    if (!*aFileExtensions)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::SetFileExtensions(char * aFileExtensions) {
    mFileExtensions.SetString(aFileExtensions);
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
nsMIMEInfoImpl::SetMIMEType(char * aMIMEType) {
    NS_RELEASE(mMIMEType);
    mMIMEType = NS_NewAtom(aMIMEType);
    if (!mMIMEType) return NS_ERROR_OUT_OF_MEMORY;
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
nsMIMEInfoImpl::SetDescription(PRUnichar * aDescription) {
    mDescription.SetString(aDescription);
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::Equals(nsIMIMEInfo *aMIMEInfo, PRBool *_retval) {
    return NS_ERROR_NOT_IMPLEMENTED;
}
