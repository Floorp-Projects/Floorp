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

/*
  This file provides the implementation for the sort service manager.
 */


#include "nsIXULDocumentInfo.h"
#include "nsIDocument.h"
#include "nsIRDFResource.h"
#include "nsRDFCID.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kXULDocumentInfoCID,      NS_XULDOCUMENTINFO_CID);
static NS_DEFINE_IID(kIXULDocumentInfoIID,     NS_IXULDOCUMENTINFO_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);


////////////////////////////////////////////////////////////////////////
// DocumentInfoImpl
//
//   This is the document info object passed to the doc loader
//
class XULDocumentInfoImpl : public nsIXULDocumentInfo
{
public:
    XULDocumentInfoImpl(void);
    virtual ~XULDocumentInfoImpl(void);
    
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIDocumentInfo
    NS_IMETHOD Init(nsIDocument* aDocument, nsIRDFResource* aResource);
    NS_IMETHOD GetDocument(nsIDocument** aDocument);
    NS_IMETHOD GetResource(nsIRDFResource** aResource);

private:
    nsIDocument* mParentDocument;
    nsIRDFResource* mFragmentRoot;
};


////////////////////////////////////////////////////////////////////////

XULDocumentInfoImpl::XULDocumentInfoImpl(void)
:mParentDocument(nsnull), mFragmentRoot(nsnull)
{
	NS_INIT_REFCNT();
}


XULDocumentInfoImpl::~XULDocumentInfoImpl(void)
{
  NS_IF_RELEASE(mParentDocument);
  NS_IF_RELEASE(mFragmentRoot);
}

nsresult
XULDocumentInfoImpl::Init(nsIDocument* aDocument, nsIRDFResource* aResource) {
  NS_IF_RELEASE(aDocument);
  NS_IF_RELEASE(aResource);

  mParentDocument = aDocument;
  mFragmentRoot = aResource;

  NS_IF_ADDREF(mParentDocument);
  NS_IF_ADDREF(mFragmentRoot);

  return NS_OK;
}

nsresult
XULDocumentInfoImpl::GetDocument(nsIDocument** aDocument) {
  NS_IF_ADDREF(mParentDocument);
  *aDocument = mParentDocument;
  return NS_OK;
}

nsresult
XULDocumentInfoImpl::GetResource(nsIRDFResource** aResource) {
  NS_IF_ADDREF(mFragmentRoot);
  *aResource = mFragmentRoot;
  return NS_OK;
}

NS_IMPL_ADDREF(XULDocumentInfoImpl);
NS_IMPL_RELEASE(XULDocumentInfoImpl);

NS_IMPL_QUERY_INTERFACE(XULDocumentInfoImpl, kIXULDocumentInfoIID);

////////////////////////////////////////////////////////////////////////


nsresult
NS_NewXULDocumentInfo(nsIXULDocumentInfo** aResult)
{
  XULDocumentInfoImpl* info = new XULDocumentInfoImpl();
  NS_ADDREF(info);
  *aResult = info;
  return NS_OK;
}

