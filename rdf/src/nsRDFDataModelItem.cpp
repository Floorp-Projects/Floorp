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

#include "nsRDFDataModelItem.h"

static NS_DEFINE_IID(kIDMItemIID, NS_IDMITEM_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsRDFDataModelItem::nsRDFDataModelItem(RDF_Resource& resource)
    : mResource(resource)
{
    NS_INIT_REFCNT();
}


nsRDFDataModelItem::~nsRDFDataModelItem(void)
{
}


NS_IMPL_ADDREF(nsRDFDataModelItem);
NS_IMPL_RELEASE(nsRDFDataModelItem);

NS_IMETHODIMP
nsRDFDataModelItem::QueryInterface(const nsIID& iid, void** result)
{
    if (NULL == result) {
        return NS_ERROR_NULL_POINTER;
    }

    *result = NULL;
    if (iid.Equals(kIDMItemIID) ||
        iid.Equals(kISupportsIID)) {
        *result = static_cast<nsIDMItem*>(this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP
nsRDFDataModelItem::GetIconImage(nsIImage*& image, nsIImageGroup* group) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
nsRDFDataModelItem::GetOpenState(PRBool& result) const
{
    result = mOpen;
    return NS_OK;
}

NS_IMETHODIMP
nsRDFDataModelItem::GetChildCount(PRUint32& count) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
nsRDFDataModelItem::GetNthChild(nsIDMItem*& pItem, PRUint32 item) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFDataModelItem::GetParent(nsIDMItem*& pItem) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFDataModelItem::GetStringPropertyValue(nsString& result, const nsString& property) const
{
    // 1. convert the property to a URI
    // 2. ask the RDF database for the value of the property
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFDataModelItem::GetIntPropertyValue(PRInt32& value, const nsString& itemProperty) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
