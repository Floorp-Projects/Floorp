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

static NS_DEFINE_IID(kIRDFAssertionCursorIID, NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,          NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,         NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFResourceManagerIID, NS_IRDFRESOURCEMANAGER_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kRDFResourceManagerCID,  NS_RDFRESOURCEMANAGER_CID);

////////////////////////////////////////////////////////////////////////

#define RDF_NAMESPACE_URI  "http://www.w3.org/TR/WD-rdf-syntax#"
static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, nextVal); // ad hoc way to make containers fast


////////////////////////////////////////////////////////////////////////

class ContainerCursorImpl : public nsIRDFAssertionCursor {
private:
    nsIRDFResourceManager* mResourceMgr;
    nsIRDFDataSource* mDataSource;
    nsIRDFResource* mContainer;
    nsIRDFNode* mNext;
    PRInt32 mCounter;

public:
    ContainerCursorImpl(nsIRDFDataSource* ds, nsIRDFResource* container);
    virtual ~ContainerCursorImpl(void);

    NS_DECL_ISUPPORTS

    NS_IMETHOD Advance(void);

    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetSubject(nsIRDFResource** aResource);
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate);
    NS_IMETHOD GetObject(nsIRDFNode** aObject);
    NS_IMETHOD GetTruthValue(PRBool* aTruthValue);
};

ContainerCursorImpl::ContainerCursorImpl(nsIRDFDataSource* ds,
                                         nsIRDFResource* container)
    : mDataSource(ds), mContainer(container), mNext(nsnull), mCounter(1)
{
    NS_INIT_REFCNT();
    NS_IF_ADDREF(mDataSource);
    NS_IF_ADDREF(mContainer);

    nsresult rv;
    rv = nsServiceManager::GetService(kRDFResourceManagerCID,
                                      kIRDFResourceManagerIID,
                                      (nsISupports**) &mResourceMgr);

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to acquire resource manager");

    NS_ASSERTION(rdf_IsContainer(mResourceMgr, mDataSource, container), "not a container");
}


ContainerCursorImpl::~ContainerCursorImpl(void)
{
    NS_IF_RELEASE(mNext);

    if (mResourceMgr)
        nsServiceManager::ReleaseService(kRDFResourceManagerCID, mResourceMgr);

    NS_IF_RELEASE(mContainer);
    NS_IF_RELEASE(mDataSource);
}

NS_IMPL_ADDREF(ContainerCursorImpl);
NS_IMPL_RELEASE(ContainerCursorImpl);

NS_IMETHODIMP_(nsresult)
ContainerCursorImpl::QueryInterface(REFNSIID iid, void** result) {
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIRDFAssertionCursorIID) ||
        iid.Equals(kIRDFCursorIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFAssertionCursor*, this);
        /* AddRef(); // not necessary */
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


NS_IMETHODIMP
ContainerCursorImpl::Advance(void)
{
    nsresult rv;

    // release the last value that we were holding
    NS_IF_RELEASE(mNext);

    nsIRDFResource* RDF_nextVal = nsnull;
    nsIRDFNode* nextNode        = nsnull;
    nsIRDFLiteral* nextVal      = nsnull;
    const PRUnichar* p;
    nsAutoString s;
    PRInt32 last;
    PRInt32 err;

    // XXX we could cache all this crap when the cursor gets created.
    if (NS_FAILED(rv = mResourceMgr->GetResource(kURIRDF_nextVal, &RDF_nextVal)))
        goto done;

    if (NS_FAILED(rv = mDataSource->GetTarget(mContainer, RDF_nextVal, PR_TRUE, &nextNode)))
        goto done;

    if (NS_FAILED(rv = nextNode->QueryInterface(kIRDFLiteralIID, (void**) &nextVal)))
        goto done;

    if (NS_FAILED(rv = nextVal->GetValue(&p)))
        goto done;

    s = p;
    last = s.ToInteger(&err);
    if (NS_FAILED(err))
        goto done;

    // initialize rv to the case where mCounter has advanced past the
    // last element
    rv = NS_ERROR_UNEXPECTED;

    while (mCounter < last) {
        nsIRDFResource* ordinalProperty = nsnull;
        if (NS_FAILED(rv = GetPredicate(&ordinalProperty)))
            break;

        rv = mDataSource->GetTarget(mContainer, ordinalProperty, PR_TRUE, &mNext);
        NS_IF_RELEASE(ordinalProperty);

        ++mCounter;

        if (NS_SUCCEEDED(rv)) {
            // Don't bother releasing mNext; we'll let the AddRef
            // serve as the implicit addref that GetNext() should
            // perform.
            break;
        }
    }

done:
    NS_IF_RELEASE(nextNode);
    NS_IF_RELEASE(nextVal);
    NS_IF_RELEASE(RDF_nextVal);
    return rv;
}



NS_IMETHODIMP
ContainerCursorImpl::GetDataSource(nsIRDFDataSource** aDataSource)
{
    NS_PRECONDITION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(mDataSource);
    *aDataSource = mDataSource;
    return NS_OK;
}


NS_IMETHODIMP
ContainerCursorImpl::GetSubject(nsIRDFResource** aSubject)
{
    NS_PRECONDITION(aSubject != nsnull, "null ptr");
    if (! aSubject)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(mContainer);
    *aSubject = mContainer;
    return NS_OK;
}


NS_IMETHODIMP
ContainerCursorImpl::GetPredicate(nsIRDFResource** aPredicate)
{
    NS_PRECONDITION(aPredicate != nsnull, "null ptr");

    nsAutoString s(kRDFNameSpaceURI);
    s.Append("_");
    s.Append(mCounter, 10);

    // this'll AddRef(), null check, etc.
    return mResourceMgr->GetUnicodeResource(s, aPredicate);
}


NS_IMETHODIMP
ContainerCursorImpl::GetObject(nsIRDFNode** aObject)
{
    NS_PRECONDITION(aObject != nsnull, "null ptr");
    if (! aObject)
        return NS_ERROR_NULL_POINTER;

    if (! mNext)
        return NS_ERROR_UNEXPECTED;

    NS_ADDREF(mNext);
    *aObject = mNext;
    return NS_OK;
}


NS_IMETHODIMP
ContainerCursorImpl::GetTruthValue(PRBool* aTruthValue)
{
    NS_PRECONDITION(aTruthValue != nsnull, "null ptr");
    if (! aTruthValue)
        return NS_ERROR_NULL_POINTER;

    *aTruthValue = PR_TRUE;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////

nsresult
NS_NewContainerCursor(nsIRDFDataSource* ds,
                      nsIRDFResource* container,
                      nsIRDFAssertionCursor** cursor)
{
    NS_PRECONDITION(ds != nsnull,        "null ptr");
    NS_PRECONDITION(container != nsnull, "null ptr");
    NS_PRECONDITION(cursor != nsnull,    "null ptr");

    if (!ds || !container || !cursor)
        return NS_ERROR_NULL_POINTER;

    ContainerCursorImpl* result = new ContainerCursorImpl(ds, container);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    *cursor = result;
    NS_ADDREF(result);
    return NS_OK;
}
