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

 */

#include "nsIRDFCursor.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "plstr.h"
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

    nsAutoString uri = s;

    if (uri.Find(kRDFNameSpaceURI) != 0)
        return PR_FALSE;

    nsAutoString tag(uri);
    tag.Cut(0, sizeof(kRDFNameSpaceURI) - 1);

    if (tag[0] != '_')
        return PR_FALSE;

    for (PRInt32 i = tag.Length() - 1; i >= 1; --i) {
        if (tag[i] < '0' || tag[i] > '9')
            return PR_FALSE;
    }

    return PR_TRUE;
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

#ifdef DEBUG
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
rdf_ContainerAddElement(nsIRDFDataSource* ds,
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



