/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  Implementation for the RDF container utils.

 */


#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "prprf.h"
#include "rdf.h"
#include "rdfutil.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;

class RDFContainerUtilsImpl : public nsIRDFContainerUtils
{
public:
    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFContainerUtils interface
    NS_DECL_NSIRDFCONTAINERUTILS

private:
    friend nsresult NS_NewRDFContainerUtils(nsIRDFContainerUtils** aResult);

    RDFContainerUtilsImpl();
    virtual ~RDFContainerUtilsImpl();

    nsresult MakeContainer(nsIRDFDataSource* aDataSource,
                           nsIRDFResource* aResource,
                           nsIRDFResource* aType,
                           nsIRDFContainer** aResult);

    bool IsA(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource, nsIRDFResource* aType);

    // pseudo constants
    static int32_t gRefCnt;
    static nsIRDFService* gRDFService;
    static nsIRDFResource* kRDF_instanceOf;
    static nsIRDFResource* kRDF_nextVal;
    static nsIRDFResource* kRDF_Bag;
    static nsIRDFResource* kRDF_Seq;
    static nsIRDFResource* kRDF_Alt;
    static nsIRDFLiteral* kOne;
};


int32_t         RDFContainerUtilsImpl::gRefCnt = 0;
nsIRDFService*  RDFContainerUtilsImpl::gRDFService;
nsIRDFResource* RDFContainerUtilsImpl::kRDF_instanceOf;
nsIRDFResource* RDFContainerUtilsImpl::kRDF_nextVal;
nsIRDFResource* RDFContainerUtilsImpl::kRDF_Bag;
nsIRDFResource* RDFContainerUtilsImpl::kRDF_Seq;
nsIRDFResource* RDFContainerUtilsImpl::kRDF_Alt;
nsIRDFLiteral*  RDFContainerUtilsImpl::kOne;

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_THREADSAFE_ISUPPORTS1(RDFContainerUtilsImpl, nsIRDFContainerUtils)

////////////////////////////////////////////////////////////////////////
// nsIRDFContainerUtils interface

NS_IMETHODIMP
RDFContainerUtilsImpl::IsOrdinalProperty(nsIRDFResource *aProperty, bool *_retval)
{
    NS_PRECONDITION(aProperty != nullptr, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    const char	*propertyStr;
    rv = aProperty->GetValueConst( &propertyStr );
    if (NS_FAILED(rv)) return rv;

    if (PL_strncmp(propertyStr, kRDFNameSpaceURI, sizeof(kRDFNameSpaceURI) - 1) != 0) {
        *_retval = false;
        return NS_OK;
    }

    const char* s = propertyStr;
    s += sizeof(kRDFNameSpaceURI) - 1;
    if (*s != '_') {
        *_retval = false;
        return NS_OK;
    }

    ++s;
    while (*s) {
        if (*s < '0' || *s > '9') {
            *_retval = false;
            return NS_OK;
        }

        ++s;
    }

    *_retval = true;
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerUtilsImpl::IndexToOrdinalResource(int32_t aIndex, nsIRDFResource **aOrdinal)
{
    NS_PRECONDITION(aIndex > 0, "illegal value");
    if (aIndex <= 0)
        return NS_ERROR_ILLEGAL_VALUE;

    nsAutoCString uri(kRDFNameSpaceURI);
    uri.Append('_');
    uri.AppendInt(aIndex);
    
    nsresult rv = gRDFService->GetResource(uri, aOrdinal);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get ordinal resource");
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerUtilsImpl::OrdinalResourceToIndex(nsIRDFResource *aOrdinal, int32_t *aIndex)
{
    NS_PRECONDITION(aOrdinal != nullptr, "null ptr");
    if (! aOrdinal)
        return NS_ERROR_NULL_POINTER;

    const char	*ordinalStr;
    if (NS_FAILED(aOrdinal->GetValueConst( &ordinalStr )))
        return NS_ERROR_FAILURE;

    const char* s = ordinalStr;
    if (PL_strncmp(s, kRDFNameSpaceURI, sizeof(kRDFNameSpaceURI) - 1) != 0) {
        NS_ERROR("not an ordinal");
        return NS_ERROR_UNEXPECTED;
    }

    s += sizeof(kRDFNameSpaceURI) - 1;
    if (*s != '_') {
        NS_ERROR("not an ordinal");
        return NS_ERROR_UNEXPECTED;
    }

    int32_t idx = 0;

    ++s;
    while (*s) {
        if (*s < '0' || *s > '9') {
            NS_ERROR("not an ordinal");
            return NS_ERROR_UNEXPECTED;
        }

        idx *= 10;
        idx += (*s - '0');

        ++s;
    }

    *aIndex = idx;
    return NS_OK;
}

NS_IMETHODIMP
RDFContainerUtilsImpl::IsContainer(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, bool *_retval)
{
    NS_PRECONDITION(aDataSource != nullptr, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResource != nullptr, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(_retval != nullptr, "null ptr");
    if (! _retval)
        return NS_ERROR_NULL_POINTER;

    if (IsA(aDataSource, aResource, kRDF_Seq) ||
        IsA(aDataSource, aResource, kRDF_Bag) ||
        IsA(aDataSource, aResource, kRDF_Alt)) {
        *_retval = true;
    }
    else {
        *_retval = false;
    }
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerUtilsImpl::IsEmpty(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource, bool* _retval)
{
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // By default, say that we're an empty container. Even if we're not
    // really even a container.
    *_retval = true;

    nsCOMPtr<nsIRDFNode> nextValNode;
    rv = aDataSource->GetTarget(aResource, kRDF_nextVal, true, getter_AddRefs(nextValNode));
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_RDF_NO_VALUE)
        return NS_OK;

    nsCOMPtr<nsIRDFLiteral> nextValLiteral;
    rv = nextValNode->QueryInterface(NS_GET_IID(nsIRDFLiteral), getter_AddRefs(nextValLiteral));
    if (NS_FAILED(rv)) return rv;

    if (nextValLiteral.get() != kOne)
        *_retval = false;

    return NS_OK;
}


NS_IMETHODIMP
RDFContainerUtilsImpl::IsBag(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, bool *_retval)
{
    NS_PRECONDITION(aDataSource != nullptr, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResource != nullptr, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(_retval != nullptr, "null ptr");
    if (! _retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = IsA(aDataSource, aResource, kRDF_Bag);
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerUtilsImpl::IsSeq(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, bool *_retval)
{
    NS_PRECONDITION(aDataSource != nullptr, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResource != nullptr, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(_retval != nullptr, "null ptr");
    if (! _retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = IsA(aDataSource, aResource, kRDF_Seq);
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerUtilsImpl::IsAlt(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, bool *_retval)
{
    NS_PRECONDITION(aDataSource != nullptr, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResource != nullptr, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(_retval != nullptr, "null ptr");
    if (! _retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = IsA(aDataSource, aResource, kRDF_Alt);
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerUtilsImpl::MakeBag(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, nsIRDFContainer **_retval)
{
    return MakeContainer(aDataSource, aResource, kRDF_Bag, _retval);
}


NS_IMETHODIMP
RDFContainerUtilsImpl::MakeSeq(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, nsIRDFContainer **_retval)
{
    return MakeContainer(aDataSource, aResource, kRDF_Seq, _retval);
}


NS_IMETHODIMP
RDFContainerUtilsImpl::MakeAlt(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, nsIRDFContainer **_retval)
{
    return MakeContainer(aDataSource, aResource, kRDF_Alt, _retval);
}



////////////////////////////////////////////////////////////////////////


RDFContainerUtilsImpl::RDFContainerUtilsImpl()
{
    if (gRefCnt++ == 0) {
        nsresult rv;

        rv = CallGetService(kRDFServiceCID, &gRDFService);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
        if (NS_SUCCEEDED(rv)) {
            gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "instanceOf"),
                                     &kRDF_instanceOf);
            gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "nextVal"),
                                     &kRDF_nextVal);
            gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "Bag"),
                                     &kRDF_Bag);
            gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "Seq"),
                                     &kRDF_Seq);
            gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "Alt"),
                                     &kRDF_Alt);
            gRDFService->GetLiteral(NS_LITERAL_STRING("1").get(), &kOne);
        }
    }
}


RDFContainerUtilsImpl::~RDFContainerUtilsImpl()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: RDFContainerUtilsImpl\n", gInstanceCount);
#endif

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gRDFService);
        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kRDF_nextVal);
        NS_IF_RELEASE(kRDF_Bag);
        NS_IF_RELEASE(kRDF_Seq);
        NS_IF_RELEASE(kRDF_Alt);
        NS_IF_RELEASE(kOne);
    }
}



nsresult
NS_NewRDFContainerUtils(nsIRDFContainerUtils** aResult)
{
    NS_PRECONDITION(aResult != nullptr, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    RDFContainerUtilsImpl* result =
        new RDFContainerUtilsImpl();

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;
    return NS_OK;
}


nsresult
RDFContainerUtilsImpl::MakeContainer(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource, nsIRDFResource* aType, nsIRDFContainer** aResult)
{
    NS_PRECONDITION(aDataSource != nullptr, "null ptr");
    if (! aDataSource)	return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResource != nullptr, "null ptr");
    if (! aResource)	return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aType != nullptr, "null ptr");
    if (! aType)	return NS_ERROR_NULL_POINTER;

    if (aResult)	*aResult = nullptr;

    nsresult rv;

    // Check to see if somebody has already turned it into a container; if so
    // don't try to do it again.
    bool isContainer;
    rv = IsContainer(aDataSource, aResource, &isContainer);
    if (NS_FAILED(rv)) return rv;

    if (!isContainer)
    {
	rv = aDataSource->Assert(aResource, kRDF_instanceOf, aType, true);
	if (NS_FAILED(rv)) return rv;

	rv = aDataSource->Assert(aResource, kRDF_nextVal, kOne, true);
	if (NS_FAILED(rv)) return rv;
    }

    if (aResult) {
        rv = NS_NewRDFContainer(aResult);
        if (NS_FAILED(rv)) return rv;

        rv = (*aResult)->Init(aDataSource, aResource);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

bool
RDFContainerUtilsImpl::IsA(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource, nsIRDFResource* aType)
{
    if (!aDataSource || !aResource || !aType) {
        NS_WARNING("Unexpected null argument");
        return false;
    }

    nsresult rv;

    bool result;
    rv = aDataSource->HasAssertion(aResource, kRDF_instanceOf, aType, true, &result);
    if (NS_FAILED(rv))
      return false;

    return result;
}

NS_IMETHODIMP
RDFContainerUtilsImpl::IndexOf(nsIRDFDataSource* aDataSource, nsIRDFResource* aContainer, nsIRDFNode* aElement, int32_t* aIndex)
{
    if (!aDataSource || !aContainer)
        return NS_ERROR_NULL_POINTER;

    // Assume we can't find it.
    *aIndex = -1;

    // If the resource is null, bail quietly
    if (! aElement)
      return NS_OK;

    // We'll assume that fan-out is much higher than fan-in, so grovel
    // through the inbound arcs, look for an ordinal resource, and
    // decode it.
    nsCOMPtr<nsISimpleEnumerator> arcsIn;
    aDataSource->ArcLabelsIn(aElement, getter_AddRefs(arcsIn));
    if (! arcsIn)
        return NS_OK;

    while (1) {
        bool hasMoreArcs = false;
        arcsIn->HasMoreElements(&hasMoreArcs);
        if (! hasMoreArcs)
            break;

        nsCOMPtr<nsISupports> isupports;
        arcsIn->GetNext(getter_AddRefs(isupports));
        if (! isupports)
            break;

        nsCOMPtr<nsIRDFResource> property =
            do_QueryInterface(isupports);

        if (! property)
            continue;

        bool isOrdinal;
        IsOrdinalProperty(property, &isOrdinal);
        if (! isOrdinal)
            continue;

        nsCOMPtr<nsISimpleEnumerator> sources;
        aDataSource->GetSources(property, aElement, true, getter_AddRefs(sources));
        if (! sources)
            continue;

        while (1) {
            bool hasMoreSources = false;
            sources->HasMoreElements(&hasMoreSources);
            if (! hasMoreSources)
                break;

            nsCOMPtr<nsISupports> isupports2;
            sources->GetNext(getter_AddRefs(isupports2));
            if (! isupports2)
                break;

            nsCOMPtr<nsIRDFResource> source =
                do_QueryInterface(isupports2);

            if (source == aContainer)
                // Found it.
                return OrdinalResourceToIndex(property, aIndex);
        }
    }

    return NS_OK;
}
