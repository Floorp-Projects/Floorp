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

#include "nsIRDFContent.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataBase.h"
#include "nsIRDFNode.h"
#include "nsIRDFResourceManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsRDFDocument.h"
#include "rdf.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

#define NC_NAMESPACE_URI "http://home.netscape.com/NC-rdf#"
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Columns);


static const char* kChildrenTag = "CHILDREN";
static const char* kFolderTag   = "FOLDER";
static const char* kColumnsTag  = "COLUMNS";

////////////////////////////////////////////////////////////////////////

class RDFTreeDocumentImpl : public nsRDFDocument {
public:
    RDFTreeDocumentImpl();
    virtual ~RDFTreeDocumentImpl();

    //NS_IMETHOD SetRootResource(nsIRDFNode* resource);

protected:
    virtual nsresult
    AddChild(nsIRDFContent* parent,
             nsIRDFNode* property,
             nsIRDFNode* value);

    /**
     * Ensure that the specified <tt>tag</tt> exists as a child of the
     * <tt>parent</tt> element.
     */
    nsresult
    EnsureChildElement(nsIContent* parent,
                       const nsString& tag,
                       nsIContent*& result);

    nsresult
    AddTreeChild(nsIRDFContent* parent,
                 nsIRDFNode* property,
                 nsIRDFNode* value);

    nsresult
    AddColumn(nsIContent* parent,
              nsIRDFNode* property,
              nsIRDFNode* title);

    nsresult
    AddColumns(nsIContent* parent,
               nsIRDFNode* columns);

    nsresult
    AddPropertyChild(nsIRDFContent* parent,
                     nsIRDFNode* property,
                     nsIRDFNode* value);
};

////////////////////////////////////////////////////////////////////////

RDFTreeDocumentImpl::RDFTreeDocumentImpl(void)
{
}

RDFTreeDocumentImpl::~RDFTreeDocumentImpl(void)
{
}


nsresult
RDFTreeDocumentImpl::EnsureChildElement(nsIContent* parent,
                                        const nsString& tag,
                                        nsIContent*& result)
{
    nsresult rv;
    result = nsnull; // reasonable default

    nsIAtom* tagAtom = NS_NewAtom(tag);
    if (! tagAtom)
        return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 count;
    if (NS_FAILED(rv = parent->ChildCount(count))) {
        NS_RELEASE(tagAtom);
        return rv;
    }

    for (PRInt32 i = 0; i < count; ++i) {
        nsIContent* kid;
        if (NS_FAILED(rv = parent->ChildAt(i, kid)))
            break;

        nsIAtom* kidTagAtom;
        if (NS_FAILED(rv = kid->GetTag(kidTagAtom))) {
            NS_RELEASE(kid);
            break;
        }

        if (kidTagAtom == tagAtom) {
            NS_RELEASE(kidTagAtom);
            NS_RELEASE(tagAtom);
            result = kid; // no need to addref, got it from ChildAt().
            return NS_OK;
        }

        NS_RELEASE(kidTagAtom);
        NS_RELEASE(kid);
    }

    NS_RELEASE(tagAtom);

    if (NS_FAILED(rv))
        return rv;

    // if we get here, we need to construct a new <children> element.

    // XXX this should be a generic XML container element, not an nsRDFElement
    nsIRDFContent* children = nsnull;

    if (NS_FAILED(rv = NS_NewRDFElement(&children)))
        goto done;

    if (NS_FAILED(rv = children->Init(this, tag, nsnull /* XXX */, PR_FALSE)))
        goto done;

    if (NS_FAILED(rv = parent->AppendChildTo(children, PR_FALSE)))
        goto done;

    result = children;
    NS_ADDREF(result);

done:
    NS_IF_RELEASE(children);
    return rv;
}

nsresult
RDFTreeDocumentImpl::AddTreeChild(nsIRDFContent* parent,
                                  nsIRDFNode* property,
                                  nsIRDFNode* value)
{
    // If it's a tree property, then we need to add the new child
    // element to a special "children" element in the parent.  The
    // child element's value will be the value of the
    // property. We'll also attach an "ID=" attribute to the new
    // child; e.g.,
    //
    // <parent>
    //   ...
    //   <children>
    //     <item id="value">
    //        <!-- recursively generated -->
    //     </item>
    //   </children>
    //   ...
    // </parent>

    nsresult rv;
    nsIRDFContent* child = nsnull;
    nsIContent* children = nsnull;
    nsAutoString s;

    // Ensure that the <children> element exists on the parent.
    if (NS_FAILED(rv = EnsureChildElement(parent, kChildrenTag, children)))
        goto done;

    // Create a new child that represents the "value"
    // resource. PR_TRUE indicates that we want the child to
    // dynamically generate its own kids.
    if (NS_FAILED(rv = NewChild(kFolderTag, value, child, PR_TRUE)))
        goto done;

    if (NS_FAILED(rv = value->GetStringValue(s)))
        goto done;

    if (NS_FAILED(rv = child->SetAttribute("ID", s, PR_FALSE)))
        goto done;

#define ALL_NODES_OPEN_HACK
#ifdef ALL_NODES_OPEN_HACK
    if (NS_FAILED(rv = child->SetAttribute("OPEN", "TRUE", PR_FALSE)))
        goto done;
#endif

    // Finally, add the thing to the <children> element.
    children->AppendChildTo(child, PR_TRUE);

done:
    NS_IF_RELEASE(child);
    NS_IF_RELEASE(children);
    return rv;
}

nsresult
RDFTreeDocumentImpl::AddColumn(nsIContent* parent,
                               nsIRDFNode* property,
                               nsIRDFNode* title)
{
    nsresult rv;

    nsAutoString tag;
    if (NS_FAILED(rv = property->GetStringValue(tag)))
        return rv;

    // XXX this should be a generic XML container element, not an nsRDFElement
    nsIRDFContent* column = nsnull;

    if (NS_FAILED(rv = NS_NewRDFElement(&column)))
        goto done;

    if (NS_FAILED(rv = column->Init(this, tag, nsnull /* XXX */, PR_FALSE)))
        goto done;

    if (NS_FAILED(rv = parent->AppendChildTo(column, PR_FALSE)))
        goto done;

    rv = AttachTextNode(column, title);

done:
    NS_IF_RELEASE(column);
    return rv;
}


nsresult
RDFTreeDocumentImpl::AddColumns(nsIContent* parent,
                                nsIRDFNode* columns)
{
    nsresult rv;
    PRBool moreElements;

    nsIContent* columnsElement;
    if (NS_FAILED(rv = EnsureChildElement(parent, kColumnsTag, columnsElement)))
        return rv;

    nsIRDFCursor* cursor = nsnull;
    if (NS_FAILED(rv = mDB->ArcLabelsOut(columns, cursor)))
        goto done;

    while (NS_SUCCEEDED(rv = cursor->HasMoreElements(moreElements)) && moreElements) {
        nsIRDFNode* property;
        PRBool tv; /* ignored */

        if (NS_SUCCEEDED(rv = cursor->GetNext(property, tv))) {
            nsIRDFNode* title;
            if (NS_SUCCEEDED(rv = mDB->GetTarget(columns, property, PR_TRUE, title))) {
                rv = AddColumn(columnsElement, property, title);
                NS_RELEASE(title);
            }
            NS_RELEASE(property);
        }

        if (NS_FAILED(rv))
            break;
    }

done:
    NS_IF_RELEASE(cursor);
    return rv;
}


nsresult
RDFTreeDocumentImpl::AddPropertyChild(nsIRDFContent* parent,
                                      nsIRDFNode* property,
                                      nsIRDFNode* value)
{
    // Otherwise, it's not a tree property. So we'll just create a
    // new element for the property, and a simple text node for
    // its value; e.g.,
    //
    // <parent>
    //   <columns>
    //     <folder>value</folder>
    //   </columns>
    //   ...
    // </parent>
    nsresult rv;
    nsIContent* columnsElement = nsnull;
    nsIRDFContent* child       = nsnull;
    nsAutoString s;

    if (NS_FAILED(rv = EnsureChildElement(parent, kColumnsTag, columnsElement)))
        goto done;

    if (NS_FAILED(rv = property->GetStringValue(s)))
        goto done;

    // XXX this should be a generic XML element, *not* an nsRDFElement.
    if (NS_FAILED(rv = NewChild(s, property, child, PR_FALSE)))
        goto done;

    if (NS_FAILED(rv = AttachTextNode(child, value)))
        goto done;

    rv = columnsElement->AppendChildTo(child, PR_TRUE);

done:
    NS_IF_RELEASE(columnsElement);
    NS_IF_RELEASE(child);
    return rv;
}


static PRBool
rdf_ContentResourceEquals(nsIRDFResourceManager* mgr,
                          nsIRDFContent* element,
                          const nsString& uri)
{
    nsresult rv;
    nsIRDFNode* resource = nsnull;
    if (NS_FAILED(rv = element->GetResource(resource)))
        return PR_FALSE;

    PRBool result = rdf_ResourceEquals(mgr, resource, uri);
    NS_RELEASE(resource);
    return result;
}


nsresult
RDFTreeDocumentImpl::AddChild(nsIRDFContent* parent,
                              nsIRDFNode* property,
                              nsIRDFNode* value)
{
    if (IsTreeProperty(property) || rdf_IsContainer(mResourceMgr, mDB, value)) {
        return AddTreeChild(parent, property, value);
    }
    else if (rdf_ResourceEquals(mResourceMgr, property, kURINC_Columns)) {
        return AddColumns(parent, value);
    }
    else {
        return AddPropertyChild(parent, property, value);
    }
}


////////////////////////////////////////////////////////////////////////

nsresult NS_NewRDFTreeDocument(nsIRDFDocument** result)
{
    nsIRDFDocument* doc = new RDFTreeDocumentImpl();
    if (! doc)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(doc);
    *result = doc;
    return NS_OK;
}



