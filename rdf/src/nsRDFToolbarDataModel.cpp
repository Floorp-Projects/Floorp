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

#include "nsRDFToolbarDataModel.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDataModelIID, NS_IDATAMODEL_IID);
static NS_DEFINE_IID(kIToolbarDataModelIID, NS_ITOOLBARDATAMODEL_IID);


////////////////////////////////////////////////////////////////////////

nsRDFToolbarDataModel::nsRDFToolbarDataModel(void)
{
    NS_INIT_REFCNT();
}



nsRDFToolbarDataModel::~nsRDFToolbarDataModel(void)
{
}

NS_IMETHODIMP_(nsrefcnt)
nsRDFToolbarDataModel::AddRef(void)
{
    return nsRDFDataModel::AddRef(); // delegate to the superclass
}

NS_IMETHODIMP_(nsrefcnt)
nsRDFToolbarDataModel::Release(void)
{
    return nsRDFDataModel::Release(); // delegate to the superclass
}

NS_IMETHODIMP
nsRDFToolbarDataModel::QueryInterface(const nsIID& iid, void** result)
{
    if (NULL == result)
        return NS_ERROR_NULL_POINTER;

    *result = NULL;
    if (iid.Equals(kIToolbarDataModelIID)) {
        *result = static_cast<nsIToolbarDataModel*>(this);
        AddRef();
        return NS_OK;
    }
    // delegate to the superclass
    return nsRDFDataModel::QueryInterface(iid, result);
}

////////////////////////////////////////////////////////////////////////

#if 0
NS_IMETHODIMP
nsRDFToolbarDataModel::InitFromURL(const nsString& url)
{
    return NS_OK;
}


NS_IMETHODIMP
nsRDFToolbarDataModel::InitFromResource(nsIDMItem* pResource)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFToolbarDataModel::GetDMWidget(nsIDMWidget*& widget) const
{
    widget = mModel.GetWidget();
    return NS_OK;
}


NS_IMETHODIMP
nsRDFToolbarDataModel::SetDMWidget(nsIDMWidget* widget)
{
    mModel.SetWidget(widget);
    return NS_OK;
}



NS_IMETHODIMP
nsRDFToolbarDataModel::GetStringPropertyValue(nsString& value, const nsString& property) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFToolbarDataModel::GetIntPropertyValue(PRInt32& value, const nsString& property) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
#endif
