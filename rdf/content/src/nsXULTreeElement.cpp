/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsCOMPtr.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsXULTreeElement.h"

NS_IMPL_ISUPPORTS_INHERITED(nsXULTreeElement, nsXULElement, nsIDOMXULTreeElement);

NS_IMETHODIMP
nsXULTreeElement::GetDatabase(nsIRDFCompositeDataSource** aDatabase)
{
    NS_PRECONDITION(aDatabase != nsnull, "null ptr");
    if (! aDatabase)
        return NS_ERROR_NULL_POINTER;

    *aDatabase = mDatabase;
    NS_IF_ADDREF(*aDatabase);
    return NS_OK;
}


NS_IMETHODIMP
nsXULTreeElement::SetDatabase(nsIRDFCompositeDataSource* aDatabase)
{
    // XXX maybe someday you'll be allowed to change it.
    NS_PRECONDITION(mDatabase == nsnull, "already initialized");
    if (mDatabase)
        return NS_ERROR_ALREADY_INITIALIZED;

    mDatabase = aDatabase;

    // XXX reconstruct the entire tree now!

    return NS_OK;
}
