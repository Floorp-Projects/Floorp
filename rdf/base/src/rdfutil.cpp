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
#include "plstr.h"
#include "prprf.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////
// RDF core vocabulary

#include "rdf.h"
static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Alt);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Bag);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Description);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, ID);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, RDF);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Seq);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, about);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, aboutEach);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, bagID);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, li);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, resource);

DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, nextVal); // ad hoc way to make containers fast

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

    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &gRDFService)))
        goto done;

    if (NS_FAILED(rv = gRDFService->GetResource(kURIRDF_instanceOf, &kRDF_instanceOf)))
        goto done;

    if (NS_FAILED(rv = gRDFService->GetResource(kURIRDF_Bag, &kRDF_Bag)))
        goto done;

    if (NS_FAILED(rv = gRDFService->GetResource(kURIRDF_Seq, &kRDF_Seq)))
        goto done;

    if (NS_FAILED(rv = gRDFService->GetResource(kURIRDF_Alt, &kRDF_Alt)))
        goto done;

    if (NS_FAILED(rv = gRDFService->GetResource(kURIRDF_nextVal, &kRDF_nextVal)))
        goto done;

done:
    if (NS_FAILED(rv)) {
        NS_IF_RELEASE(kRDF_nextVal);
        NS_IF_RELEASE(kRDF_Alt);
        NS_IF_RELEASE(kRDF_Seq);
        NS_IF_RELEASE(kRDF_Bag);

        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////

PRBool
rdf_IsOrdinalProperty(const nsIRDFResource* property)
{
    const char* s;
    if (NS_FAILED(property->GetValue(&s)))
        return PR_FALSE;

    if (PL_strncmp(s, kRDFNameSpaceURI, sizeof(kRDFNameSpaceURI) - 1) != 0)
        return PR_FALSE;

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
    const char* s;
    if (NS_FAILED(aOrdinal->GetValue(&s)))
        return PR_FALSE;

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


PRBool
rdf_IsContainer(nsIRDFDataSource* ds,
                nsIRDFResource* resource)
{
    if (rdf_IsBag(ds, resource) ||
        rdf_IsSeq(ds, resource) ||
        rdf_IsAlt(ds, resource)) {
        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }
}

PRBool
rdf_IsBag(nsIRDFDataSource* aDataSource,
          nsIRDFResource* aResource)
{
    PRBool result = PR_FALSE;

    nsresult rv;
    if (NS_FAILED(rv = rdf_EnsureRDFService()))
        goto done;

    rv = aDataSource->HasAssertion(aResource, kRDF_instanceOf, kRDF_Bag, PR_TRUE, &result);

done:
    return result;
}


PRBool
rdf_IsSeq(nsIRDFDataSource* aDataSource,
          nsIRDFResource* aResource)
{
    PRBool result = PR_FALSE;

    nsresult rv;
    if (NS_FAILED(rv = rdf_EnsureRDFService()))
        goto done;

    rv = aDataSource->HasAssertion(aResource, kRDF_instanceOf, kRDF_Seq, PR_TRUE, &result);

done:
    return result;
}


PRBool
rdf_IsAlt(nsIRDFDataSource* aDataSource,
          nsIRDFResource* aResource)
{
    PRBool result = PR_FALSE;

    nsresult rv;
    if (NS_FAILED(rv = rdf_EnsureRDFService()))
        goto done;

    rv = aDataSource->HasAssertion(aResource, kRDF_instanceOf, kRDF_Alt, PR_TRUE, &result);

done:
    return result;
}

// A complete hack that looks at the string value of a node and
// guesses if it's a resource
static PRBool
rdf_IsResource(const nsString& s)
{
    PRInt32 index;

    // A URI needs a colon. 
    index = s.Find(':');
    if (index < 0)
        return PR_FALSE;

    // Assume some sane maximum for protocol specs
#define MAX_PROTOCOL_SPEC 10
    if (index > MAX_PROTOCOL_SPEC)
        return PR_FALSE;

    // It can't have spaces or newlines or tabs
    if (s.Find(' ') > 0 || s.Find('\n') > 0 || s.Find('\t') > 0)
        return PR_FALSE;

    return PR_TRUE;
}

// 0. node, node, node
nsresult
rdf_Assert(nsIRDFDataSource* ds,
           nsIRDFResource* subject,
           nsIRDFResource* predicate,
           nsIRDFNode* object)
{
    NS_ASSERTION(ds,        "null ptr");
    NS_ASSERTION(subject,   "null ptr");
    NS_ASSERTION(predicate, "null ptr");
    NS_ASSERTION(object,    "null ptr");

#ifdef DEBUG_waterson
    const char* s;
    predicate->GetValue(&s);
    printf(" %s", (strchr(s, '#') ? strchr(s, '#')+1 : s));
    subject->GetValue(&s);
    printf("(%s, ", s);
     

    nsIRDFResource* objectResource;
    nsIRDFLiteral* objectLiteral;

    if (NS_SUCCEEDED(object->QueryInterface(kIRDFResourceIID, (void**) &objectResource))) {
        objectResource->GetValue(&s);
        printf(" %s)\n", s);
        NS_RELEASE(objectResource);
    }
    else if (NS_SUCCEEDED(object->QueryInterface(kIRDFLiteralIID, (void**) &objectLiteral))) {
        const PRUnichar* p;
        objectLiteral->GetValue(&p);
        nsAutoString s2(p);
        char buf[1024];
        printf(" %s)\n", s2.ToCString(buf, sizeof buf));
        NS_RELEASE(objectLiteral);
    }
#endif
    return ds->Assert(subject, predicate, object, PR_TRUE);
}


// 1. string, string, string
nsresult
rdf_Assert(nsIRDFDataSource* ds,
           const nsString& subjectURI, 
           const nsString& predicateURI,
           const nsString& objectURI)
{
    NS_ASSERTION(ds, "null ptr");

    nsresult rv;
    nsIRDFResource* subject;
    if (NS_FAILED(rv = rdf_EnsureRDFService()))
        return rv;

    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(subjectURI, &subject)))
        return rv;

    rv = rdf_Assert(ds, subject, predicateURI, objectURI);
    NS_RELEASE(subject);

    return rv;
}

// 2. node, node, string
nsresult
rdf_Assert(nsIRDFDataSource* ds,
           nsIRDFResource* subject,
           nsIRDFResource* predicate,
           const nsString& objectURI)
{
    NS_ASSERTION(ds,      "null ptr");
    NS_ASSERTION(subject, "null ptr");

    nsresult rv;
    nsIRDFNode* object;

    if (NS_FAILED(rv = rdf_EnsureRDFService()))
        return rv;

    // XXX Make a guess *here* if the object should be a resource or a
    // literal. If you don't like it, then call ds->Assert() yerself.
    if (rdf_IsResource(objectURI)) {
        nsIRDFResource* resource;
        if (NS_FAILED(rv = gRDFService->GetUnicodeResource(objectURI, &resource)))
            return rv;

        object = resource;
    }
    else {
        nsIRDFLiteral* literal;
        if (NS_FAILED(rv = gRDFService->GetLiteral(objectURI, &literal)))
            return rv;

        object = literal;
    }

    rv = rdf_Assert(ds, subject, predicate, object);
    NS_RELEASE(object);

    return rv;
}


// 3. node, string, string
nsresult
rdf_Assert(nsIRDFDataSource* ds,
           nsIRDFResource* subject,
           const nsString& predicateURI,
           const nsString& objectURI)
{
    NS_ASSERTION(ds,      "null ptr");
    NS_ASSERTION(subject, "null ptr");

    nsresult rv;
	 
    if (NS_FAILED(rv = rdf_EnsureRDFService()))
        return rv;

    nsIRDFResource* predicate;
    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(predicateURI, &predicate)))
        return rv;

    rv = rdf_Assert(ds, subject, predicate, objectURI);
    NS_RELEASE(predicate);

    return rv;
}

// 4. node, string, node
nsresult
rdf_Assert(nsIRDFDataSource* ds,
           nsIRDFResource* subject,
           const nsString& predicateURI,
           nsIRDFNode* object)
{
    NS_ASSERTION(ds,      "null ptr");
    NS_ASSERTION(subject, "null ptr");
    NS_ASSERTION(object,  "null ptr");

    nsresult rv;
    if (NS_FAILED(rv = rdf_EnsureRDFService()))
        return rv;

    nsIRDFResource* predicate;
    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(predicateURI, &predicate)))
        return rv;

    rv = rdf_Assert(ds, subject, predicate, object);
    NS_RELEASE(predicate);

    return rv;
}

// 5. string, string, node
nsresult
rdf_Assert(nsIRDFDataSource* ds,
           const nsString& subjectURI,
           const nsString& predicateURI,
           nsIRDFNode* object)
{
    NS_ASSERTION(ds, "null ptr");

    nsresult rv;
    if (NS_FAILED(rv = rdf_EnsureRDFService()))
        return rv;

    nsIRDFResource* subject;
    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(subjectURI, &subject)))
        return rv;

    rv = rdf_Assert(ds, subject, predicateURI, object);
    NS_RELEASE(subject);

    return rv;
}



nsresult
rdf_CreateAnonymousResource(const nsString& aContextURI, nsIRDFResource** result)
{
static PRUint32 gCounter = 0;

    nsresult rv;
    if (NS_FAILED(rv = rdf_EnsureRDFService()))
        return rv;

    do {
        nsAutoString s(aContextURI);
        s.Append("#$");
        s.Append(++gCounter, 10);

        nsIRDFResource* resource;
        if (NS_FAILED(rv = gRDFService->GetUnicodeResource(s, &resource)))
            return rv;

        // XXX an ugly but effective way to make sure that this
        // resource is really unique in the world.
        nsrefcnt refcnt = resource->AddRef();
        resource->Release();

        if (refcnt == 2) {
            *result = resource;
            break;
        }
    } while (1);

    return NS_OK;
}

PRBool
rdf_IsAnonymousResource(const nsString& aContextURI, nsIRDFResource* aResource)
{
    nsresult rv;
    const char* s;
    if (NS_FAILED(rv = aResource->GetValue(&s))) {
        NS_ASSERTION(PR_FALSE, "unable to get resource URI");
        return PR_FALSE;
    }

    nsAutoString uri(s);

    // Make sure that they have the same context (prefix)
    if (uri.Find(aContextURI) != 0)
        return PR_FALSE;

    uri.Cut(0, aContextURI.Length());

    // Anonymous resources look like the regexp "\$[0-9]+"
    if (uri[0] != '#' || uri[1] != '$')
        return PR_FALSE;

    for (PRInt32 i = uri.Length() - 1; i >= 1; --i) {
        if (uri[i] < '0' || uri[i] > '9')
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

nsresult
rdf_MakeBag(nsIRDFDataSource* ds,
            nsIRDFResource* bag)
{
    NS_ASSERTION(ds,  "null ptr");
    NS_ASSERTION(bag, "null ptr");

    nsresult rv;

    // XXX check to see if someone else has already made this into a container.

    if (NS_FAILED(rv = rdf_Assert(ds, bag, kURIRDF_instanceOf, kURIRDF_Bag)))
        return rv;

    if (NS_FAILED(rv = rdf_Assert(ds, bag, kURIRDF_nextVal, "1")))
        return rv;

    return NS_OK;
}


nsresult
rdf_MakeSeq(nsIRDFDataSource* ds,
            nsIRDFResource* seq)
{
    NS_ASSERTION(ds,  "null ptr");
    NS_ASSERTION(seq, "null ptr");

    nsresult rv;

    // XXX check to see if someone else has already made this into a container.

    if (NS_FAILED(rv = rdf_Assert(ds, seq, kURIRDF_instanceOf, kURIRDF_Seq)))
        return rv;

    if (NS_FAILED(rv = rdf_Assert(ds, seq, kURIRDF_nextVal, "1")))
        return rv;

    return NS_OK;
}


nsresult
rdf_MakeAlt(nsIRDFDataSource* ds,
            nsIRDFResource* alt)
{
    NS_ASSERTION(ds,  "null ptr");
    NS_ASSERTION(alt, "null ptr");

    nsresult rv;

    // XXX check to see if someone else has already made this into a container.

    if (NS_FAILED(rv = rdf_Assert(ds, alt, kURIRDF_instanceOf, kURIRDF_Alt)))
        return rv;

    if (NS_FAILED(rv = rdf_Assert(ds, alt, kURIRDF_nextVal, "1")))
        return rv;

    return NS_OK;
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
    if (NS_FAILED(rv = gRDFService->GetLiteral(s, getter_AddRefs(nextVal)))) {
        NS_ERROR("unable to get nextVal literal");
        return rv;
    }

    if (NS_FAILED(rv = aDataSource->Assert(aContainer, kRDF_nextVal, nextVal, PR_TRUE))) {
        NS_ERROR("unable to update nextVal");
        return rv;
    }

    return NS_OK;
}

static nsresult
rdf_ContainerGetNextValue(nsIRDFDataSource* ds,
                          nsIRDFResource* container,
                          nsIRDFResource** result)
{
    NS_ASSERTION(ds,        "null ptr");
    NS_ASSERTION(container, "null ptr");

    nsresult rv;
    if (NS_FAILED(rv = rdf_EnsureRDFService()))
        return rv;

    nsIRDFNode* nextValNode       = nsnull;
    nsIRDFLiteral* nextValLiteral = nsnull;
    const PRUnichar* s;
    nsAutoString nextValStr;
    PRInt32 nextVal;
    PRInt32 err;

    // Get the next value, which hangs off of the bag via the
    // RDF:nextVal property.
    if (NS_FAILED(rv = ds->GetTarget(container, kRDF_nextVal, PR_TRUE, &nextValNode)))
        goto done;

    if (NS_FAILED(rv = nextValNode->QueryInterface(kIRDFLiteralIID, (void**) &nextValLiteral)))
        goto done;

    if (NS_FAILED(rv = nextValLiteral->GetValue(&s)))
        goto done;

    nextValStr = s;
    nextVal = nextValStr.ToInteger(&err);
    if (NS_FAILED(err))
        goto done;

    // Generate a URI that we can return.
    nextValStr = kRDFNameSpaceURI;
    nextValStr.Append("_");
    nextValStr.Append(nextVal, 10);

    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(nextValStr, result)))
        goto done;

    // Now increment the RDF:nextVal property.
    if (NS_FAILED(rv = ds->Unassert(container, kRDF_nextVal, nextValLiteral)))
        goto done;

    NS_RELEASE(nextValLiteral);

    ++nextVal;
    nextValStr.Truncate();
    nextValStr.Append(nextVal, 10);

    if (NS_FAILED(rv = gRDFService->GetLiteral(nextValStr, &nextValLiteral)))
        goto done;

    if (NS_FAILED(rv = rdf_Assert(ds, container, kRDF_nextVal, nextValLiteral)))
        goto done;

done:
    NS_IF_RELEASE(nextValLiteral);
    NS_IF_RELEASE(nextValNode);
    return rv;
}





nsresult
rdf_ContainerAppendElement(nsIRDFDataSource* ds,
                           nsIRDFResource* container,
                           nsIRDFNode* element)
{
    NS_ASSERTION(ds,        "null ptr");
    NS_ASSERTION(container, "null ptr");
    NS_ASSERTION(element,   "null ptr");

    nsresult rv;

    nsIRDFResource* nextVal;

    if (NS_FAILED(rv = rdf_ContainerGetNextValue(ds, container, &nextVal)))
        goto done;

    if (NS_FAILED(rv = rdf_Assert(ds, container, nextVal, element)))
        goto done;

done:
    return rv;
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

    while (NS_SUCCEEDED(rv = elements->Advance())) {
        nsCOMPtr<nsIRDFNode> element;
        if (NS_FAILED(rv = elements->GetObject(getter_AddRefs(element)))) {
            NS_ERROR("unable to read cursor");
            return rv;
        }

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
        if (NS_FAILED(rv = elements->GetPredicate(getter_AddRefs(ordinal)))) {
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
        while (NS_SUCCEEDED(rv = elements->Advance())) {
            if (NS_FAILED(rv = elements->GetObject(getter_AddRefs(element)))) {
                NS_ERROR("unable to get element from cursor");
                return rv;
            }

            if (NS_FAILED(rv = elements->GetPredicate(getter_AddRefs(ordinal)))) {
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

            if (NS_FAILED(rv = aDataSource->Assert(aContainer, ordinal, element, PR_TRUE))) {
                NS_ERROR("unable to add element to the container");
                return rv;
            }

            ++index;
        }

        NS_ASSERTION(rv == NS_ERROR_RDF_CURSOR_EMPTY, "severe error advancing cursor");

        // Update the container's nextVal to reflect this mumbo jumbo
        if (NS_FAILED(rv = rdf_ContainerSetNextValue(aDataSource, aContainer, index))) {
            NS_ERROR("unable to update container's nextVal");
            return rv;
        }

        return NS_OK;
    }

    NS_ASSERTION(rv == NS_ERROR_RDF_CURSOR_EMPTY, "severe error advancing cursor");

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
    while (NS_SUCCEEDED(rv = elements->Advance())) {
        if (NS_FAILED(rv = elements->GetPredicate(getter_AddRefs(ordinal)))) {
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

    // Make sure Advance() didn't bomb...
    NS_ASSERTION(NS_SUCCEEDED(rv) || rv == NS_ERROR_RDF_CURSOR_EMPTY,
                 "severe error advancing cursor");

    if (NS_FAILED(rv) && rv != NS_ERROR_RDF_CURSOR_EMPTY)
        return rv;

    // Remember if we've exhausted the cursor: if so, this degenerates
    // into a simple "append" operation.
    PRBool cursorExhausted = (rv == NS_ERROR_RDF_CURSOR_EMPTY);

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
            if (NS_FAILED(rv = elements->GetPredicate(getter_AddRefs(ordinal)))) {
                NS_ERROR("unable to get element's ordinal index");
                return rv;
            }

            nsCOMPtr<nsIRDFNode> element;
            if (NS_FAILED(rv = elements->GetObject(getter_AddRefs(element)))) {
                NS_ERROR("unable to get element from cursor");
                return rv;
            }

            if (NS_FAILED(rv = aDataSource->Unassert(aContainer, ordinal, element))) {
                NS_ERROR("unable to remove element from container");
                return rv;
            }

            if (NS_FAILED(rv = rdf_IndexToOrdinalResource(++index, getter_AddRefs(ordinal)))) {
                NS_ERROR("unable to convert index to ordinal resource");
                return rv;
            }

            if (NS_FAILED(rv = aDataSource->Assert(aContainer, ordinal, element, PR_TRUE))) {
                NS_ERROR("unable to add element to container");
                return rv;
            }
        } while (NS_SUCCEEDED(rv = elements->Advance()));

        NS_ASSERTION(rv == NS_ERROR_RDF_CURSOR_EMPTY, "severe error advancing cursor");
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
    while (NS_SUCCEEDED(rv = elements->Advance())) {
        nsCOMPtr<nsIRDFNode> element;
        if (NS_FAILED(rv = elements->GetObject(getter_AddRefs(element)))) {
            NS_ERROR("unable to get element from cursor");
            return rv;
        }

        // Okay, we've found it.
        nsCOMPtr<nsIRDFResource> ordinal;
        if (NS_FAILED(rv = elements->GetPredicate(getter_AddRefs(ordinal)))) {
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

    NS_ASSERTION(rv == NS_ERROR_RDF_CURSOR_EMPTY, "severe error advancing cursor");

    NS_WARNING("element not found");
    *aIndex = -1;
    return NS_OK;
}

