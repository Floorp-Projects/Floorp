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

#include "nsIRDFCursor.h"
#include "nsIRDFDataBase.h"
#include "nsIRDFNode.h"
#include "nsIRDFResourceManager.h"
#include "nsString.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////
// RDF core vocabulary

#include "rdf.h"
#define RDF_NAMESPACE_URI  "http://www.w3.org/TR/WD-rdf-syntax#"
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

PRBool
rdf_IsOrdinalProperty(const nsIRDFNode* property)
{
    nsAutoString uri;

    if (NS_FAILED(property->GetStringValue(uri)))
        return PR_FALSE;

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
rdf_IsContainer(nsIRDFResourceManager* mgr,
                nsIRDFDataSource* ds,
                nsIRDFNode* resource)
{
    PRBool result = PR_FALSE;

    nsIRDFNode* RDF_instanceOf = nsnull;
    nsIRDFNode* RDF_Bag        = nsnull;
    nsIRDFNode* RDF_Seq        = nsnull;

    nsresult rv;
    if (NS_FAILED(rv = mgr->GetNode(kURIRDF_instanceOf, RDF_instanceOf)))
        goto done;
    
    if (NS_FAILED(rv = mgr->GetNode(kURIRDF_Bag, RDF_Bag)))
        goto done;

    if (NS_FAILED(rv = ds->HasAssertion(resource, RDF_instanceOf, RDF_Bag, PR_TRUE, result)))
        goto done;

    if (result)
        goto done;

    if (NS_FAILED(rv = mgr->GetNode(kURIRDF_Seq, RDF_Seq)))
        goto done;

    if (NS_FAILED(rv = ds->HasAssertion(resource, RDF_instanceOf, RDF_Seq, PR_TRUE, result)))
        goto done;


done:
    NS_IF_RELEASE(RDF_Seq);
    NS_IF_RELEASE(RDF_Bag);
    NS_IF_RELEASE(RDF_instanceOf);
    return result;
}


// A complete hack that looks at the string value of a node and
// guesses if it's a resource
PRBool
rdf_IsResource(nsIRDFNode* node)
{
    nsresult rv;
    nsAutoString v;

    if (NS_FAILED(rv = node->GetStringValue(v)))
        return PR_FALSE;

    PRInt32 index;

    // A URI needs a colon. 
    index = v.Find(':');
    if (index < 0)
        return PR_FALSE;

    // Assume some sane maximum for protocol specs
#define MAX_PROTOCOL_SPEC 10
    if (index > MAX_PROTOCOL_SPEC)
        return PR_FALSE;

    // It can't have spaces or newlines or tabs
    if (v.Find(' ') > 0 || v.Find('\n') > 0 || v.Find('\t') > 0)
        return PR_FALSE;

    return PR_TRUE;
}



// 0. node, node, node
nsresult
rdf_Assert(nsIRDFDataSource* ds,
           nsIRDFNode* subject,
           nsIRDFNode* predicate,
           nsIRDFNode* object)
{
    NS_ASSERTION(ds,        "null ptr");
    NS_ASSERTION(subject,   "null ptr");
    NS_ASSERTION(predicate, "null ptr");
    NS_ASSERTION(object,    "null ptr");

#ifdef DEBUG
    char buf[1024];
    nsAutoString s;

    subject->GetStringValue(s);
    printf("(%s\n", s.ToCString(buf, sizeof buf));
    predicate->GetStringValue(s);
    printf(" %s\n", s.ToCString(buf, sizeof buf));
    object->GetStringValue(s);
    printf(" %s)\n", s.ToCString(buf, sizeof buf));
#endif
    return ds->Assert(subject, predicate, object);
}


// 1. string, string, string
nsresult
rdf_Assert(nsIRDFResourceManager* mgr,
           nsIRDFDataSource* ds,
           const nsString& subjectURI, 
           const nsString& predicateURI,
           const nsString& objectURI)
{
    NS_ASSERTION(mgr, "null ptr");
    NS_ASSERTION(ds, "null ptr");

    nsresult rv;
    nsIRDFNode* subject;
    if (NS_FAILED(rv = mgr->GetNode(subjectURI, subject)))
        return rv;

    rv = rdf_Assert(mgr, ds, subject, predicateURI, objectURI);
    NS_RELEASE(subject);

    return rv;
}

// 2. node, node, string
nsresult
rdf_Assert(nsIRDFResourceManager* mgr,
           nsIRDFDataSource* ds,
           nsIRDFNode* subject,
           nsIRDFNode* predicate,
           const nsString& objectURI)
{
    NS_ASSERTION(mgr,     "null ptr");
    NS_ASSERTION(ds,      "null ptr");
    NS_ASSERTION(subject, "null ptr");

    nsresult rv;
    nsIRDFNode* object;
    if (NS_FAILED(rv = mgr->GetNode(objectURI, object)))
        return rv;

    rv = rdf_Assert(ds, subject, predicate, object);
    NS_RELEASE(object);

    return rv;
}


// 3. node, string, string
nsresult
rdf_Assert(nsIRDFResourceManager* mgr,
           nsIRDFDataSource* ds,
           nsIRDFNode* subject,
           const nsString& predicateURI,
           const nsString& objectURI)
{
    NS_ASSERTION(mgr,     "null ptr");
    NS_ASSERTION(ds,      "null ptr");
    NS_ASSERTION(subject, "null ptr");

    nsresult rv;
    nsIRDFNode* object;
    if (NS_FAILED(rv = mgr->GetNode(objectURI, object)))
        return rv;

    rv = rdf_Assert(mgr, ds, subject, predicateURI, object);
    NS_RELEASE(object);

    return rv;
}

// 4. node, string, node
nsresult
rdf_Assert(nsIRDFResourceManager* mgr,
           nsIRDFDataSource* ds,
           nsIRDFNode* subject,
           const nsString& predicateURI,
           nsIRDFNode* object)
{
    NS_ASSERTION(mgr,     "null ptr");
    NS_ASSERTION(ds,      "null ptr");
    NS_ASSERTION(subject, "null ptr");
    NS_ASSERTION(object,  "null ptr");

    nsresult rv;
    nsIRDFNode* predicate;
    if (NS_FAILED(rv = mgr->GetNode(predicateURI, predicate)))
        return rv;

    rv = rdf_Assert(ds, subject, predicate, object);
    NS_RELEASE(predicate);

    return rv;
}

// 5. string, string, node
nsresult
rdf_Assert(nsIRDFResourceManager* mgr,
           nsIRDFDataSource* ds,
           const nsString& subjectURI,
           const nsString& predicateURI,
           nsIRDFNode* object)
{
    NS_ASSERTION(mgr, "null ptr");
    NS_ASSERTION(ds, "null ptr");

    nsresult rv;
    nsIRDFNode* subject;
    if (NS_FAILED(rv = mgr->GetNode(subjectURI, subject)))
        return rv;

    rv = rdf_Assert(mgr, ds, subject, predicateURI, object);
    NS_RELEASE(subject);

    return rv;
}



nsresult
rdf_CreateAnonymousNode(nsIRDFResourceManager* mgr,
                        nsIRDFNode*& result)
{
    static PRUint32 gCounter = 0;

    nsAutoString s = "$";
    s.Append(++gCounter, 10);

    return mgr->GetNode(s, result);
}

nsresult
rdf_CreateBag(nsIRDFResourceManager* mgr,
              nsIRDFDataSource* ds,
              nsIRDFNode*& result)
{
    NS_ASSERTION(mgr, "null ptr");
    NS_ASSERTION(ds,  "null ptr");

    nsresult rv;
    nsIRDFNode* bag = nsnull;

    result = nsnull; // reasonable default

    if (NS_FAILED(rv = rdf_CreateAnonymousNode(mgr, bag)))
        goto done;

    if (NS_FAILED(rv = rdf_Assert(mgr, ds, bag, kURIRDF_instanceOf, kURIRDF_Bag)))
        goto done;

    if (NS_FAILED(rv = rdf_Assert(mgr, ds, bag, kURIRDF_nextVal, "1")))
        goto done;

    result = bag;
    NS_ADDREF(result);

done:
    NS_IF_RELEASE(bag);
    return rv;
}


nsresult
rdf_CreateSequence(nsIRDFResourceManager* mgr,
                   nsIRDFDataSource* ds,
                   nsIRDFNode*& result)
{
    NS_ASSERTION(mgr, "null ptr");
    NS_ASSERTION(ds,  "null ptr");

    nsresult rv;
    nsIRDFNode* bag = nsnull;

    result = nsnull; // reasonable default

    if (NS_FAILED(rv = rdf_CreateAnonymousNode(mgr, bag)))
        goto done;

    if (NS_FAILED(rv = rdf_Assert(mgr, ds, bag, kURIRDF_instanceOf, kURIRDF_Seq)))
        goto done;

    if (NS_FAILED(rv = rdf_Assert(mgr, ds, bag, kURIRDF_nextVal, "1")))
        goto done;

    result = bag;
    NS_ADDREF(result);

done:
    NS_IF_RELEASE(bag);
    return rv;
}


nsresult
rdf_CreateAlternation(nsIRDFResourceManager* mgr,
                      nsIRDFDataSource* ds,
                      nsIRDFNode*& result)
{
    NS_ASSERTION(mgr, "null ptr");
    NS_ASSERTION(ds,  "null ptr");

    nsresult rv;
    nsIRDFNode* bag = nsnull;

    result = nsnull; // reasonable default

    if (NS_FAILED(rv = rdf_CreateAnonymousNode(mgr, bag)))
        goto done;

    if (NS_FAILED(rv = rdf_Assert(mgr, ds, bag, kURIRDF_instanceOf, kURIRDF_Alt)))
        goto done;

    if (NS_FAILED(rv = rdf_Assert(mgr, ds, bag, kURIRDF_nextVal, "1")))
        goto done;

    result = bag;
    NS_ADDREF(result);

done:
    NS_IF_RELEASE(bag);
    return rv;
}


static nsresult
rdf_ContainerGetNextValue(nsIRDFResourceManager* mgr,
                          nsIRDFDataSource* ds,
                          nsIRDFNode* container,
                          PRInt32& result)
{
    NS_ASSERTION(mgr,       "null ptr");
    NS_ASSERTION(ds,        "null ptr");
    NS_ASSERTION(container, "null ptr");

    nsresult rv;
    nsIRDFNode* nextValProperty = nsnull;
    nsIRDFNode* nextValResource = nsnull;
    nsAutoString nextVal;
    PRInt32 err;

    if (NS_FAILED(rv = mgr->GetNode(kURIRDF_nextVal, nextValProperty)))
        goto done;

    if (NS_FAILED(rv = ds->GetTarget(container, nextValProperty, PR_TRUE, nextValResource)))
        goto done;

    if (NS_FAILED(rv = nextValResource->GetStringValue(nextVal)))
        goto done;

    result = nextVal.ToInteger(&err);
    if (NS_FAILED(err))
        goto done;

    if (NS_FAILED(rv = ds->Unassert(container, nextValProperty, nextValResource)))
        goto done;

    nextVal.Truncate();
    nextVal.Append(result + 1, 10);

    if (NS_FAILED(rv = rdf_Assert(mgr, ds, container, nextValProperty, nextVal)))
        goto done;

done:
    NS_IF_RELEASE(nextValResource);
    NS_IF_RELEASE(nextValProperty);
    return rv;
}



nsresult
rdf_ContainerAddElement(nsIRDFResourceManager* mgr,
                        nsIRDFDataSource* ds,
                        nsIRDFNode* container,
                        nsIRDFNode* element)
{
    NS_ASSERTION(mgr,       "null ptr");
    NS_ASSERTION(ds,        "null ptr");
    NS_ASSERTION(container, "null ptr");
    NS_ASSERTION(element,   "null ptr");

    nsresult rv;

    PRInt32 nextVal;
    nsAutoString uri;

    if (NS_FAILED(rv = rdf_ContainerGetNextValue(mgr, ds, container, nextVal)))
        goto done;

    uri = kRDFNameSpaceURI;
    uri.Append("_");
    uri.Append(nextVal, 10);

    if (rv = rdf_Assert(mgr, ds, container, uri, element))
        goto done;

done:
    return rv;
}



nsresult
rdf_ContainerAddElement(nsIRDFResourceManager* mgr,
                        nsIRDFDataSource* ds,
                        nsIRDFNode* container,
                        const nsString& literalOrURI)
{
    NS_ASSERTION(mgr,       "null ptr");
    NS_ASSERTION(ds,        "null ptr");
    NS_ASSERTION(container, "null ptr");

    nsresult rv;
    nsIRDFNode* node;

    if (NS_FAILED(rv = mgr->GetNode(literalOrURI, node)))
        goto done;

    if (NS_FAILED(rv = rdf_ContainerAddElement(mgr, ds, container, node)))
        goto done;

done:
    NS_IF_RELEASE(node);
    return rv;    
}


PRBool
rdf_ResourceEquals(nsIRDFResourceManager* mgr,
                   nsIRDFNode* resource,
                   const nsString& uri)
{
    NS_ASSERTION(mgr,      "null ptr");
    NS_ASSERTION(resource, "null ptr");

    PRBool result = PR_FALSE;
    nsresult rv;

    nsIRDFNode* that = nsnull;

    if (NS_FAILED(rv = mgr->GetNode(uri, that)))
        goto done;

    result = (that == resource);

done:
    NS_IF_RELEASE(that);
    return result;
}

