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

#include "nscore.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFResourceManager.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "prlog.h"
#include "rdf.h"
#include "rdfutil.h"

/*

  A simple cursor that enumerates the elements of a container.

  Caveats
  -------

  1. This uses an implementation-specific detail to determine the
     index of the last element in the container; specifically, the RDF
     utilities maintain a counter attribute on the container that
     holds the numeric value of the next value that is to be
     assigned. So, this cursor will bust if you use it with a bag that
     hasn't been created using the RDF utility routines.

  2. This is sort of a continuation of (1), but -- it's not smart
     enough to notice duplicates.

  TODO. This is way too brain dead to handle aggregated RDF
  databases. It needs to be upgraded in a big way.

 */

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIRDFResourceManagerIID, NS_IRDFRESOURCEMANAGER_IID);
static NS_DEFINE_CID(kRDFResourceManagerCID,  NS_RDFRESOURCEMANAGER_CID);

////////////////////////////////////////////////////////////////////////

#define RDF_NAMESPACE_URI  "http://www.w3.org/TR/WD-rdf-syntax#"
static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, nextVal); // ad hoc way to make containers fast


////////////////////////////////////////////////////////////////////////

class ContainerCursorImpl : public nsIRDFCursor {
private:
    nsIRDFResourceManager* mResourceMgr;
    nsIRDFDataSource* mDataSource;
    nsIRDFNode* mContainer;
    nsIRDFNode* mNext;
    PRInt32 mCounter;

    void SkipToNext(void);

public:
    ContainerCursorImpl(nsIRDFDataSource* ds, nsIRDFNode* container);
    virtual ~ContainerCursorImpl(void);

    NS_DECL_ISUPPORTS

    NS_IMETHOD HasMoreElements(PRBool& result);
    NS_IMETHOD GetNext(nsIRDFNode*& next, PRBool& tv);
};

ContainerCursorImpl::ContainerCursorImpl(nsIRDFDataSource* ds,
                                         nsIRDFNode* container)
    : mDataSource(ds), mContainer(container), mNext(nsnull), mCounter(1)
{
    NS_ASSERTION(ds != nsnull, "null ptr");
    NS_ASSERTION(container != nsnull, "null ptr");

    NS_IF_ADDREF(mDataSource);
    NS_IF_ADDREF(mContainer);

    nsresult rv;
    rv = nsServiceManager::GetService(kRDFResourceManagerCID,
                                      kIRDFResourceManagerIID,
                                      (nsISupports**) &mResourceMgr);

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to acquire resource manager");

    NS_ASSERTION(rdf_IsContainer(mResourceMgr, mDataSource, container), "not a container");
    SkipToNext();
}


ContainerCursorImpl::~ContainerCursorImpl(void)
{
    NS_IF_RELEASE(mNext);

    if (mResourceMgr)
        nsServiceManager::ReleaseService(kRDFResourceManagerCID, mResourceMgr);

    NS_IF_RELEASE(mContainer);
    NS_IF_RELEASE(mDataSource);
}

static NS_DEFINE_IID(kIRDFCursorIID, NS_IRDFCURSOR_IID);
NS_IMPL_ISUPPORTS(ContainerCursorImpl, kIRDFCursorIID);


NS_IMETHODIMP
ContainerCursorImpl::HasMoreElements(PRBool& result)
{
    return (mNext != nsnull);
}


NS_IMETHODIMP
ContainerCursorImpl::GetNext(nsIRDFNode*& next, PRBool& tv)
{
    if (! mNext)
        return NS_ERROR_UNEXPECTED;

    next = mNext; // no need to addref, SkipToNext() did it
    tv = PR_TRUE;

    SkipToNext();
    return NS_OK;
}


void
ContainerCursorImpl::SkipToNext(void)
{
    if (!mDataSource || !mContainer)
        return;

    nsresult rv;
    mNext = nsnull;

    nsIRDFNode* RDF_nextVal = nsnull;
    nsIRDFNode* nextVal     = nsnull;
    nsAutoString next;
    PRInt32 last;
    PRInt32 err;

    // XXX we could cache all this crap when the cursor gets created.
    if (NS_FAILED(rv = mResourceMgr->GetNode(kURIRDF_nextVal, RDF_nextVal)))
        goto done;

    if (NS_FAILED(rv = mDataSource->GetTarget(mContainer, RDF_nextVal, PR_TRUE, nextVal)))
        goto done;

    if (NS_FAILED(rv = nextVal->GetStringValue(next)))
        goto done;

    last = next.ToInteger(&err);
    if (NS_FAILED(err))
        goto done;

    while (mCounter < last) {
        next = kRDFNameSpaceURI;
        next.Append("_");
        next.Append(mCounter, 10);

        nsIRDFNode* ordinalProperty = nsnull;
        if (NS_FAILED(rv = mResourceMgr->GetNode(next, ordinalProperty)))
            break;
            
        rv = mDataSource->GetTarget(mContainer, ordinalProperty, PR_TRUE, mNext);
        NS_IF_RELEASE(ordinalProperty);

        if (NS_SUCCEEDED(rv)) {
            // Don't bother releasing mNext; we'll let the AddRef
            // serve as the implicit addref that GetNext() should
            // perform.
            break;
        }

        ++mCounter;
    }

done:
    NS_IF_RELEASE(nextVal);
    NS_IF_RELEASE(RDF_nextVal);
}


////////////////////////////////////////////////////////////////////////

nsresult
NS_NewContainerCursor(nsIRDFDataSource* ds,
                      nsIRDFNode* container,
                      nsIRDFCursor*& cursor)
{
    nsIRDFCursor* result = new ContainerCursorImpl(ds, container);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    cursor = result;
    NS_ADDREF(cursor);
    return NS_OK;
}
