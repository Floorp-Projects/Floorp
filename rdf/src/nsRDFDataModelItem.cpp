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

#include "nsRDFDataModel.h"
#include "nsRDFDataModelItem.h"
#include "rdf-int.h"

static NS_DEFINE_IID(kISupportsIID,    NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDMItemIID,      NS_IDMITEM_IID);
static NS_DEFINE_IID(kIRDFResourceIID, NS_IDMITEM_IID);

////////////////////////////////////////////////////////////////////////

nsRDFDataModelItem::nsRDFDataModelItem(nsRDFDataModel& model, RDF_Resource resource)
    : mDataModel(model),
      mResource(resource),
      mOpen(PR_FALSE),
      mParent(NULL),
      mCachedSubtreeSize(1)
{
    NS_INIT_REFCNT();
}


nsRDFDataModelItem::~nsRDFDataModelItem(void)
{
    for (PRUint32 i = 0; i < mChildren.GetSize(); ++i) {
        nsRDFDataModelItem* child = static_cast<nsRDFDataModelItem*>(mChildren[i]);
        child->Release();
    }
}

NS_IMPL_ADDREF(nsRDFDataModelItem);
NS_IMPL_RELEASE(nsRDFDataModelItem);

NS_IMETHODIMP
nsRDFDataModelItem::QueryInterface(const nsIID& iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = NULL;
    if (iid.Equals(kIDMItemIID) ||
        iid.Equals(kISupportsIID)) {
        *result = static_cast<nsIDMItem*>(this);
        AddRef();
        return NS_OK;
    }
    else if (iid.Equals(kIRDFResourceIID)) {
        *result = static_cast<nsIRDFResource*>(this);
        AddRef();
        return NS_OK;
    }
    return NS_ERROR_NO_INTERFACE;
}


////////////////////////////////////////////////////////////////////////
// nsIDMItem interface

NS_IMETHODIMP
nsRDFDataModelItem::GetIconImage(nsIImage*& image, nsIImageGroup* group) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
nsRDFDataModelItem::GetOpenState(PRBool& result) const
{
    result = IsOpen();
    return NS_OK;
}

// XXX I assume this is coming...
NS_IMETHODIMP
nsRDFDataModelItem::SetOpenState(PRBool open)
{
    if (open)
        Open();
    else
        Close();

    return NS_OK;
}

NS_IMETHODIMP
nsRDFDataModelItem::GetChildCount(PRUint32& count) const
{
    count = mChildren.GetSize();
    return NS_OK;
}



NS_IMETHODIMP
nsRDFDataModelItem::GetNthChild(nsIDMItem*& pItem, PRUint32 n) const
{
    if (n < 0 || n > mChildren.GetUpperBound())
        return NS_ERROR_UNEXPECTED; // XXX

    pItem = static_cast<nsIDMItem*>(mChildren[n]);
    pItem->AddRef();

    return NS_OK;
}


NS_IMETHODIMP
nsRDFDataModelItem::GetSubtreeSize(PRUint32& result) const
{
    if (! mCachedSubtreeSize) {
        mCachedSubtreeSize = 1; // me

        for (PRUint32 i = 0; i < mChildren.GetSize(); ++i) {
            nsRDFDataModelItem* child =
                static_cast<nsRDFDataModelItem*>(mChildren[i]);

            PRUint32 childSubtreeSize;
            child->GetSubtreeSize(childSubtreeSize);
            mCachedSubtreeSize += childSubtreeSize;
        }
    }
    result = mCachedSubtreeSize;
    return NS_OK;
}


NS_IMETHODIMP
nsRDFDataModelItem::GetParent(nsIDMItem*& pItem) const
{
    pItem = static_cast<nsIDMItem*>(mParent);
    return NS_OK;
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


////////////////////////////////////////////////////////////////////////
// nsIRDFResource interface

NS_IMETHODIMP
nsRDFDataModelItem::GetResource(RDF_Resource& resource /* out */) const
{
    resource = mResource;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// Implementation methods

void
nsRDFDataModelItem::InvalidateCachedSubtreeSize(void)
{
    if (mCachedSubtreeSize) {
        mCachedSubtreeSize = 0;
        if (mParent)
            mParent->InvalidateCachedSubtreeSize();
    }
}



void
nsRDFDataModelItem::AddChild(nsRDFDataModelItem* child)
{
    mChildren.Add(child);
    nsRDFDataModelItem* item = this;
    while (item) {
        PRUint32 subtreeSize;
        child->GetSubtreeSize(subtreeSize);
        item->mCachedSubtreeSize += subtreeSize;
        item = item->mParent;
    }
}


void
nsRDFDataModelItem::Open(void)
{
    RDF_Cursor cursor;
    if (mDataModel.GetArcType() == eRDFArcType_Outbound) {
        // Arcs are outbound, that is, from a parent to it's
        // child. Find all targets whose source is "me".
        cursor = RDF_GetTargets(mDataModel.GetDB(),
                                mResource,
                                mDataModel.GetArcProperty(),
                                RDF_RESOURCE_TYPE,
                                PR_TRUE);
    }
    else {
        // Arcs are inbound, that is, from a child to it's
        // parent. Find all sources whose target is "me".
        cursor = RDF_GetSources(mDataModel.GetDB(),
                                mResource,
                                mDataModel.GetArcProperty(),
                                RDF_RESOURCE_TYPE,
                                PR_TRUE);
    }

    if (cursor) {
        RDF_Resource r;
        while ((r = static_cast<RDF_Resource>(RDF_NextValue(cursor))) != NULL) {
            nsRDFDataModelItem* child;
            if (NS_FAILED(mDataModel.CreateItem(r, child)))
                continue;

            // CreateItem() will have done an AddRef(), like a good
            // COM citizen...
            mChildren.Add(child);
            child->mParent = this;
        }
        RDF_DisposeCursor(cursor);
    }
    
    InvalidateCachedSubtreeSize();
    mOpen = PR_TRUE;
}


void
nsRDFDataModelItem::Close(void)
{
    for (PRUint32 i = 0; i < mChildren.GetSize(); ++i) {
        nsRDFDataModelItem* child = static_cast<nsRDFDataModelItem*>(mChildren[i]);
        child->Release();
    }

    InvalidateCachedSubtreeSize();
    mOpen = PR_FALSE;
}
