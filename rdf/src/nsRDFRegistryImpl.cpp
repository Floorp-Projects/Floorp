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

#include "nsIRDFDataSource.h"
#include "nsRDFRegistryImpl.h"
#include "nsString.h"
#include "prlog.h"

static NS_DEFINE_IID(kIRDFRegistryIID, NS_IRDFREGISTRY_IID);

////////////////////////////////////////////////////////////////////////

struct RegistryEntry {
    nsAutoString      mPrefix;
    nsIRDFDataSource* mDataSource;
};

////////////////////////////////////////////////////////////////////////

nsRDFRegistryImpl::nsRDFRegistryImpl(void)
{
    NS_INIT_REFCNT();
}


nsRDFRegistryImpl::~nsRDFRegistryImpl(void)
{
    PR_ASSERT(mDataSources.Count() == 0);
}


NS_IMPL_ISUPPORTS(nsRDFRegistryImpl, kIRDFRegistryIID);

NS_IMETHODIMP
nsRDFRegistryImpl::Register(const nsString& prefix,
                            nsIRDFDataSource* dataSource)
{
    if (! dataSource)
        return NS_ERROR_NULL_POINTER;

    // Make sure that we don't have duplicates, and that we don't try
    // to replace an existing entry...
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        RegistryEntry* e = NS_STATIC_CAST(RegistryEntry*, mDataSources[i]);
        if (e->mPrefix == prefix)
            return NS_ERROR_FAILURE;

        // XXX not sure if this is right...
        if (e->mDataSource == dataSource)
            return NS_ERROR_FAILURE;
    }

    RegistryEntry* e = new RegistryEntry;
    if (! e)
        return NS_ERROR_OUT_OF_MEMORY;

    e->mPrefix     = prefix;
    e->mDataSource = dataSource; // n.b., no AddRef()!

    mDataSources.AppendElement(e);
    return NS_OK;
}


NS_IMETHODIMP
nsRDFRegistryImpl::Unregister(const nsIRDFDataSource* dataSource)
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        RegistryEntry* e = NS_STATIC_CAST(RegistryEntry*, mDataSources[i]);
        if (e->mDataSource == dataSource) {
            mDataSources.RemoveElementAt(i);
            delete e;
            return NS_OK;
        }
    }

    return NS_ERROR_FAILURE;    
}


NS_IMETHODIMP
nsRDFRegistryImpl::Find(const nsString& uri, nsIRDFDataSource*& result)
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        RegistryEntry* e = NS_STATIC_CAST(RegistryEntry*, mDataSources[i]);
        if (uri.Find(e->mPrefix) == 0) {
            result = e->mDataSource;
            NS_ADDREF(result);
            return NS_OK;
        }
    }

    return NS_ERROR_FAILURE;
}
