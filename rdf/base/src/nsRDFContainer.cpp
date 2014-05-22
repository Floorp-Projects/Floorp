/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  Implementation for the RDF container.

  Notes
  -----

  1. RDF containers are one-indexed. This means that a lot of the loops
     that you'd normally think you'd write like this:

       for (i = 0; i < count; ++i) {}

     You've gotta write like this:

       for (i = 1; i <= count; ++i) {}

     "Sure, right, yeah, of course.", you say. Well maybe I'm just
     thick, but it's easy to slip up.

  2. The RDF:nextVal property on the container is an
     implementation-level hack that is used to quickly compute the
     next value for appending to the container. It will no doubt
     become royally screwed up in the case of aggregation.

  3. The RDF:nextVal property is also used to retrieve the count of
     elements in the container.

 */


#include "nsCOMPtr.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFInMemoryDataSource.h"
#include "nsIRDFPropagatableDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "rdf.h"

#define RDF_SEQ_LIST_LIMIT   8

class RDFContainerImpl : public nsIRDFContainer
{
public:

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFContainer interface
    NS_DECL_NSIRDFCONTAINER

private:
    friend nsresult NS_NewRDFContainer(nsIRDFContainer** aResult);

    RDFContainerImpl();
    virtual ~RDFContainerImpl();

    nsresult Init();

    nsresult Renumber(int32_t aStartIndex, int32_t aIncrement);
    nsresult SetNextValue(int32_t aIndex);
    nsresult GetNextValue(nsIRDFResource** aResult);
    
    nsIRDFDataSource* mDataSource;
    nsIRDFResource*   mContainer;

    // pseudo constants
    static int32_t gRefCnt;
    static nsIRDFService*        gRDFService;
    static nsIRDFContainerUtils* gRDFContainerUtils;
    static nsIRDFResource*       kRDF_nextVal;
};


int32_t               RDFContainerImpl::gRefCnt = 0;
nsIRDFService*        RDFContainerImpl::gRDFService;
nsIRDFContainerUtils* RDFContainerImpl::gRDFContainerUtils;
nsIRDFResource*       RDFContainerImpl::kRDF_nextVal;

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ISUPPORTS(RDFContainerImpl, nsIRDFContainer)



////////////////////////////////////////////////////////////////////////
// nsIRDFContainer interface

NS_IMETHODIMP
RDFContainerImpl::GetDataSource(nsIRDFDataSource** _retval)
{
    *_retval = mDataSource;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerImpl::GetResource(nsIRDFResource** _retval)
{
    *_retval = mContainer;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerImpl::Init(nsIRDFDataSource *aDataSource, nsIRDFResource *aContainer)
{
    NS_PRECONDITION(aDataSource != nullptr, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aContainer != nullptr, "null ptr");
    if (! aContainer)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    bool isContainer;
    rv = gRDFContainerUtils->IsContainer(aDataSource, aContainer, &isContainer);
    if (NS_FAILED(rv)) return rv;

    // ``throw'' if we can't create a container on the specified
    // datasource/resource combination.
    if (! isContainer)
        return NS_ERROR_FAILURE;

    NS_IF_RELEASE(mDataSource);
    mDataSource = aDataSource;
    NS_ADDREF(mDataSource);

    NS_IF_RELEASE(mContainer);
    mContainer = aContainer;
    NS_ADDREF(mContainer);

    return NS_OK;
}


NS_IMETHODIMP
RDFContainerImpl::GetCount(int32_t *aCount)
{
    if (!mDataSource || !mContainer)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    // Get the next value, which hangs off of the bag via the
    // RDF:nextVal property. This is the _next value_ that will get
    // assigned in a one-indexed array. So, it's actually _one more_
    // than the actual count of elements in the container.
    //
    // XXX To handle aggregation, this should probably be a
    // GetTargets() that enumerates all of the values and picks the
    // largest one.
    nsCOMPtr<nsIRDFNode> nextValNode;
    rv = mDataSource->GetTarget(mContainer, kRDF_nextVal, true, getter_AddRefs(nextValNode));
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_RDF_NO_VALUE)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIRDFLiteral> nextValLiteral;
    rv = nextValNode->QueryInterface(NS_GET_IID(nsIRDFLiteral), getter_AddRefs(nextValLiteral));
    if (NS_FAILED(rv)) return rv;

    const char16_t *s;
    rv = nextValLiteral->GetValueConst( &s );
    if (NS_FAILED(rv)) return rv;

    nsAutoString nextValStr(s);

    int32_t nextVal;
    nsresult err;
    nextVal = nextValStr.ToInteger(&err);
    if (NS_FAILED(err))
        return NS_ERROR_UNEXPECTED;

    *aCount = nextVal - 1;
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerImpl::GetElements(nsISimpleEnumerator **_retval)
{
    if (!mDataSource || !mContainer)
        return NS_ERROR_NOT_INITIALIZED;

    return NS_NewContainerEnumerator(mDataSource, mContainer, _retval);
}


NS_IMETHODIMP
RDFContainerImpl::AppendElement(nsIRDFNode *aElement)
{
    if (!mDataSource || !mContainer)
        return NS_ERROR_NOT_INITIALIZED;

    NS_PRECONDITION(aElement != nullptr, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsIRDFResource> nextVal;
    rv = GetNextValue(getter_AddRefs(nextVal));
    if (NS_FAILED(rv)) return rv;

    rv = mDataSource->Assert(mContainer, nextVal, aElement, true);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
RDFContainerImpl::RemoveElement(nsIRDFNode *aElement, bool aRenumber)
{
    if (!mDataSource || !mContainer)
        return NS_ERROR_NOT_INITIALIZED;

    NS_PRECONDITION(aElement != nullptr, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    int32_t idx;
    rv = IndexOf(aElement, &idx);
    if (NS_FAILED(rv)) return rv;

    if (idx < 0)
        return NS_OK;

    // Remove the element.
    nsCOMPtr<nsIRDFResource> ordinal;
    rv = gRDFContainerUtils->IndexToOrdinalResource(idx,
                                                    getter_AddRefs(ordinal));
    if (NS_FAILED(rv)) return rv;

    rv = mDataSource->Unassert(mContainer, ordinal, aElement);
    if (NS_FAILED(rv)) return rv;

    if (aRenumber) {
        // Now slide the rest of the collection backwards to fill in
        // the gap. This will have the side effect of completely
        // renumber the container from index to the end.
        rv = Renumber(idx + 1, -1);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


NS_IMETHODIMP
RDFContainerImpl::InsertElementAt(nsIRDFNode *aElement, int32_t aIndex, bool aRenumber)
{
    if (!mDataSource || !mContainer)
        return NS_ERROR_NOT_INITIALIZED;

    NS_PRECONDITION(aElement != nullptr, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aIndex >= 1, "illegal value");
    if (aIndex < 1)
        return NS_ERROR_ILLEGAL_VALUE;

    nsresult rv;

    int32_t count;
    rv = GetCount(&count);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(aIndex <= count + 1, "illegal value");
    if (aIndex > count + 1)
        return NS_ERROR_ILLEGAL_VALUE;

    if (aRenumber) {
        // Make a hole for the element. This will have the side effect of
        // completely renumbering the container from 'aIndex' to 'count',
        // and will spew assertions.
        rv = Renumber(aIndex, +1);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIRDFResource> ordinal;
    rv = gRDFContainerUtils->IndexToOrdinalResource(aIndex, getter_AddRefs(ordinal));
    if (NS_FAILED(rv)) return rv;

    rv = mDataSource->Assert(mContainer, ordinal, aElement, true);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
RDFContainerImpl::RemoveElementAt(int32_t aIndex, bool aRenumber, nsIRDFNode** _retval)
{
    if (!mDataSource || !mContainer)
        return NS_ERROR_NOT_INITIALIZED;

    *_retval = nullptr;

    if (aIndex< 1)
        return NS_ERROR_ILLEGAL_VALUE;

    nsresult rv;

    int32_t count;
    rv = GetCount(&count);
    if (NS_FAILED(rv)) return rv;

    if (aIndex > count)
        return NS_ERROR_ILLEGAL_VALUE;

    nsCOMPtr<nsIRDFResource> ordinal;
    rv = gRDFContainerUtils->IndexToOrdinalResource(aIndex, getter_AddRefs(ordinal));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFNode> old;
    rv = mDataSource->GetTarget(mContainer, ordinal, true, getter_AddRefs(old));
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_OK) {
        rv = mDataSource->Unassert(mContainer, ordinal, old);
        if (NS_FAILED(rv)) return rv;

        if (aRenumber) {
            // Now slide the rest of the collection backwards to fill in
            // the gap. This will have the side effect of completely
            // renumber the container from index to the end.
            rv = Renumber(aIndex + 1, -1);
            if (NS_FAILED(rv)) return rv;
        }
    }

    old.swap(*_retval);

    return NS_OK;
}

NS_IMETHODIMP
RDFContainerImpl::IndexOf(nsIRDFNode *aElement, int32_t *aIndex)
{
    if (!mDataSource || !mContainer)
        return NS_ERROR_NOT_INITIALIZED;

    return gRDFContainerUtils->IndexOf(mDataSource, mContainer,
                                       aElement, aIndex);
}


////////////////////////////////////////////////////////////////////////


RDFContainerImpl::RDFContainerImpl()
    : mDataSource(nullptr), mContainer(nullptr)
{
}


nsresult
RDFContainerImpl::Init()
{
    if (gRefCnt++ == 0) {
        nsresult rv;

        NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
        rv = CallGetService(kRDFServiceCID, &gRDFService);
        if (NS_FAILED(rv)) {
            NS_ERROR("unable to get RDF service");
            return rv;
        }

        rv = gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "nextVal"),
                                      &kRDF_nextVal);
        if (NS_FAILED(rv)) return rv;

        NS_DEFINE_CID(kRDFContainerUtilsCID, NS_RDFCONTAINERUTILS_CID);
        rv = CallGetService(kRDFContainerUtilsCID, &gRDFContainerUtils);
        if (NS_FAILED(rv)) {
            NS_ERROR("unable to get RDF container utils service");
            return rv;
        }
    }

    return NS_OK;
}


RDFContainerImpl::~RDFContainerImpl()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: RDFContainerImpl\n", gInstanceCount);
#endif

    NS_IF_RELEASE(mContainer);
    NS_IF_RELEASE(mDataSource);

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gRDFContainerUtils);
        NS_IF_RELEASE(gRDFService);
        NS_IF_RELEASE(kRDF_nextVal);
    }
}


nsresult
NS_NewRDFContainer(nsIRDFContainer** aResult)
{
    RDFContainerImpl* result = new RDFContainerImpl();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = result->Init();
    if (NS_FAILED(rv)) {
        delete result;
        return rv;
    }

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
RDFContainerImpl::Renumber(int32_t aStartIndex, int32_t aIncrement)
{
    if (!mDataSource || !mContainer)
        return NS_ERROR_NOT_INITIALIZED;

    // Renumber the elements in the container starting with
    // aStartIndex, updating each element's index by aIncrement. For
    // example,
    //
    //   (1:a 2:b 3:c)
    //   Renumber(2, +1);
    //   (1:a 3:b 4:c)
    //   Renumber(3, -1);
    //   (1:a 2:b 3:c)
    //
    nsresult rv;

    if (! aIncrement)
        return NS_OK;

    int32_t count;
    rv = GetCount(&count);
    if (NS_FAILED(rv)) return rv;

    if (aIncrement > 0) {
        // Update the container's nextVal to reflect the
        // renumbering. We do this now if aIncrement > 0 because we'll
        // want to be able to acknowledge that new elements are in the
        // container.
        rv = SetNextValue(count + aIncrement + 1);
        if (NS_FAILED(rv)) return rv;
    }

    int32_t i;
    if (aIncrement < 0) {
        i = aStartIndex;
    }
    else {
        i = count; // we're one-indexed.
    }

    // Note: once we disable notifications, don't exit this method until
    // enabling notifications
    nsCOMPtr<nsIRDFPropagatableDataSource> propagatable =
        do_QueryInterface(mDataSource);
    if (propagatable) {
        propagatable->SetPropagateChanges(false);
    }

    bool    err = false;
    while (!err && ((aIncrement < 0) ? (i <= count) : (i >= aStartIndex)))
    {
        nsCOMPtr<nsIRDFResource> oldOrdinal;
        rv = gRDFContainerUtils->IndexToOrdinalResource(i, getter_AddRefs(oldOrdinal));
        if (NS_FAILED(rv))
        {
            err = true;
            continue;
        }

        nsCOMPtr<nsIRDFResource> newOrdinal;
        rv = gRDFContainerUtils->IndexToOrdinalResource(i + aIncrement, getter_AddRefs(newOrdinal));
        if (NS_FAILED(rv))
        {
            err = true;
            continue;
        }

        // Because of aggregation, we need to be paranoid about the
        // possibility that >1 element may be present per ordinal. If
        // there _is_ in fact more than one element, they'll all get
        // assigned to the same new ordinal; i.e., we don't make any
        // attempt to "clean up" the duplicate numbering. (Doing so
        // would require two passes.)
        nsCOMPtr<nsISimpleEnumerator> targets;
        rv = mDataSource->GetTargets(mContainer, oldOrdinal, true, getter_AddRefs(targets));
        if (NS_FAILED(rv))
        {
            err = true;
            continue;
        }

        while (1) {
            bool hasMore;
            rv = targets->HasMoreElements(&hasMore);
            if (NS_FAILED(rv))
            {
                err = true;
                break;
            }

            if (! hasMore)
                break;

            nsCOMPtr<nsISupports> isupports;
            rv = targets->GetNext(getter_AddRefs(isupports));
            if (NS_FAILED(rv))
            {
                err = true;
                break;
            }

            nsCOMPtr<nsIRDFNode> element( do_QueryInterface(isupports) );
            NS_ASSERTION(element != nullptr, "something funky in the enumerator");
            if (! element)
            {
                err = true;
                rv = NS_ERROR_UNEXPECTED;
                break;
            }

            rv = mDataSource->Unassert(mContainer, oldOrdinal, element);
            if (NS_FAILED(rv))
            {
                err = true;
                break;
            }

            rv = mDataSource->Assert(mContainer, newOrdinal, element, true);
            if (NS_FAILED(rv))
            {
                err = true;
                break;
            }
        }

        i -= aIncrement;
    }

    if (!err && (aIncrement < 0))
    {
        // Update the container's nextVal to reflect the
        // renumbering. We do this now if aIncrement < 0 because, up
        // until this point, we'll want people to be able to find
        // things that are still "at the end".
        rv = SetNextValue(count + aIncrement + 1);
        if (NS_FAILED(rv))
        {
            err = true;
        }
    }

    // Note: MUST enable notifications before exiting this method
    if (propagatable) {
        propagatable->SetPropagateChanges(true);
    }

    if (err) return(rv);

    return NS_OK;
}



nsresult
RDFContainerImpl::SetNextValue(int32_t aIndex)
{
    if (!mDataSource || !mContainer)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    // Remove the current value of nextVal, if there is one.
    nsCOMPtr<nsIRDFNode> nextValNode;
    if (NS_SUCCEEDED(rv = mDataSource->GetTarget(mContainer,
                                                 kRDF_nextVal,
                                                 true,
                                                 getter_AddRefs(nextValNode)))) {
        if (NS_FAILED(rv = mDataSource->Unassert(mContainer, kRDF_nextVal, nextValNode))) {
            NS_ERROR("unable to update nextVal");
            return rv;
        }
    }

    nsAutoString s;
    s.AppendInt(aIndex, 10);

    nsCOMPtr<nsIRDFLiteral> nextVal;
    if (NS_FAILED(rv = gRDFService->GetLiteral(s.get(), getter_AddRefs(nextVal)))) {
        NS_ERROR("unable to get nextVal literal");
        return rv;
    }

    rv = mDataSource->Assert(mContainer, kRDF_nextVal, nextVal, true);
    if (rv != NS_RDF_ASSERTION_ACCEPTED) {
        NS_ERROR("unable to update nextVal");
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


nsresult
RDFContainerImpl::GetNextValue(nsIRDFResource** aResult)
{
    if (!mDataSource || !mContainer)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    // Get the next value, which hangs off of the bag via the
    // RDF:nextVal property.
    nsCOMPtr<nsIRDFNode> nextValNode;
    rv = mDataSource->GetTarget(mContainer, kRDF_nextVal, true, getter_AddRefs(nextValNode));
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_RDF_NO_VALUE)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIRDFLiteral> nextValLiteral;
    rv = nextValNode->QueryInterface(NS_GET_IID(nsIRDFLiteral), getter_AddRefs(nextValLiteral));
    if (NS_FAILED(rv)) return rv;

    const char16_t* s;
    rv = nextValLiteral->GetValueConst(&s);
    if (NS_FAILED(rv)) return rv;

    int32_t nextVal = 0;
    {
        for (const char16_t* p = s; *p != 0; ++p) {
            NS_ASSERTION(*p >= '0' && *p <= '9', "not a digit");
            if (*p < '0' || *p > '9')
                break;

            nextVal *= 10;
            nextVal += *p - '0';
        }
    }

    static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;
    char buf[sizeof(kRDFNameSpaceURI) + 16];
    nsFixedCString nextValStr(buf, sizeof(buf), 0);
    nextValStr = kRDFNameSpaceURI;
    nextValStr.Append('_');
    nextValStr.AppendInt(nextVal, 10);

    rv = gRDFService->GetResource(nextValStr, aResult);
    if (NS_FAILED(rv)) return rv;

    // Now increment the RDF:nextVal property.
    rv = mDataSource->Unassert(mContainer, kRDF_nextVal, nextValLiteral);
    if (NS_FAILED(rv)) return rv;

    ++nextVal;
    nextValStr.Truncate();
    nextValStr.AppendInt(nextVal, 10);

    rv = gRDFService->GetLiteral(NS_ConvertASCIItoUTF16(nextValStr).get(), getter_AddRefs(nextValLiteral));
    if (NS_FAILED(rv)) return rv;

    rv = mDataSource->Assert(mContainer, kRDF_nextVal, nextValLiteral, true);
    if (NS_FAILED(rv)) return rv;

    if (RDF_SEQ_LIST_LIMIT == nextVal)
    {
        // focal point for RDF container mutation;
        // basically, provide a hint to allow for fast access
        nsCOMPtr<nsIRDFInMemoryDataSource> inMem = do_QueryInterface(mDataSource);
        if (inMem)
        {
            // ignore error; failure just means slower access
            (void)inMem->EnsureFastContainment(mContainer);
        }
    }

    return NS_OK;
}
