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

static NS_DEFINE_IID(kIToolbarDataModelIID, NS_ITOOLBARDATAMODEL_IID);


////////////////////////////////////////////////////////////////////////

nsRDFToolbarDataModel::nsRDFToolbarDataModel(nsIRDFDataBase& db, RDF_Resource& root)
    : nsRDFDataModel(db), mRoot(root)
{
}



nsRDFToolbarDataModel::~nsRDFToolbarDataModel(void)
{
}


NS_IMETHODIMP
nsRDFToolbarDataModel::QueryInterface(const nsIID& iid, void** result)
{
    if (NULL == result)
        return NS_ERROR_NULL_POINTER;

    *result = NULL;
    if (iid.Equals(kIToolbarDataModelIID)) {
        *result = static_cast<nsIToolbarDataModel*>(this);
        nsRDFDataModel::AddRef(); // delegate to the superclass
        return NS_OK;
    }

    // delegate to the superclass.
    return nsRDFDataModel::QueryInterface(iid, result);
}

