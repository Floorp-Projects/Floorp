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
#include "nsRDFDataModelItem.h"
#include "nsIRDFDataBase.h"
#include "nsString.h"
#include "plstr.h"
#include "prprf.h"
#include "rdf-int.h"

// XXX these have just GOT to go away. They're copied from ht.c
PRInt32 htCounter = 0;

char* gNavCenterDataSources1[15] = {
    "rdf:localStore",
    "rdf:remoteStore",
    "rdf:bookmarks",
    "rdf:remoteStore",
    "rdf:history",
    /* "rdf:ldap", */
    "rdf:esftp",
    /* "rdf:mail", */
#ifdef	XP_MAC
    "rdf:appletalk",
#endif
    "rdf:lfs",
    "rdf:ht",
    "rdf:columns",
    "rdf:find",
    NULL
};



////////////////////////////////////////////////////////////////////////

nsRDFDataModel::nsRDFDataModel(void)
    : mDB(NULL),
      mRoot(NULL),
      mWidget(NULL),
      mArcProperty(NULL)
{
    PR_ASSERT(gCoreVocab);
    if (gCoreVocab) {
        mArcProperty = gCoreVocab->RDF_parent;
        mArcType     = eRDFArcType_Inbound;
    }

    NS_INIT_REFCNT();
}

nsRDFDataModel::~nsRDFDataModel(void)
{
    if (mRoot)
        mRoot->Release();

    if (mDB)
        RDF_ReleaseDB(mDB);
}

NS_IMPL_ADDREF(nsRDFDataModel);
NS_IMPL_RELEASE(nsRDFDataModel);

static NS_DEFINE_IID(kIDataModelIID, NS_IDATAMODEL_IID);
NS_IMPL_QUERY_INTERFACE(nsRDFDataModel, kIDataModelIID);

////////////////////////////////////////////////////////////////////////
// nsIDataModel interface

NS_IMETHODIMP
nsRDFDataModel::InitFromURL(const nsString& aUrl)
{
    // XXX A ghastly simplification of HT_PaneFromURL()
    char* url = aUrl.ToNewCString();

    const char* dbstr[2];
    dbstr[0] = getBaseURL(url);
    dbstr[1] = NULL;

    mDB = RDF_GetDB(dbstr);
    PL_strfree(const_cast<char*>(dbstr[0]));

    // XXX now we try to find the top-level resource in the
    // database. PR_FALSE indicates that, if it ain't there, don't
    // create it.
    RDF_Resource r = RDF_GetResource(mDB, url, PR_FALSE);
    delete url;

    PR_ASSERT(r);
    if (! r)
        return NS_ERROR_UNEXPECTED;

    nsRDFDataModelItem* item = new nsRDFDataModelItem(*this, r);
    PR_ASSERT(item);
    if (! item)
        return NS_ERROR_OUT_OF_MEMORY;

    if (mRoot)
        mRoot->Release();

    mRoot = item;
    mRoot->AddRef();
    mRoot->SetOpenState(PR_TRUE);

    return NS_OK;
}


NS_IMETHODIMP
nsRDFDataModel::InitFromResource(nsIDMItem* pResource)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFDataModel::GetDMWidget(nsIDMWidget*& widget) const
{
    widget = GetWidget();
    return NS_OK;
}


NS_IMETHODIMP
nsRDFDataModel::SetDMWidget(nsIDMWidget* widget)
{
    SetWidget(widget);
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

////////////////////////////////////////////////////////////////////////

