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

const PRUint32 nsRDFTreeDataModelItem::kInvalidIndentationLevel = PRUint32(-1);

static NS_DEFINE_IID(kITreeDMItemIID, NS_ITREEDMITEM_IID);

////////////////////////////////////////////////////////////////////////

nsRDFTreeDataModelItem::nsRDFTreeDataModelItem(nsRDFTreeModel& tree, RDF_Resource& resource)
    : nsRDFDataModelItem(resource),
      mTree(tree),
      mOpen(PR_FALSE),
      mEnabled(PR_FALSE)
{
}


nsRDFTreeDataModelItem::~nsRDFTreeDataModelItem(void)
{
}


NS_IMETHODIMP
nsRDFTreeDataModelItem::QueryInterface(const nsIID& iid, void** result)
{
    if (NULL == result)
        return NS_ERROR_NULL_POINTER;

    *result = NULL;
    if (iid.Equals(kITreeDMItemIID)) {
        *result = static_cast<nsITreeDMItem*>(this);
        nsRDFDataModelItem::AddRef(); // delegate to the superclass
        return NS_OK;
    }

    // delegate to the superclass.
    return nsRDFDataModelItem::QueryInterface(iid, result);
}


////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP
nsRDFTreeDataModelItem::GetTriggerImage(nsIImage*& pImage, nsIImageGroup* pGroup) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFTreeDataModelItem::GetIndentationLevel(PRUint32& indentation) const
{
    // 1. ask the tree what the parent/child URI is.
    // 2. use it to walk back up to the root, counting as we go
    // 3. multiply the resulting count by the pixel width

    mCachedIndentationLevel = 0; // XXX test "mutable" keyword
    return NS_ERROR_NOT_IMPLEMENTED;
}
