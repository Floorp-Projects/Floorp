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

  This builds an HTML-like model, complete with text nodes, that can
  be displayed in a vanilla HTML content viewer. You can apply CSS2
  styles to the text, etc.

 */

#include "nsIRDFContent.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataBase.h"
#include "nsIRDFNode.h"
#include "nsIRDFResourceManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsITextContent.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFDocument.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIContentIID,            NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDocumentIID,           NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFResourceManagerIID, NS_IRDFRESOURCEMANAGER_IID);
static NS_DEFINE_IID(kITextContentIID,        NS_ITEXT_CONTENT_IID); // XXX grr...

static NS_DEFINE_CID(kRDFResourceManagerCID,  NS_RDFRESOURCEMANAGER_CID);
static NS_DEFINE_CID(kTextNodeCID,            NS_TEXTNODE_CID);

////////////////////////////////////////////////////////////////////////

class RDFHTMLDocumentImpl : public nsRDFDocument {
public:
    RDFHTMLDocumentImpl();
    virtual ~RDFHTMLDocumentImpl();

    // nsIRDFDocument interface
    NS_IMETHOD CreateChildren(nsIRDFNode* resource, nsISupportsArray* children);

protected:
    nsresult NewChild(nsIRDFNode* resource,
                      nsIRDFContent*& result,
                      PRBool childrenMustBeGenerated);

    nsresult CreateChild(nsIRDFNode* property,
                         nsIRDFNode* value,
                         nsIRDFContent*& result);
};

////////////////////////////////////////////////////////////////////////

RDFHTMLDocumentImpl::RDFHTMLDocumentImpl(void)
{
}

RDFHTMLDocumentImpl::~RDFHTMLDocumentImpl(void)
{
}

NS_IMETHODIMP
RDFHTMLDocumentImpl::CreateChildren(nsIRDFNode* resource, nsISupportsArray* children)
{
    nsresult rv;

    NS_ASSERTION(mDB, "not initialized");
    if (! mDB)
        return NS_ERROR_NOT_INITIALIZED;

    nsIRDFResourceManager* mgr;
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFResourceManagerCID,
                                                    kIRDFResourceManagerIID,
                                                    (nsISupports**) &mgr)))
        return rv;

    nsIRDFCursor* properties = nsnull;
    PRBool moreProperties;

#ifdef ONLY_CREATE_RDF_CONTAINERS_AS_CONTENT
    if (! rdf_IsContainer(mgr, mDB, resource))
        goto done;
#endif

    // Create a cursor that'll enumerate all of the outbound arcs
    if (NS_FAILED(rv = mDB->ArcLabelsOut(resource, properties)))
        goto done;

    while (NS_SUCCEEDED(rv = properties->HasMoreElements(moreProperties)) && moreProperties) {
        nsIRDFNode* property = nsnull;
        PRBool tv;

        if (NS_FAILED(rv = properties->GetNext(property, tv /* ignored */)))
            break;

        nsAutoString uri;
        if (NS_FAILED(rv = property->GetStringValue(uri))) {
            NS_RELEASE(property);
            break;
        }

#ifdef ONLY_CREATE_RDF_CONTAINERS_AS_CONTENT
        if (! rdf_IsOrdinalProperty(uri)) {
            NS_RELEASE(property);
            continue;
        }
#endif

        // Create a second cursor that'll enumerate all of the values
        // for all of the arcs.
        nsIRDFCursor* values;
        if (NS_FAILED(rv = mDB->GetTargets(resource, property, PR_TRUE, values))) {
            NS_RELEASE(property);
            break;
        }

        PRBool moreValues;
        while (NS_SUCCEEDED(rv = values->HasMoreElements(moreValues)) && moreValues) {
            nsIRDFNode* value = nsnull;
            if (NS_FAILED(rv = values->GetNext(value, tv /* ignored */)))
                break;

            // XXX At this point, we need to decide exactly what kind
            // of kid to create in the content model. For example, for
            // leaf nodes, we probably want to create some kind of
            // text element.
            nsIRDFContent* child;
            if (NS_FAILED(rv = CreateChild(property, value, child))) {
                NS_RELEASE(value);
                break;
            }

            // And finally, add the child into the content model
            children->AppendElement(child);

            NS_RELEASE(child);
            NS_RELEASE(value);
        }

        NS_RELEASE(values);
        NS_RELEASE(property);

        if (NS_FAILED(rv))
            break;
    }

done:
    NS_IF_RELEASE(properties);
    nsServiceManager::ReleaseService(kRDFResourceManagerCID, mgr);

    return rv;
}



nsresult
RDFHTMLDocumentImpl::NewChild(nsIRDFNode* resource,
                              nsIRDFContent*& result,
                              PRBool childrenMustBeGenerated)
{
    nsresult rv;

    nsIRDFContent* child;
    if (NS_FAILED(rv = NS_NewRDFElement(&child)))
        return rv;

    if (NS_FAILED(rv = child->Init(this, resource, childrenMustBeGenerated))) {
        NS_RELEASE(child);
        return rv;
    }

    result = child;
    return NS_OK;
}


nsresult
RDFHTMLDocumentImpl::CreateChild(nsIRDFNode* property,
                                 nsIRDFNode* value,
                                 nsIRDFContent*& result)
{
    nsresult rv;
    nsIRDFContent* child = nsnull;
    nsIRDFContent* grandchild = nsnull;
    nsIContent* grandchild2 = nsnull;
    nsITextContent* text = nsnull;
    nsIDocument* doc = nsnull;
    nsAutoString v;

    // Construct a new child node. We will explicitly be generating
    // all of this node's kids, so set the
    // "children-must-be-generated" flag to false.
    if (NS_FAILED(rv = NewChild(property, child, PR_FALSE)))
        goto error;

    // If this is NOT a resource, then construct a grandchild which is
    // just a vanilla text node.
    if (! rdf_IsResource(value)) {
        if (NS_FAILED(rv = value->GetStringValue(v)))
            goto error;

        if (NS_FAILED(rv = nsRepository::CreateInstance(kTextNodeCID,
                                                        nsnull,
                                                        kIContentIID,
                                                        (void**) &grandchild2)))
            goto error;

        if (NS_FAILED(rv = QueryInterface(kIDocumentIID, (void**) &doc)))
            goto error;

        if (NS_FAILED(rv = grandchild2->SetDocument(doc, PR_FALSE)))
            goto error;

        NS_RELEASE(doc);

        if (NS_FAILED(rv = grandchild2->QueryInterface(kITextContentIID, (void**) &text)))
            goto error;

        if (NS_FAILED(rv = text->SetText(v.GetUnicode(), v.Length(), PR_FALSE)))
            goto error;

        NS_RELEASE(text);

        // hook it up to the child
        if (NS_FAILED(rv = grandchild2->SetParent(child)))
            goto error;

        if (NS_FAILED(rv = child->AppendChildTo(NS_STATIC_CAST(nsIContent*, grandchild2), PR_TRUE)))
            goto error;
    }

    // Construct a grandchild which is another RDF resource node. This
    // node *will* need to recursively generate its children on
    // demand, so set the children-must-be-generated flag to true.
    if (NS_FAILED(rv = NewChild(value, grandchild, PR_TRUE)))
        goto error;

    if (NS_FAILED(rv = child->AppendChildTo(NS_STATIC_CAST(nsIContent*, grandchild), PR_TRUE)))
        goto error;

    // whew!
    result = child;
    return NS_OK;

error:
    NS_IF_RELEASE(text);
    NS_IF_RELEASE(doc);
    NS_IF_RELEASE(grandchild);
    NS_IF_RELEASE(child);
    return rv;
}


////////////////////////////////////////////////////////////////////////

nsresult NS_NewRDFHTMLDocument(nsIRDFDocument** result)
{
    nsIRDFDocument* doc = new RDFHTMLDocumentImpl();
    if (! doc)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(doc);
    *result = doc;
    return NS_OK;
}
