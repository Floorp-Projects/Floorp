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
#include "nsRDFToolbarDataModelItem.h"

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
// nsIDataModel implementation -- delgates to superclass

NS_IMETHODIMP
nsRDFToolbarDataModel::InitFromURL(const nsString& url)
{
    return nsRDFDataModel::InitFromURL(url);
}


NS_IMETHODIMP
nsRDFToolbarDataModel::InitFromResource(nsIDMItem* pResource)
{
    return nsRDFDataModel::InitFromResource(pResource);
}


NS_IMETHODIMP
nsRDFToolbarDataModel::GetDMWidget(nsIDMWidget*& widget) const
{
    return nsRDFDataModel::GetDMWidget(widget);
}


NS_IMETHODIMP
nsRDFToolbarDataModel::SetDMWidget(nsIDMWidget* widget)
{
    return nsRDFDataModel::SetDMWidget(widget);
}



NS_IMETHODIMP
nsRDFToolbarDataModel::GetStringPropertyValue(nsString& value, const nsString& property) const
{
    return nsRDFDataModel::GetStringPropertyValue(value, property);
}


NS_IMETHODIMP
nsRDFToolbarDataModel::GetIntPropertyValue(PRInt32& value, const nsString& property) const
{
    return nsRDFDataModel::GetIntPropertyValue(value, property);
}

////////////////////////////////////////////////////////////////////////
// nsIToolbarDataModel interface




////////////////////////////////////////////////////////////////////////
// Implementation methods

NS_METHOD
nsRDFToolbarDataModel::CreateItem(RDF_Resource r, nsRDFDataModelItem*& result)
{
    result = new nsRDFToolbarDataModelItem(*this, r);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    result->AddRef(); // like a good little COM citizen
    return NS_OK;
}
