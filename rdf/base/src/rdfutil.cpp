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

  Implementations for a bunch of useful RDF utility routines.  Many of
  these will eventually be exported outside of RDF.DLL via the
  nsIRDFService interface.

  TO DO

  1) Make this so that it doesn't permanently leak the RDF service
     object.

  2) Make container functions thread-safe. They currently don't ensure
     that the RDF:nextVal property is maintained safely.

 */

#include "nsCOMPtr.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "prprf.h"
#include "rdfutil.h"

#include "rdf.h"
static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,  NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFServiceIID,  NS_IRDFSERVICE_IID);

static NS_DEFINE_CID(kRDFServiceCID,   NS_RDFSERVICE_CID);

////////////////////////////////////////////////////////////////////////

// XXX This'll permanently leak
static nsIRDFService* gRDFService = nsnull;

static nsIRDFResource* kRDF_instanceOf = nsnull;
static nsIRDFResource* kRDF_Bag        = nsnull;
static nsIRDFResource* kRDF_Seq        = nsnull;
static nsIRDFResource* kRDF_Alt        = nsnull;
static nsIRDFResource* kRDF_nextVal    = nsnull;

static nsresult
rdf_EnsureRDFService(void)
{
    if (gRDFService)
        return NS_OK;

    nsresult rv;

    rv = nsServiceManager::GetService(kRDFServiceCID,
                                      kIRDFServiceIID,
                                      (nsISupports**) &gRDFService);

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
    if (NS_FAILED(rv)) return rv;

    rv = gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_instanceOf);
    rv = gRDFService->GetResource(RDF_NAMESPACE_URI "Bag",        &kRDF_Bag);
    rv = gRDFService->GetResource(RDF_NAMESPACE_URI "Seq",        &kRDF_Seq);
    rv = gRDFService->GetResource(RDF_NAMESPACE_URI "Alt",        &kRDF_Alt);
    rv = gRDFService->GetResource(RDF_NAMESPACE_URI "nextVal",    &kRDF_nextVal);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

PRBool
rdf_IsOrdinalProperty(nsIRDFResource* aProperty)
{
    nsXPIDLCString propertyStr;
    if (NS_FAILED(aProperty->GetValue( getter_Copies(propertyStr) )))
        return PR_FALSE;

    if (PL_strncmp(propertyStr, kRDFNameSpaceURI, sizeof(kRDFNameSpaceURI) - 1) != 0)
        return PR_FALSE;

    const char* s = propertyStr;
    s += sizeof(kRDFNameSpaceURI) - 1;
    if (*s != '_')
        return PR_FALSE;

    ++s;
    while (*s) {
        if (*s < '0' || *s > '9')
            return PR_FALSE;

        ++s;
    }

    return PR_TRUE;
}


nsresult
rdf_OrdinalResourceToIndex(nsIRDFResource* aOrdinal, PRInt32* aIndex)
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

nsresult
rdf_IndexToOrdinalResource(PRInt32 aIndex, nsIRDFResource** aOrdinal)
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
    if (NS_FAILED(rv = rdf_EnsureRDFService()))
        return rv;

    if (NS_FAILED(rv = gRDFService->GetResource(buf, aOrdinal))) {
        NS_ERROR("unable to get ordinal resource");
        return rv;
    }
    
    return NS_OK;
}

static PRBool
rdf_IsA(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource, nsIRDFResource* aType)
{
    nsresult rv;
    rv = rdf_EnsureRDFService();
    if (NS_FAILED(rv)) return PR_FALSE;

    PRBool result;
    rv = aDataSource->HasAssertion(aResource, kRDF_instanceOf, aType, PR_TRUE, &result);
    if (NS_FAILED(rv)) return PR_FALSE;

    return result;
}


PRBool
rdf_IsContainer(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource)
{
    nsresult rv;
    rv = rdf_EnsureRDFService();
    if (NS_FAILED(rv)) return PR_FALSE;

    if (rdf_IsA(aDataSource, aResource, kRDF_Bag) ||
        rdf_IsA(aDataSource, aResource, kRDF_Seq) ||
        rdf_IsA(aDataSource, aResource, kRDF_Alt)) {
        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }
}

PRBool
rdf_IsBag(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource)
{
    nsresult rv;
    rv = rdf_EnsureRDFService();
    if (NS_FAILED(rv)) return PR_FALSE;

    return rdf_IsA(aDataSource, aResource, kRDF_Bag);
}


PRBool
rdf_IsSeq(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource)
{
    nsresult rv;
    rv = rdf_EnsureRDFService();
    if (NS_FAILED(rv)) return PR_FALSE;

    return rdf_IsA(aDataSource, aResource, kRDF_Seq);
}


PRBool
rdf_IsAlt(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource)
{
    nsresult rv;
    rv = rdf_EnsureRDFService();
    if (NS_FAILED(rv)) return PR_FALSE;

    return rdf_IsA(aDataSource, aResource, kRDF_Alt);
}


nsresult
rdf_CreateAnonymousResource(const nsString& aContextURI, nsIRDFResource** aResult)
{
static PRUint32 gCounter = 0;

    nsresult rv;
    rv = rdf_EnsureRDFService();
    if (NS_FAILED(rv)) return rv;

    do {
        nsAutoString s(aContextURI);
        s.Append("#$");
        s.Append(++gCounter, 10);

        nsIRDFResource* resource;
        if (NS_FAILED(rv = gRDFService->GetUnicodeResource(s.GetUnicode(), &resource)))
            return rv;

        // XXX an ugly but effective way to make sure that this
        // resource is really unique in the world.
        nsrefcnt refcnt = resource->AddRef();
        resource->Release();

        if (refcnt == 2) {
            *aResult = resource;
            break;
        }
    } while (1);

    return NS_OK;
}

PRBool
rdf_IsAnonymousResource(const nsString& aContextURI, nsIRDFResource* aResource)
{
    nsresult rv;
    nsXPIDLCString s;
    if (NS_FAILED(rv = aResource->GetValue( getter_Copies(s) ))) {
        NS_ASSERTION(PR_FALSE, "unable to get resource URI");
        return PR_FALSE;
    }

    nsAutoString uri((const char*) s);

    // Make sure that they have the same context (prefix)
    if (uri.Find(aContextURI) != 0)
        return PR_FALSE;

    uri.Cut(0, aContextURI.Length());

    // Anonymous resources look like the regexp "\$[0-9]+"
    if (uri.CharAt(0) != '#' || uri.CharAt(1) != '$')
        return PR_FALSE;

    for (PRInt32 i = uri.Length() - 1; i >= 1; --i) {
        if (uri.CharAt(i) < '0' || uri.CharAt(i) > '9')
            return PR_FALSE;
    }

    return PR_TRUE;
}


PR_EXTERN(nsresult)
rdf_PossiblyMakeRelative(const nsString& aContextURI, nsString& aURI)
{
    // This implementation is extremely simple: no ".." or anything
    // fancy like that. If the context URI is not a prefix of the URI
    // in question, we'll just bail.
    if (aURI.Find(aContextURI) != 0)
        return NS_OK;

    // Otherwise, pare down the target URI, removing the context URI.
    aURI.Cut(0, aContextURI.Length());

    if (aURI.First() == '#' || aURI.First() == '/')
        aURI.Cut(0, 1);

    return NS_OK;
}


PR_EXTERN(nsresult)
rdf_PossiblyMakeAbsolute(const nsString& aContextURI, nsString& aURI)
{
    PRInt32 index = aURI.Find(':');
    if (index > 0 && index < 25 /* XXX */)
        return NS_OK;

    PRUnichar last  = aContextURI.Last();
    PRUnichar first = aURI.First();

    nsAutoString result(aContextURI);
    if (last == '#' || last == '/') {
        if (first == '#') {
            result.Truncate(result.Length() - 2);
        }
        result.Append(aURI);
    }
    else if (first == '#') {
        result.Append(aURI);
    }
    else {
        result.Append('#');
        result.Append(aURI);
    }

    aURI = result;
    return NS_OK;
}


static nsresult
rdf_MakeContainer(nsIRDFDataSource* aDataSource, nsIRDFResource* aContainer, nsIRDFResource* aType)
{
    NS_ASSERTION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_ASSERTION(aContainer != nsnull, "null ptr");
    if (! aContainer)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    rv = aDataSource->Assert(aContainer, kRDF_instanceOf, aType, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFLiteral> nextVal;
    rv = gRDFService->GetLiteral(nsAutoString("1").GetUnicode(), getter_AddRefs(nextVal));
    if (NS_FAILED(rv)) return rv;

    rv = aDataSource->Assert(aContainer, kRDF_nextVal, nextVal, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
rdf_MakeBag(nsIRDFDataSource* aDataSource, nsIRDFResource* aBag)
{
    nsresult rv;
    rv = rdf_EnsureRDFService();
    if (NS_FAILED(rv)) return rv;

    return rdf_MakeContainer(aDataSource, aBag, kRDF_Bag);
}


nsresult
rdf_MakeSeq(nsIRDFDataSource* aDataSource, nsIRDFResource* aSeq)
{
    nsresult rv;
    rv = rdf_EnsureRDFService();
    if (NS_FAILED(rv)) return rv;

    return rdf_MakeContainer(aDataSource, aSeq, kRDF_Seq);
}


nsresult
rdf_MakeAlt(nsIRDFDataSource* aDataSource, nsIRDFResource* aAlt)
{
    nsresult rv;
    rv = rdf_EnsureRDFService();
    if (NS_FAILED(rv)) return rv;

    return rdf_MakeContainer(aDataSource, aAlt, kRDF_Alt);
}


static nsresult
rdf_ContainerSetNextValue(nsIRDFDataSource* aDataSource,
                          nsIRDFResource* aContainer,
                          PRInt32 aIndex)
{
    nsresult rv;

    if (NS_FAILED(rv = rdf_EnsureRDFService()))
        return rv;

    // Remove the current value of nextVal, if there is one.
    nsCOMPtr<nsIRDFNode> nextValNode;
    if (NS_SUCCEEDED(rv = aDataSource->GetTarget(aContainer,
                                                 kRDF_nextVal,
                                                 PR_TRUE,
                                                 getter_AddRefs(nextValNode)))) {
        if (NS_FAILED(rv = aDataSource->Unassert(aContainer, kRDF_nextVal, nextValNode))) {
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

    rv = aDataSource->Assert(aContainer, kRDF_nextVal, nextVal, PR_TRUE);
    if (rv != NS_RDF_ASSERTION_ACCEPTED) {
        NS_ERROR("unable to update nextVal");
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

static nsresult
rdf_ContainerGetNextValue(nsIRDFDataSource* aDataSource,
                          nsIRDFResource* aContainer,
                          nsIRDFResource** aResult)
{
    NS_PRECONDITION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aContainer != nsnull, "null ptr");
    if (! aContainer)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    rv = rdf_EnsureRDFService();
    if (NS_FAILED(rv)) return rv;

    nsIRDFNode* nextValNode       = nsnull;
    nsIRDFLiteral* nextValLiteral = nsnull;
    nsXPIDLString s;
    nsAutoString nextValStr;
    PRInt32 nextVal;
    PRInt32 err;

    // Get the next value, which hangs off of the bag via the
    // RDF:nextVal property.
    if (NS_FAILED(rv = aDataSource->GetTarget(aContainer, kRDF_nextVal, PR_TRUE, &nextValNode)))
        goto done;

    if (rv == NS_RDF_NO_VALUE) {
        rv = NS_ERROR_UNEXPECTED;
        goto done;
    }

    if (NS_FAILED(rv = nextValNode->QueryInterface(kIRDFLiteralIID, (void**) &nextValLiteral)))
        goto done;

    if (NS_FAILED(rv = nextValLiteral->GetValue( getter_Copies(s) )))
        goto done;

    nextValStr = (const PRUnichar*) s;
    nextVal = nextValStr.ToInteger(&err);
    if (NS_FAILED(err))
        goto done;

    // Generate a URI that we can return.
    nextValStr = kRDFNameSpaceURI;
    nextValStr.Append("_");
    nextValStr.Append(nextVal, 10);

    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(nextValStr.GetUnicode(), aResult)))
        goto done;

    // Now increment the RDF:nextVal property.
    if (NS_FAILED(rv = aDataSource->Unassert(aContainer, kRDF_nextVal, nextValLiteral)))
        goto done;

    NS_RELEASE(nextValLiteral);

    ++nextVal;
    nextValStr.Truncate();
    nextValStr.Append(nextVal, 10);

    if (NS_FAILED(rv = gRDFService->GetLiteral(nextValStr.GetUnicode(), &nextValLiteral)))
        goto done;

    if (NS_FAILED(rv = aDataSource->Assert(aContainer, kRDF_nextVal, nextValLiteral, PR_TRUE)))
        goto done;

done:
    NS_IF_RELEASE(nextValLiteral);
    NS_IF_RELEASE(nextValNode);
    return rv;
}





nsresult
rdf_ContainerAppendElement(nsIRDFDataSource* aDataSource,
                           nsIRDFResource* aContainer,
                           nsIRDFNode* aElement)
{
    NS_PRECONDITION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aContainer != nsnull, "null ptr");
    if (! aContainer)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(rdf_IsContainer(aDataSource, aContainer), "not a container");

    nsresult rv;
    nsCOMPtr<nsIRDFResource> nextVal;
    rv = rdf_ContainerGetNextValue(aDataSource, aContainer, getter_AddRefs(nextVal));
    if (NS_FAILED(rv)) return rv;

    rv = aDataSource->Assert(aContainer, nextVal, aElement, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}



nsresult
rdf_ContainerRemoveElement(nsIRDFDataSource* aDataSource,
                           nsIRDFResource* aContainer,
                           nsIRDFNode* aElement)
{
    NS_PRECONDITION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aContainer != nsnull, "null ptr");
    if (! aContainer)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // Create a cursor and grovel through the container looking for
    // the specified element.
    nsCOMPtr<nsIRDFAssertionCursor> elements;
    if (NS_FAILED(rv = NS_NewContainerCursor(aDataSource,
                                             aContainer,
                                             getter_AddRefs(elements)))) {
        NS_ERROR("unable to create container cursor");
        return rv;
    }

    while (1) {
        rv = elements->Advance();
        if (NS_FAILED(rv))
            return rv;

        if (rv == NS_RDF_CURSOR_EMPTY)
            break;

        nsCOMPtr<nsIRDFNode> element;
        if (NS_FAILED(rv = elements->GetTarget(getter_AddRefs(element)))) {
            NS_ERROR("unable to read cursor");
            return rv;
        }

        NS_ASSERTION(rv != NS_RDF_NO_VALUE, "null item in cursor");
        if (rv == NS_RDF_NO_VALUE)
            continue;

        PRBool eq;
        if (NS_FAILED(rv = element->EqualsNode(aElement, &eq))) {
            NS_ERROR("severe error on equality check");
            return rv;
        }

        if (! eq)
            continue;

        // Okay, we've found it.

        // What was it's index?
        nsCOMPtr<nsIRDFResource> ordinal;
        if (NS_FAILED(rv = elements->GetLabel(getter_AddRefs(ordinal)))) {
            NS_ERROR("unable to get element's ordinal index");
            return rv;
        }

        // First, remove the element.
        if (NS_FAILED(rv = aDataSource->Unassert(aContainer, ordinal, element))) {
            NS_ERROR("unable to remove element from the container");
            return rv;
        }

        PRInt32 index;
        if (NS_FAILED(rv = rdf_OrdinalResourceToIndex(ordinal, &index))) {
            NS_ERROR("unable to convert ordinal URI to index");
            return rv;
        }

        // Now slide the rest of the collection backwards to fill in
        // the gap.
        while (1) {
            rv = elements->Advance();
            if (NS_FAILED(rv))
                return rv;

            if (rv == NS_RDF_CURSOR_EMPTY)
                break;

            if (NS_FAILED(rv = elements->GetTarget(getter_AddRefs(element)))) {
                NS_ERROR("unable to get element from cursor");
                return rv;
            }

            NS_ASSERTION(rv != NS_RDF_NO_VALUE, "null value in cursor");
            if (rv == NS_RDF_NO_VALUE)
                continue;

            if (NS_FAILED(rv = elements->GetLabel(getter_AddRefs(ordinal)))) {
                NS_ERROR("unable to get element's ordinal index");
                return rv;
            }

            if (NS_FAILED(rv = aDataSource->Unassert(aContainer, ordinal, element))) {
                NS_ERROR("unable to remove element from the container");
                return rv;
            }

            if (NS_FAILED(rv = rdf_IndexToOrdinalResource(index, getter_AddRefs(ordinal)))) {
                NS_ERROR("unable to construct ordinal resource");
                return rv;
            }

            rv = aDataSource->Assert(aContainer, ordinal, element, PR_TRUE);
            if (rv != NS_RDF_ASSERTION_ACCEPTED) {
                NS_ERROR("unable to add element to the container");
                return NS_ERROR_FAILURE;
            }

            ++index;
        }

        // Update the container's nextVal to reflect this mumbo jumbo
        if (NS_FAILED(rv = rdf_ContainerSetNextValue(aDataSource, aContainer, index))) {
            NS_ERROR("unable to update container's nextVal");
            return rv;
        }

        return NS_OK;
    }

    NS_WARNING("attempt to remove non-existant element from container");
    return NS_OK;
}



nsresult
rdf_ContainerInsertElementAt(nsIRDFDataSource* aDataSource,
                             nsIRDFResource* aContainer,
                             nsIRDFNode* aElement,
                             PRInt32 aIndex)
{
    NS_PRECONDITION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aContainer != nsnull, "null ptr");
    if (! aContainer)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aIndex >= 1, "illegal value");
    if (aIndex < 1)
        return NS_ERROR_ILLEGAL_VALUE;

    nsresult rv;

    nsCOMPtr<nsIRDFAssertionCursor> elements;

    if (NS_FAILED(rv = NS_NewContainerCursor(aDataSource,
                                             aContainer,
                                             getter_AddRefs(elements)))) {
        NS_ERROR("unable to create container cursor");
        return rv;
    }

    nsCOMPtr<nsIRDFResource> ordinal;
    PRInt32 index = 1;

    // Advance the cursor to the aIndex'th element.
    while (1) {
        rv = elements->Advance();
        if (NS_FAILED(rv))
            return rv;

        if (rv == NS_RDF_CURSOR_EMPTY)
            break;

        if (NS_FAILED(rv = elements->GetLabel(getter_AddRefs(ordinal)))) {
            NS_ERROR("unable to get element's ordinal index");
            return rv;
        }

        if (NS_FAILED(rv = rdf_OrdinalResourceToIndex(ordinal, &index))) {
            NS_ERROR("unable to convert ordinal resource to index");
            return rv;
        }

        // XXX Use >= in case any "holes" exist...
        if (index >= aIndex)
            break;
    }

    // Remember if we've exhausted the cursor: if so, this degenerates
    // into a simple "append" operation.
    PRBool cursorExhausted = (rv == NS_RDF_CURSOR_EMPTY);

    // XXX Be paranoid: there may have been a "hole"
    if (index > aIndex)
        index = aIndex;

    // If we ran all the way to the end of the cursor, then "index"
    // will contain the ordinal value of the last element in the
    // container. Increment it by one to get the position at which
    // we'll want to insert it into the container.
    if (cursorExhausted && aIndex > index) {
        if (index != aIndex) {
            // Sanity check only: aIndex was _way_ too large...
            NS_WARNING("out of bounds");
        }

        ++index;
    }

    // At this point "index" contains the index at which we want to
    // insert the new element.
    if (NS_FAILED(rv = rdf_IndexToOrdinalResource(index, getter_AddRefs(ordinal)))) {
        NS_ERROR("unable to convert index to ordinal resource");
        return rv;
    }

    // Insert it!
    if (NS_FAILED(rv = aDataSource->Assert(aContainer, ordinal, aElement, PR_TRUE))) {
        NS_ERROR("unable to add element to container");
        return rv;
    }

    // Now slide the rest of the container "up" by one...
    if (! cursorExhausted) {
        do {
            if (NS_FAILED(rv = elements->GetLabel(getter_AddRefs(ordinal)))) {
                NS_ERROR("unable to get element's ordinal index");
                return rv;
            }

            nsCOMPtr<nsIRDFNode> element;
            if (NS_FAILED(rv = elements->GetTarget(getter_AddRefs(element)))) {
                NS_ERROR("unable to get element from cursor");
                return rv;
            }

            NS_ASSERTION(rv != NS_RDF_NO_VALUE, "null value in cursor");
            if (rv == NS_RDF_NO_VALUE)
                continue;

            if (NS_FAILED(rv = aDataSource->Unassert(aContainer, ordinal, element))) {
                NS_ERROR("unable to remove element from container");
                return rv;
            }

            if (NS_FAILED(rv = rdf_IndexToOrdinalResource(++index, getter_AddRefs(ordinal)))) {
                NS_ERROR("unable to convert index to ordinal resource");
                return rv;
            }

            rv = aDataSource->Assert(aContainer, ordinal, element, PR_TRUE);
            if (rv != NS_RDF_ASSERTION_ACCEPTED) {
                NS_ERROR("unable to add element to container");
                return NS_ERROR_FAILURE;
            }

            rv = elements->Advance();
            if (NS_FAILED(rv))
                return rv;

            if (rv == NS_RDF_CURSOR_EMPTY)
                break;
        }
        while (1);
    }

    // Now update the container's nextVal
    if (NS_FAILED(rv = rdf_ContainerSetNextValue(aDataSource, aContainer, ++index))) {
        NS_ERROR("unable to update container's nextVal");
        return rv;
    }

    return NS_OK;
}


nsresult
rdf_ContainerIndexOf(nsIRDFDataSource* aDataSource,
                     nsIRDFResource* aContainer,
                     nsIRDFNode* aElement,
                     PRInt32* aIndex)
{
    NS_PRECONDITION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aContainer != nsnull, "null ptr");
    if (! aContainer)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aIndex != nsnull, "null ptr");
    if (! aIndex)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsIRDFAssertionCursor> elements;
    if (NS_FAILED(rv = NS_NewContainerCursor(aDataSource,
                                             aContainer,
                                             getter_AddRefs(elements)))) {
        NS_ERROR("unable to create container cursor");
        return rv;
    }


    // Advance the cursor until we find the element we want
    while (1) {
        rv = elements->Advance();
        if (NS_FAILED(rv))
            return rv;

        if (rv == NS_RDF_CURSOR_EMPTY)
            break;

        nsCOMPtr<nsIRDFNode> element;
        if (NS_FAILED(rv = elements->GetTarget(getter_AddRefs(element)))) {
            NS_ERROR("unable to get element from cursor");
            return rv;
        }

        NS_ASSERTION(rv != NS_RDF_NO_VALUE, "null value in cursor");
        if (rv == NS_RDF_NO_VALUE)
            continue;

        // Okay, we've found it.
        nsCOMPtr<nsIRDFResource> ordinal;
        if (NS_FAILED(rv = elements->GetLabel(getter_AddRefs(ordinal)))) {
            NS_ERROR("unable to get element's ordinal index");
            return rv;
        }

        PRInt32 index;
        if (NS_FAILED(rv = rdf_OrdinalResourceToIndex(ordinal, &index))) {
            NS_ERROR("unable to convert ordinal resource to index");
            return rv;
        }

        if (element != nsCOMPtr<nsIRDFNode>( do_QueryInterface(aElement)) )
            continue;

        *aIndex = index;
        return NS_OK;
    }

    NS_WARNING("element not found");
    *aIndex = -1;
    return NS_OK;
}

