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

#include "nsRDFTreeColumn.h"
#include "nsRDFTreeDataModel.h"
#include "nsIDMWidget.h"
#include "nsIDataModel.h"

static NS_DEFINE_IID(kIDataModelIID, NS_IDATAMODEL_IID);
static NS_DEFINE_IID(kITreeColumnIID, NS_ITREECOLUMN_IID);

const PRUint32 nsRDFTreeColumn::kDefaultWidth = 64; // XXX

////////////////////////////////////////////////////////////////////////

nsRDFTreeColumn::nsRDFTreeColumn(nsRDFTreeDataModel& tree,
                                 const nsString& name,
                                 RDF_Resource property,
                                 PRUint32 width)
    : mTree(tree),
      mName(name),
      mProperty(property),
      mWidth(width),
      mSortState(eColumnSortState_Unsorted),
      mDesiredPercentage(0.0)
{
    NS_INIT_REFCNT();
    mTree.AddRef();
}


nsRDFTreeColumn::~nsRDFTreeColumn(void)
{
}


NS_IMPL_ADDREF(nsRDFTreeColumn);
NS_IMPL_RELEASE(nsRDFTreeColumn);
NS_IMPL_QUERY_INTERFACE(nsRDFTreeColumn, kITreeColumnIID);


////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsRDFTreeColumn::GetPixelWidth(PRUint32& width) const
{
    width = mWidth;
    return NS_OK;
}



NS_IMETHODIMP
nsRDFTreeColumn::GetDesiredPercentage(double& percentage) const
{
    percentage = mDesiredPercentage;
    return NS_OK;
}


NS_IMETHODIMP
nsRDFTreeColumn::GetSortState(nsColumnSortState& answer) const
{
    answer = mSortState;
    return NS_OK;
}


NS_IMETHODIMP
nsRDFTreeColumn::GetColumnName(nsString& name) const
{
    name = mName;
    return NS_OK;
}


NS_IMETHODIMP
nsRDFTreeColumn::SetPixelWidth(PRUint32 newWidth)
{
    mWidth = newWidth;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////

void
nsRDFTreeColumn::SetVisibility(PRBool visible)
{
    if (visible != mVisible) {
        mVisible = visible;

        nsIDataModel* model;
        if (NS_SUCCEEDED(mTree.QueryInterface(kIDataModelIID, (void**) &model))) {
            nsIDMWidget* dmWidget;

            // XXX if interface inheirits...
            //if (NS_SUCCEEDED(mTree.GetDMWidget(dmWidget))) {

            if (NS_SUCCEEDED(model->GetDMWidget(dmWidget))) {
#if 0
                // XXX IMO, this should be in its own interface. Need to ask Hyatt...
                nsITreeWidget* treeWidget;
                if (NS_SUCCEEDED(dmWidget->QueryInterface(kITreeWidgetIID, &treeWidget))) {
                    treeWidget->OnColumnVisibilityChanged(static_cast<nsITreeColumn*>(this),
                                                          mVisible);

                    treeWidget->Release();
                }
#endif
                dmWidget->Release();
            }
            model->Release();
        }
    }
}


PRBool
nsRDFTreeColumn::IsVisible(void) const
{
    return mVisible;
}

