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

#include "nsRDFTreeDataModelItem.h"
#include "nsRDFTreeDataModel.h"

const PRUint32 nsRDFTreeDataModelItem::kInvalidIndentationLevel = PRUint32(-1);

static NS_DEFINE_IID(kIDMItemIID, NS_IDMITEM_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITreeDMItemIID, NS_ITREEDMITEM_IID);

////////////////////////////////////////////////////////////////////////

nsRDFTreeDataModelItem::nsRDFTreeDataModelItem(nsRDFTreeDataModel& tree,
                                               RDF_Resource resource)
    : nsRDFDataModelItem(tree, resource)
{
}


nsRDFTreeDataModelItem::~nsRDFTreeDataModelItem(void)
{
}


NS_IMETHODIMP_(nsrefcnt)
nsRDFTreeDataModelItem::AddRef(void)
{
    return nsRDFDataModelItem::AddRef();
}


NS_IMETHODIMP_(nsrefcnt)
nsRDFTreeDataModelItem::Release(void)
{
    return nsRDFDataModelItem::Release();
}


NS_IMETHODIMP
nsRDFTreeDataModelItem::QueryInterface(const nsIID& iid, void** result)
{
    if (NULL == result)
        return NS_ERROR_NULL_POINTER;

    *result = NULL;
    if (iid.Equals(kITreeDMItemIID)) {
        *result = static_cast<nsITreeDMItem*>(this);
        AddRef();
        return NS_OK;
    }

    // delegate to the superclass.
    return nsRDFDataModelItem::QueryInterface(iid, result);
}


////////////////////////////////////////////////////////////////////////
// nsIDMItem interface -- delgates to superclass

NS_IMETHODIMP
nsRDFTreeDataModelItem::GetIconImage(nsIImage*& image, nsIImageGroup* group) const
{
    return nsRDFDataModelItem::GetIconImage(image, group);
}

NS_IMETHODIMP
nsRDFTreeDataModelItem::GetOpenState(PRBool& result) const
{
    return nsRDFDataModelItem::GetOpenState(result);
}

NS_IMETHODIMP
nsRDFTreeDataModelItem::GetChildCount(PRUint32& count) const
{
    return nsRDFDataModelItem::GetChildCount(count);
}



NS_IMETHODIMP
nsRDFTreeDataModelItem::GetNthChild(nsIDMItem*& pItem, PRUint32 item) const
{
    return nsRDFDataModelItem::GetNthChild(pItem, item);
}


NS_IMETHODIMP
nsRDFTreeDataModelItem::GetParent(nsIDMItem*& pItem) const
{
    return nsRDFDataModelItem::GetParent(pItem);
}


NS_IMETHODIMP
nsRDFTreeDataModelItem::GetStringPropertyValue(nsString& result, const nsString& property) const
{
    return nsRDFDataModelItem::GetStringPropertyValue(result, property);
}


NS_IMETHODIMP
nsRDFTreeDataModelItem::GetIntPropertyValue(PRInt32& value, const nsString& itemProperty) const
{
    return nsRDFDataModelItem::GetIntPropertyValue(value, itemProperty);
}


////////////////////////////////////////////////////////////////////////
// nsITreeDMItem interface

NS_IMETHODIMP
nsRDFTreeDataModelItem::GetTriggerImage(nsIImage*& pImage, nsIImageGroup* pGroup) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFTreeDataModelItem::GetIndentationLevel(PRUint32& indentation) const
{
    PRUint32 i = 0;

    nsRDFDataModelItem* item =
        static_cast<nsRDFDataModelItem*>(const_cast<nsRDFTreeDataModelItem*>(this));

    while ((item = item->GetParent()) != NULL)
        i += 1; // XXX should this be some pixel value?

    indentation = i;
    return NS_OK;
}
