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

#include "nsRDFToolbarDataModelItem.h"
#include "nsRDFToolbarDataModel.h"

static NS_DEFINE_IID(kIToolbarDMItemIID, NS_ITOOLBARDMITEM_IID);
static NS_DEFINE_IID(kIDMItemIID, NS_ITOOLBARDMITEM_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////

nsRDFToolbarDataModelItem::nsRDFToolbarDataModelItem(nsRDFToolbarDataModel& toolbar,
                                                     RDF_Resource resource)
    : nsRDFDataModelItem(toolbar, resource)
{
}


nsRDFToolbarDataModelItem::~nsRDFToolbarDataModelItem(void)
{
}

NS_IMETHODIMP_(nsrefcnt)
nsRDFToolbarDataModelItem::AddRef(void)
{
    return nsRDFDataModelItem::AddRef();
}


NS_IMETHODIMP_(nsrefcnt)
nsRDFToolbarDataModelItem::Release(void)
{
    return nsRDFDataModelItem::Release();
}

NS_IMETHODIMP
nsRDFToolbarDataModelItem::QueryInterface(const nsIID& iid, void** result)
{
    if (NULL == result)
        return NS_ERROR_NULL_POINTER;

    *result = NULL;
    if (iid.Equals(kIToolbarDMItemIID)) {
        *result = static_cast<nsIToolbarDMItem*>(this);
        AddRef();
        return NS_OK;
    }

    // delegate to the superclass.
    return nsRDFDataModelItem::QueryInterface(iid, result);
}


////////////////////////////////////////////////////////////////////////

#if 0
NS_IMETHODIMP
nsRDFToolbarDataModelItem::GetIconImage(nsIImage*& image, nsIImageGroup* group) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
nsRDFToolbarDataModelItem::GetOpenState(PRBool& result) const
{
    result = IsOpen();
    return NS_OK;
}

NS_IMETHODIMP
nsRDFToolbarDataModelItem::GetChildCount(PRUint32& count) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
nsRDFToolbarDataModelItem::GetNthChild(nsIDMItem*& pItem, PRUint32 item) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFToolbarDataModelItem::GetParent(nsIDMItem*& pItem) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFToolbarDataModelItem::GetStringPropertyValue(nsString& result, const nsString& property) const
{
    // 1. convert the property to a URI
    // 2. ask the RDF database for the value of the property
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFToolbarDataModelItem::GetIntPropertyValue(PRInt32& value, const nsString& itemProperty) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
#endif
