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
    NS_IMETHOD IsOrdinalProperty(nsIRDFResource *aProperty, PRBool *_retval);
    NS_IMETHOD IndexToOrdinalResource(PRInt32 aIndex, nsIRDFResource **_retval);
    NS_IMETHOD OrdinalResourceToIndex(nsIRDFResource *aOrdinal, PRInt32 *_retval);
    NS_IMETHOD IsContainer(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, PRBool *_retval);
    NS_IMETHOD IsBag(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, PRBool *_retval);
    NS_IMETHOD IsSeq(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, PRBool *_retval);
    NS_IMETHOD IsAlt(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, PRBool *_retval);
    NS_IMETHOD MakeBag(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, nsIRDFContainer **_retval);
    NS_IMETHOD MakeSeq(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, nsIRDFContainer **_retval);
    NS_IMETHOD MakeAlt(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, nsIRDFContainer **_retval);

private:
    friend nsresult NS_NewRDFContainerUtils(nsIRDFContainerUtils** aResult);

    RDFContainerUtilsImpl();
    virtual ~RDFContainerUtilsImpl();

    nsresult MakeContainer(nsIRDFDataSource* aDataSource,
                           nsIRDFResource* aResource,
                           nsIRDFResource* aType,
                           nsIRDFContainer** aResult);

    // pseudo constants
    static PRInt32 gRefCnt;
    static nsIRDFService* gRDFService;
    static nsIRDFResource* kRDF_instanceOf;
    static nsIRDFResource* kRDF_nextVal;
    static nsIRDFResource* kRDF_Bag;
    static nsIRDFResource* kRDF_Seq;
    static nsIRDFResource* kRDF_Alt;
};


PRInt32         RDFContainerUtilsImpl::gRefCnt = 0;
nsIRDFService*  RDFContainerUtilsImpl::gRDFService;
nsIRDFResource* RDFContainerUtilsImpl::kRDF_instanceOf;
nsIRDFResource* RDFContainerUtilsImpl::kRDF_nextVal;
nsIRDFResource* RDFContainerUtilsImpl::kRDF_Bag;
nsIRDFResource* RDFContainerUtilsImpl::kRDF_Seq;
nsIRDFResource* RDFContainerUtilsImpl::kRDF_Alt;

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ISUPPORTS(RDFContainerUtilsImpl, nsIRDFContainerUtils::GetIID());

////////////////////////////////////////////////////////////////////////
// nsIRDFContainerUtils interface

NS_IMETHODIMP
RDFContainerUtilsImpl::IsOrdinalProperty(nsIRDFResource *aProperty, PRBool *_retval)
{
    nsresult rv;

    nsXPIDLCString propertyStr;
    rv = aProperty->GetValue( getter_Copies(propertyStr) );
    if (NS_FAILED(rv)) return rv;

    if (PL_strncmp(propertyStr, kRDFNameSpaceURI, sizeof(kRDFNameSpaceURI) - 1) != 0) {
        *_retval = PR_FALSE;
        return NS_OK;
    }

    const char* s = propertyStr;
    s += sizeof(kRDFNameSpaceURI) - 1;
    if (*s != '_') {
        *_retval = PR_FALSE;
        return NS_OK;
    }

    ++s;
    while (*s) {
        if (*s < '0' || *s > '9') {
            *_retval = PR_FALSE;
            return NS_OK;
        }

        ++s;
    }

    *_retval = PR_TRUE;
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerUtilsImpl::IndexToOrdinalResource(PRInt32 aIndex, nsIRDFResource **aOrdinal)
{
    NS_PRECONDITION(aIndex > 0, "illegal value");
    if (aIndex <= 0)
        return NS_ERROR_ILLEGAL_VALUE;

    // 16 digits should be plenty to hold a decimal version of a
    // PRInt32.
    char buf[sizeof(kRDFNameSpaceURI) + 16 + 1];

    PL_strcpy(buf, kRDFNameSpaceURI);
    buf[sizeof(kRDFNameSpaceURI) - 1] = '_';
    
    PR_snprintf(buf + sizeof(kRDFNameSpaceURI), 16, "%ld", aIndex);
    
    nsresult rv;

    rv = gRDFService->GetResource(buf, aOrdinal);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get ordinal resource");
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerUtilsImpl::OrdinalResourceToIndex(nsIRDFResource *aOrdinal, PRInt32 *aIndex)
{
    nsXPIDLCString ordinalStr;
    if (NS_FAILED(aOrdinal->GetValue( getter_Copies(ordinalStr) )))
        return PR_FALSE;

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

    PRInt32 index = 0;

    ++s;
    while (*s) {
        if (*s < '0' || *s > '9') {
            NS_ERROR("not an ordinal");
            return NS_ERROR_UNEXPECTED;
        }

        index *= 10;
        index += (*s - '0');

        ++s;
    }

    *aIndex = index;
    return NS_OK;
}

NS_IMETHODIMP
RDFContainerUtilsImpl::IsContainer(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, PRBool *_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! _retval)
        return NS_ERROR_NULL_POINTER;

    if (rdf_IsA(aDataSource, aResource, kRDF_Bag) ||
        rdf_IsA(aDataSource, aResource, kRDF_Seq) ||
        rdf_IsA(aDataSource, aResource, kRDF_Alt)) {
        *_retval = PR_TRUE;
    }
    else {
        *_retval = PR_FALSE;
    }
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerUtilsImpl::IsBag(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, PRBool *_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! _retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = rdf_IsA(aDataSource, aResource, kRDF_Bag);
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerUtilsImpl::IsSeq(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, PRBool *_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! _retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = rdf_IsA(aDataSource, aResource, kRDF_Seq);
    return NS_OK;
}


NS_IMETHODIMP
RDFContainerUtilsImpl::IsAlt(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, PRBool *_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! _retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = rdf_IsA(aDataSource, aResource, kRDF_Alt);
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
    NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        nsresult rv;

        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          nsIRDFService::GetIID(),
                                          (nsISupports**) &gRDFService);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
        if (NS_SUCCEEDED(rv)) {
            gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_instanceOf);
            gRDFService->GetResource(RDF_NAMESPACE_URI "nextVal",    &kRDF_nextVal);
            gRDFService->GetResource(RDF_NAMESPACE_URI "Bag",        &kRDF_Bag);
            gRDFService->GetResource(RDF_NAMESPACE_URI "Seq",        &kRDF_Seq);
            gRDFService->GetResource(RDF_NAMESPACE_URI "Alt",        &kRDF_Alt);
        }
    }
}


RDFContainerUtilsImpl::~RDFContainerUtilsImpl()
{
    if (--gRefCnt == 0) {
        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }
        
        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kRDF_nextVal);
        NS_IF_RELEASE(kRDF_Bag);
        NS_IF_RELEASE(kRDF_Seq);
        NS_IF_RELEASE(kRDF_Alt);
    }
}



nsresult
NS_NewRDFContainerUtils(nsIRDFContainerUtils** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
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
    nsresult rv;
    rv = aDataSource->Assert(aResource, kRDF_instanceOf, aType, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFLiteral> nextVal;
    rv = gRDFService->GetLiteral(nsAutoString("1").GetUnicode(), getter_AddRefs(nextVal));
    if (NS_FAILED(rv)) return rv;

    rv = aDataSource->Assert(aResource, kRDF_nextVal, nextVal, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    if (aResult) {
        rv = NS_NewRDFContainer(aResult);
        if (NS_FAILED(rv)) return rv;

        rv = (*aResult)->Init(aDataSource, aResource);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

