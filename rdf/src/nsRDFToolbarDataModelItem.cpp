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

////////////////////////////////////////////////////////////////////////

nsRDFToolbarDataModelItem::nsRDFToolbarDataModelItem(nsRDFToolbarDataModel& Toolbar, RDF_Resource& resource)
    : nsRDFDataModelItem(resource),
      mToolbar(Toolbar)
{
}


nsRDFToolbarDataModelItem::~nsRDFToolbarDataModelItem(void)
{
}


NS_IMETHODIMP
nsRDFToolbarDataModelItem::QueryInterface(const nsIID& iid, void** result)
{
    if (NULL == result)
        return NS_ERROR_NULL_POINTER;

    *result = NULL;
    if (iid.Equals(kIToolbarDMItemIID)) {
        *result = static_cast<nsIToolbarDMItem*>(this);
        nsRDFDataModelItem::AddRef(); // delegate to the superclass
        return NS_OK;
    }

    // delegate to the superclass.
    return nsRDFDataModelItem::QueryInterface(iid, result);
}


