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

/*

  Implementation for the RDF container.

 */

#include "nsCOMPtr.h"
#include "nsIRDFContainer.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "rdf.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID, NS_RDFCONTAINERUTILS_CID);
static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;

class RDFContainerImpl : public nsIRDFContainer
{
public:

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFContainer interface
    NS_IMETHOD Init(nsIRDFDataSource *aDataSource, nsIRDFResource *aContainer);
    NS_IMETHOD GetCount(PRInt32 *_retval);
    NS_IMETHOD GetElements(nsISimpleEnumerator **_retval);
    NS_IMETHOD AppendElement(nsIRDFNode *aElement);
    NS_IMETHOD RemoveElement(nsIRDFNode *aElement, PRBool aRenumber);
    NS_IMETHOD InsertElementAt(nsIRDFNode *aElement, PRInt32 aIndex, PRBool aRenumber);
    NS_IMETHOD IndexOf(nsIRDFNode *aElement, PRInt32 *_retval);

private:
    friend nsresult NS_NewRDFContainer(nsIRDFContainer** aResult);

    RDFContainerImpl();
    virtual ~RDFContainerImpl();

    nsresult Renumber(PRInt32 aStartIndex);
    nsresult SetNextValue(PRInt32 aIndex);
    nsresult GetNextValue(nsIRDFResource** aResult);
    
    nsIRDFDataSource* mDataSource;
    nsIRDFResource*   mContainer;

    // pseudo constants
    static PRInt32 gRefCnt;
    static nsIRDFService*        gRDFService;
    static nsIRDFContainerUtils* gRDFContainerUtils;
    static nsIRDFResource*       kRDF_nextVal;
};


PRInt32               RDFContainerImpl::gRefCnt = 0;
nsIRDFService*        RDFContainerImpl::gRDFService;
nsIRDFContainerUtils* RDFContainerImpl::gRDFContainerUtils;
nsIRDFResource*       RDFContainerImpl::kRDF_nextVal;

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ISUPPORTS(RDFContainerImpl, nsIRDFContainer::GetIID());



////////////////////////////////////////////////////////////////////////
// nsIRDFContainer interface

NS_IMETHODIMP
RDFContainerImpl::Init(nsIRDFDataSource *aDataSource, nsIRDFResource *aContainer)
{
    NS_PRECONDITION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aContainer != nsnull, "null ptr");
    if (! aContainer)
        return NS_ERROR_NULL_POINTER;

    NS_IF_RELEASE(mDataSource);
    mDataSource = aDataSource;
    NS_ADDREF(mDataSource);

    NS_IF_RELEASE(mContainer);
    mContainer = aContainer;
    NS_ADDREF(mContainer);

    return NS_OK;
}


NS_IMETHODIMP
RDFContainerImpl::GetCount(PRInt32 *aCount)
{
    NS_PRECONDITION(aCount != nsnull, "null ptr");
    if (! aCount)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // Get the next value, which hangs off of the bag via the
    // RDF:nextVal property.
    nsCOMPtr<nsIRDFNode> nextValNode;
    rv = mDataSource->GetTarget(mContainer, kRDF_nextVal, PR_TRUE, getter_AddRefs(nextValNode));
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_RDF_NO_VALUE)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIRDFLiteral> nextValLiteral;
    rv = nextValNode->QueryInterface(nsIRDFLiteral::GetIID(), getter_AddRefs(nextValLiteral));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLString s;
    rv = nextValLiteral->GetValue( getter_Copies(s) );
    if (NS_FAILED(rv)) return rv;

    nsAutoString nextValStr = (const PRUnichar*) s;

    PRInt32 err;
    *aCount = nextValStr.ToInteger(&err);
    if (NS_FAILED(err))
        return NS_ERROR_UNEXPECTED;

    return NS_OK;
}


NS_IMETHODIMP
RDFContainerImpl::GetElements(nsISimpleEnumerator **_retval)
{
    return NS_NewContainerEnumerator(mDataSource, mContainer, _retval);
}


NS_IMETHODIMP
RDFContainerImpl::AppendElement(nsIRDFNode *aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    PRBool isContainer;
    rv = gRDFContainerUtils->IsContainer(mDataSource, mContainer, &isContainer);
    if (NS_FAILED(rv)) return rv;

    NS_PRECONDITION(isContainer, "not a container");
    if (! isContainer)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIRDFResource> nextVal;
    rv = GetNextValue(getter_AddRefs(nextVal));
    if (NS_FAILED(rv)) return rv;

    rv = mDataSource->Assert(mContainer, nextVal, aElement, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
RDFContainerImpl::RemoveElement(nsIRDFNode *aElement, PRBool aRenumber)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    PRInt32 index;
    rv = IndexOf(aElement, &index);
    if (NS_FAILED(rv)) return rv;

    if (index < 0) {
        NS_WARNING("attempt to remove non-existant element");
        return NS_OK;
    }

    // Remove the element.
    nsCOMPtr<nsIRDFResource> ordinal;
    rv = gRDFContainerUtils->IndexToOrdinalResource(index, getter_AddRefs(ordinal));
    if (NS_FAILED(rv)) return rv;

    rv = mDataSource->Unassert(mContainer, ordinal, aElement);
    if (NS_FAILED(rv)) return rv;

    if (aRenumber) {
        // Now slide the rest of the collection backwards to fill in
        // the gap. This will have the side effect of completely
        // renumber the container from index to the end.
        rv = Renumber(index);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


NS_IMETHODIMP
RDFContainerImpl::InsertElementAt(nsIRDFNode *aElement, PRInt32 aIndex, PRBool aRenumber)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aIndex >= 1, "illegal value");
    if (aIndex < 1)
        return NS_ERROR_ILLEGAL_VALUE;

    nsresult rv;

    PRInt32 count;
    rv = GetCount(&count);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(aIndex <= count, "illegal value");
    if (aIndex > count)
        return NS_ERROR_ILLEGAL_VALUE;

    if (aRenumber) {
        // Make a hole for the element. This will have the side effect of
        // completely renumbering the container from 'aIndex' to 'count',
        // and will spew assertions.
        rv = Renumber(aIndex);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIRDFResource> ordinal;
    rv = gRDFContainerUtils->IndexToOrdinalResource(aIndex, getter_AddRefs(ordinal));
    if (NS_FAILED(rv)) return rv;

    rv = mDataSource->Assert(mContainer, ordinal, aElement, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
RDFContainerImpl::IndexOf(nsIRDFNode *aElement, PRInt32 *aIndex)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aIndex != nsnull, "null ptr");
    if (! aIndex)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    PRInt32 count;
    rv = GetCount(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 index = 0; index < count; ++index) {
        nsCOMPtr<nsIRDFResource> ordinal;
        rv = gRDFContainerUtils->IndexToOrdinalResource(index, getter_AddRefs(ordinal));
        if (NS_FAILED(rv)) return rv;

        // Get all of the elements in the container with the specified
        // ordinal. This is an ultra-paranoid way to do it, but -- due
        // to aggregation, we may end up with a container that has >1
        // element for the same ordinal.
        nsCOMPtr<nsISimpleEnumerator> targets;
        rv = mDataSource->GetTargets(mContainer, ordinal, PR_TRUE, getter_AddRefs(targets));
        if (NS_FAILED(rv)) return rv;

        while (1) {
            PRBool hasMore;
            rv = targets->HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return rv;

            if (! hasMore)
                break;

            nsCOMPtr<nsISupports> isupports;
            rv = targets->GetNext(getter_AddRefs(isupports));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to read cursor");
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIRDFNode> element = do_QueryInterface(isupports);

            if (element.get() != aElement)
                continue;

            // Okay, we've found it!
            *aIndex = index;
            return NS_OK;
        }
    }

    NS_WARNING("element not found");
    *aIndex = -1;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////

RDFContainerImpl::RDFContainerImpl()
    : mDataSource(nsnull), mContainer(nsnull)
{
    NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        nsresult rv;

        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          nsIRDFService::GetIID(),
                                          (nsISupports**) &gRDFService);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
        if (NS_SUCCEEDED(rv)) {
            gRDFService->GetResource(RDF_NAMESPACE_URI "nextVal", &kRDF_nextVal);
        }

        rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
                                          nsIRDFContainerUtils::GetIID(),
                                          (nsISupports**) &gRDFContainerUtils);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF container utils service");
    }
}


RDFContainerImpl::~RDFContainerImpl()
{
    NS_IF_RELEASE(mContainer);
    NS_IF_RELEASE(mDataSource);

    if (--gRefCnt == 0) {
        nsServiceManager::ReleaseService(kRDFContainerUtilsCID, gRDFContainerUtils);
        nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
    }
}


nsresult
NS_NewRDFContainer(nsIRDFContainer** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    RDFContainerImpl* result =
        new RDFContainerImpl();

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;
    return NS_OK;
}


nsresult
NS_NewRDFContainer(nsIRDFDataSource* aDataSource,
                   nsIRDFResource* aResource,
                   nsIRDFContainer** aResult)
{
    nsresult rv;
    rv = NS_NewRDFContainer(aResult);
    if (NS_FAILED(rv)) return rv;

    rv = (*aResult)->Init(aDataSource, aResource);
    if (NS_FAILED(rv)) {
        NS_RELEASE(*aResult);
    }
    return rv;
}


nsresult
RDFContainerImpl::Renumber(PRInt32 aStartIndex)
{
    // Renumber the elements in the container
    nsresult rv;

    PRInt32 count;
    rv = GetCount(&count);
    if (NS_FAILED(rv)) return rv;

    PRInt32 oldIndex = aStartIndex;
    PRInt32 newIndex = aStartIndex;
    while (oldIndex < count) {
        nsCOMPtr<nsIRDFResource> oldOrdinal;
        rv = gRDFContainerUtils->IndexToOrdinalResource(oldIndex, getter_AddRefs(oldOrdinal));
        if (NS_FAILED(rv)) return rv;

        // Because of aggregation, we need to be paranoid about
        // the possibility that >1 element may be present per
        // ordinal.
        nsCOMPtr<nsISimpleEnumerator> targets;
        rv = mDataSource->GetTargets(mContainer, oldOrdinal, PR_TRUE, getter_AddRefs(targets));
        if (NS_FAILED(rv)) return rv;

        while (1) {
            PRBool hasMore;
            rv = targets->HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return rv;

            if (! hasMore)
                break;

            nsCOMPtr<nsISupports> isupports;
            rv = targets->GetNext(getter_AddRefs(isupports));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIRDFNode> element( do_QueryInterface(isupports) );
            NS_ASSERTION(element != nsnull, "something funky in the enumerator");
            if (! element)
                return NS_ERROR_UNEXPECTED;

            rv = mDataSource->Unassert(mContainer, oldOrdinal, element);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIRDFResource> newOrdinal;
            rv = gRDFContainerUtils->IndexToOrdinalResource(++newIndex, getter_AddRefs(newOrdinal));
            if (NS_FAILED(rv)) return rv;

            rv = mDataSource->Assert(mContainer, newOrdinal, element, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
        }
    }

    // Update the container's nextVal to reflect the renumbering
    rv = SetNextValue(++newIndex);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}



nsresult
RDFContainerImpl::SetNextValue(PRInt32 aIndex)
{
    nsresult rv;

    // Remove the current value of nextVal, if there is one.
    nsCOMPtr<nsIRDFNode> nextValNode;
    if (NS_SUCCEEDED(rv = mDataSource->GetTarget(mContainer,
                                                 kRDF_nextVal,
                                                 PR_TRUE,
                                                 getter_AddRefs(nextValNode)))) {
        if (NS_FAILED(rv = mDataSource->Unassert(mContainer, kRDF_nextVal, nextValNode))) {
            NS_ERROR("unable to update nextVal");
            return rv;
        }
    }

    nsAutoString s;
    s.Append(aIndex, 10);

    nsCOMPtr<nsIRDFLiteral> nextVal;
    if (NS_FAILED(rv = gRDFService->GetLiteral(s.GetUnicode(), getter_AddRefs(nextVal)))) {
        NS_ERROR("unable to get nextVal literal");
        return rv;
    }

    rv = mDataSource->Assert(mContainer, kRDF_nextVal, nextVal, PR_TRUE);
    if (rv != NS_RDF_ASSERTION_ACCEPTED) {
        NS_ERROR("unable to update nextVal");
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


nsresult
RDFContainerImpl::GetNextValue(nsIRDFResource** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // Get the next value, which hangs off of the bag via the
    // RDF:nextVal property.
    nsCOMPtr<nsIRDFNode> nextValNode;
    rv = mDataSource->GetTarget(mContainer, kRDF_nextVal, PR_TRUE, getter_AddRefs(nextValNode));
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_RDF_NO_VALUE)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIRDFLiteral> nextValLiteral;
    rv = nextValNode->QueryInterface(nsIRDFLiteral::GetIID(), getter_AddRefs(nextValLiteral));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLString s;
    rv = nextValLiteral->GetValue( getter_Copies(s) );
    if (NS_FAILED(rv)) return rv;

    nsAutoString nextValStr = (const PRUnichar*) s;

    PRInt32 err;
    PRInt32 nextVal = nextValStr.ToInteger(&err);
    if (NS_FAILED(err))
        return NS_ERROR_UNEXPECTED;

    // Generate a URI that we can return.
    nextValStr = kRDFNameSpaceURI;
    nextValStr.Append("_");
    nextValStr.Append(nextVal, 10);

    rv = gRDFService->GetUnicodeResource(nextValStr.GetUnicode(), aResult);
    if (NS_FAILED(rv)) return rv;

    // Now increment the RDF:nextVal property.
    rv = mDataSource->Unassert(mContainer, kRDF_nextVal, nextValLiteral);
    if (NS_FAILED(rv)) return rv;

    ++nextVal;
    nextValStr.Truncate();
    nextValStr.Append(nextVal, 10);

    rv = gRDFService->GetLiteral(nextValStr.GetUnicode(), getter_AddRefs(nextValLiteral));
    if (NS_FAILED(rv)) return rv;

    rv = mDataSource->Assert(mContainer, kRDF_nextVal, nextValLiteral, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


