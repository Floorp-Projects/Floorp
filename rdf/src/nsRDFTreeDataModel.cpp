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

static NS_DEFINE_IID(kITreeDataModelIID, NS_ITREEDATAMODEL_IID);


////////////////////////////////////////////////////////////////////////

nsRDFTreeDataModel::nsRDFTreeDataModel(nsIRDFDataBase& db, RDF_Resource& root)
    : nsRDFDataModel(db), mRoot(root)
{
}



nsRDFTreeDataModel::~nsRDFTreeDataModel(void)
{
}


NS_IMETHODIMP
nsRDFTreeDataModel::QueryInterface(const nsIID& iid, void** result)
{
    if (NULL == result)
        return NS_ERROR_NULL_POINTER;

    *result = NULL;
    if (iid.Equals(kITreeDataModelIID)) {
        *result = static_cast<nsITreeDataModel*>(this);
        nsRDFDataModel::AddRef(); // delegate to the superclass
        return NS_OK;
    }

    // delegate to the superclass.
    return nsRDFDataModel::QueryInterface(iid, result);
}


////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP
nsRDFTreeDataModel::GetVisibleColumnCount(int& count) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFTreeDataModel::GetNthColumn(nsITreeColumn*& pColumn, int n) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
	

NS_IMETHODIMP
nsRDFTreeDataModel::GetFirstVisibleItemIndex(int& index) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFTreeDataModel::GetNthTreeItem(nsITreeItem*& pItem, int n) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFTreeDataModel::GetItemTextForColumn(nsString& nodeText,
                                         nsITreeItem* pItem,
                                         nsITreeColumn* pColumn) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
