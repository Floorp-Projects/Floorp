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

////////////////////////////////////////////////////////////////////////

PRBool
rdf_IsOrdinalProperty(const nsString& uri)
{
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
                nsIRDFDataBase* db,
                nsIRDFNode* resource)
{
    PRBool result = PR_FALSE;

    nsIRDFNode* RDF_instanceOf = nsnull;
    nsIRDFNode* RDF_Bag        = nsnull;

    nsresult rv;
    if (NS_FAILED(rv = mgr->GetNode(kURIRDF_instanceOf, RDF_instanceOf)))
        goto done;
    
    if (NS_FAILED(rv = mgr->GetNode(kURIRDF_Bag, RDF_Bag)))
        goto done;

    rv = db->HasAssertion(resource, RDF_instanceOf, RDF_Bag, PR_TRUE, result);

done:
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

