/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsRDFTreeDataModel.h"
#include "nsRDFTreeColumn.h"
#include "nsRDFTreeDataModelItem.h"
#include "rdf-int.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDataModelIID, NS_IDATAMODEL_IID);
static NS_DEFINE_IID(kITreeDataModelIID, NS_ITREEDATAMODEL_IID);


////////////////////////////////////////////////////////////////////////

nsRDFTreeDataModel::nsRDFTreeDataModel(void)
{
}



nsRDFTreeDataModel::~nsRDFTreeDataModel(void)
{
    for (PRUint32 i = 0; i < mColumns.GetSize(); ++i) {
        nsRDFTreeColumn* column = static_cast<nsRDFTreeColumn*>(mColumns[i]);

        PR_ASSERT(column);
        if (! column)
            continue;

        column->Release();
    }
}

NS_IMETHODIMP_(nsrefcnt)
nsRDFTreeDataModel::AddRef(void)
{
    return nsRDFDataModel::AddRef(); // delegate to the superclass
}

NS_IMETHODIMP_(nsrefcnt)
nsRDFTreeDataModel::Release(void)
{
    return nsRDFDataModel::Release(); // delegate to the superclass
}

NS_IMETHODIMP
nsRDFTreeDataModel::QueryInterface(const nsIID& iid, void** result)
{
    if (NULL == result)
        return NS_ERROR_NULL_POINTER;

    *result = NULL;
    if (iid.Equals(kITreeDataModelIID)) {
        *result = static_cast<nsITreeDataModel*>(this);
        AddRef();
        return NS_OK;
    }
    return nsRDFDataModel::QueryInterface(iid, result);
}

////////////////////////////////////////////////////////////////////////
// nsIDataModel implementation -- delgates to superclass

NS_IMETHODIMP
nsRDFTreeDataModel::InitFromURL(const nsString& url)
{
    return nsRDFDataModel::InitFromURL(url);
}


NS_IMETHODIMP
nsRDFTreeDataModel::InitFromResource(nsIDMItem* pResource)
{
    return nsRDFDataModel::InitFromResource(pResource);
}


NS_IMETHODIMP
nsRDFTreeDataModel::GetDMWidget(nsIDMWidget*& widget) const
{
    return nsRDFDataModel::GetDMWidget(widget);
}


NS_IMETHODIMP
nsRDFTreeDataModel::SetDMWidget(nsIDMWidget* widget)
{
    return nsRDFDataModel::SetDMWidget(widget);
}



NS_IMETHODIMP
nsRDFTreeDataModel::GetStringPropertyValue(nsString& value, const nsString& property) const
{
    return nsRDFDataModel::GetStringPropertyValue(value, property);
}


NS_IMETHODIMP
nsRDFTreeDataModel::GetIntPropertyValue(PRInt32& value, const nsString& property) const
{
    return nsRDFDataModel::GetIntPropertyValue(value, property);
}

////////////////////////////////////////////////////////////////////////
// nsITreeDataModel interface

NS_IMETHODIMP
nsRDFTreeDataModel::GetVisibleColumnCount(PRUint32& count) const
{
    count = 0;
    for (PRUint32 i = 0; i < mColumns.GetSize(); ++i) {
        nsRDFTreeColumn* column = static_cast<nsRDFTreeColumn*>(mColumns.Get(i));
        if (column->IsVisible())
            ++count;
    }
    return NS_OK;
}


NS_IMETHODIMP
nsRDFTreeDataModel::GetColumnCount(PRUint32& count) const
{
    return mColumns.GetSize();
}


NS_IMETHODIMP
nsRDFTreeDataModel::GetNthColumn(nsITreeColumn*& pColumn, PRUint32 n) const
{
    if (n < 0 || n > mColumns.GetUpperBound()) {
        PR_ASSERT(0);
        return NS_ERROR_ILLEGAL_VALUE;
    }

    pColumn = static_cast<nsITreeColumn*>(mColumns.Get(n));
    return NS_OK;
}
	

NS_IMETHODIMP
nsRDFTreeDataModel::GetFirstVisibleItemIndex(PRUint32& index) const
{
    // XXX hack! where do we ever set this???
    index = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsRDFTreeDataModel::GetNthTreeItem(nsITreeDMItem*& pItem, PRUint32 n) const
{
    
    return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef DYNAMIC_CASTING
#define DYNAMIC_CAST(__type, __pointer) dynamic_cast<__type>(__pointer)
#else
#define DYNAMIC_CAST(__type, __pointer) ((__type)(__pointer))
#endif

NS_IMETHODIMP
nsRDFTreeDataModel::GetItemTextForColumn(nsString& nodeText,
                                         nsITreeDMItem* pItem,
                                         nsITreeColumn* pColumn) const
{
    // XXX this is a horribly simplified version of what
    // HT_GetNodeData() does. Hopefully it's enough to do the job for
    // now.

    // XXX may need to turn off the dynamic_cast stuff...
    nsRDFTreeDataModelItem* item =
        DYNAMIC_CAST(nsRDFTreeDataModelItem*, pItem);

    PR_ASSERT(item);
    if (! item)
        return NS_ERROR_UNEXPECTED; // XXX

    nsRDFTreeColumn* column =
        DYNAMIC_CAST(nsRDFTreeColumn*, pColumn);

    PR_ASSERT(column);
    if (! column)
        return NS_ERROR_UNEXPECTED; // XXX
    
    const char* result =
        (const char*) RDF_GetSlotValue(GetDB(),
                                       item->GetResource(),   // resource
                                       column->GetProperty(), // property
                                       RDF_STRING_TYPE,
                                       PR_FALSE,
                                       PR_TRUE);

    PR_ASSERT(result);
    if (! result)
        return NS_ERROR_NULL_POINTER;

    nodeText = result;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsRDFTreeDataModel implementation methods

void
nsRDFTreeDataModel::CreateColumns(void)
{
    // This mostly came over from HT_NewView()
    nsRDFDataModelItem* root = GetRoot();
    PR_ASSERT(root);
    if (! root)
        return;

    RDF_Cursor cursor;
    cursor = RDF_GetTargets(GetDB(), root->GetResource(),
                            gNavCenter->RDF_Column,
                            RDF_RESOURCE_TYPE, PR_TRUE);

    PR_ASSERT(cursor != NULL);
    if (cursor == NULL)
        return;

    RDF_Resource r;
    while ((r = static_cast<RDF_Resource>(RDF_NextValue(cursor))) != NULL) {
        if (resourceID(r) == NULL)
            break;

#if 0 // XXX no datatype support yet...
        PRUint32 type;
        type = (PRUint32) RDF_GetSlotValue(GetDB(), r, gNavCenter->RDF_ColumnDataType,
                                           RDF_INT_TYPE, PR_FALSE, PR_TRUE);

        if (!type)
            type = HT_COLUMN_STRING;
#endif

        PRUint32 width;
        width = (PRUint32) RDF_GetSlotValue(GetDB(), r, gNavCenter->RDF_ColumnWidth,
                                            RDF_INT_TYPE, PR_FALSE, PR_TRUE);

        const char* name;
        name = (const char *) RDF_GetSlotValue(GetDB(), r, gCoreVocab->RDF_name,
                                               RDF_STRING_TYPE, PR_FALSE, PR_TRUE);

        nsRDFTreeColumn* column = new nsRDFTreeColumn(*this, name, r, width);

        PR_ASSERT(column);
        if (! column)
            continue;

        column->AddRef();
        mColumns.Add(column);
    }	
    RDF_DisposeCursor(cursor);
}

void
nsRDFTreeDataModel::AddColumn(const nsString& name, RDF_Resource property)
{
    nsRDFTreeColumn* column = new nsRDFTreeColumn(*this, name, property);
    mColumns.Add(column);
}


NS_METHOD
nsRDFTreeDataModel::CreateItem(RDF_Resource r, nsRDFDataModelItem*& result)
{
    result = new nsRDFTreeDataModelItem(*this, r);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}
