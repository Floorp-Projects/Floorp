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

#include "nsRDFDataModel.h"
#include "nsIRDFDataBase.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDataModelIID, NS_IDATAMODEL_IID);

////////////////////////////////////////////////////////////////////////

nsRDFDataModel::nsRDFDataModel(nsIRDFDataBase& db)
    : mDB(db), mWidget(NULL)
{
    db.AddRef();
    NS_INIT_REFCNT();
}

nsRDFDataModel::~nsRDFDataModel(void)
{
    mDB.Release();
}


NS_IMPL_ADDREF(nsRDFDataModel);
NS_IMPL_RELEASE(nsRDFDataModel);

NS_IMETHODIMP
nsRDFDataModel::QueryInterface(const nsIID& iid, void** result)
{
    if (NULL == result)
        return NS_ERROR_NULL_POINTER;

    *result = NULL;
    if (iid.Equals(kISupportsIID) ||
        iid.Equals(kIDataModelIID)) {
        *result = static_cast<nsIDataModel*>(this);
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP
nsRDFDataModel::GetDMWidget(nsIDMWidget*& widget) const
{
    widget = mWidget;
    return NS_OK;
}


NS_IMETHODIMP
nsRDFDataModel::SetDMWidget(nsIDMWidget* widget)
{
    mWidget = widget;
    return NS_OK;
}



NS_IMETHODIMP
nsRDFDataModel::GetStringPropertyValue(nsString& value, const nsString& property) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFDataModel::GetIntPropertyValue(PRInt32& value, const nsString& property) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
