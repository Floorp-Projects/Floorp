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
#include "nsINameSpaceManager.h"
#include "nsISupportsArray.h"
#include "nsRDFDocument.h"
#include "rdf.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

#define NC_NAMESPACE_URI "http://home.netscape.com/NC-rdf#"
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Columns);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Column);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Title);

static const char* kChildrenTag = "CHILDREN";
static const char* kFolderTag   = "FOLDER";
static const char* kColumnsTag  = "COLUMNS";

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,  NS_IRDFLITERAL_IID);

////////////////////////////////////////////////////////////////////////

class RDFTreeDocumentImpl : public nsRDFDocument {
public:
    RDFTreeDocumentImpl();
    virtual ~RDFTreeDocumentImpl();

    //NS_IMETHOD SetRootResource(nsIRDFNode* resource);

protected:
    virtual nsresult
    AddChild(nsIRDFContent* parent,
             nsIRDFResource* property,
             nsIRDFNode* value);

    /**
     * Ensure that the specified <tt>tag</tt> exists as a child of the
     * <tt>parent</tt> element.
     */
    nsresult
    EnsureChildElement(nsIContent* parent,
                       const nsString& tag,
                       nsIContent*& result);

    /**
     * Add a new child to the content model from a tree property. This creates
     * the necessary cruft in the content model to ensure that content will
     * recursively be generated as the content model is descended.
     */
    nsresult
    AddTreeChild(nsIRDFContent* parent,
                 nsIRDFResource* property,
                 nsIRDFResource* value);

    /**
     * Add a single column to the content model.
     */
    nsresult
    AddColumn(nsIContent* parent,
              nsIRDFResource* property,
              nsIRDFLiteral* title);


    /**
     * Add columns to the content model from an RDF container (sequence or bag)
     */
    nsresult
    AddColumnsFromContainer(nsIContent* parent,
                            nsIRDFResource* columns);


    /**
     * Add columns to the content model from a set of multi-attributes
     */
    nsresult
    AddColumnsFromMultiAttributes(nsIContent* parent,
                                  nsIRDFResource* columns);

    /**
     * Add columns to the content model.
     */
    nsresult
    AddColumns(nsIContent* parent,
               nsIRDFResource* columns);

    /**
     * Add a new property element to the content model
     */
    nsresult
    AddPropertyChild(nsIRDFContent* parent,
                     nsIRDFResource* property,
                     nsIRDFNode* value);
};

////////////////////////////////////////////////////////////////////////

static nsIAtom* kIdAtom;
static nsIAtom* kOpenAtom;

RDFTreeDocumentImpl::RDFTreeDocumentImpl(void)
{
  if (nsnull == kIdAtom) {
    kIdAtom = NS_NewAtom("ID");
    kOpenAtom = NS_NewAtom("open");
  }
  else {
    NS_ADDREF(kIdAtom);
    NS_ADDREF(kOpenAtom);
  }
}

RDFTreeDocumentImpl::~RDFTreeDocumentImpl(void)
{
  nsrefcnt refcnt;
  NS_RELEASE2(kIdAtom, refcnt);
  NS_RELEASE2(kOpenAtom, refcnt);
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
                                  nsIRDFResource* property,
                                  nsIRDFResource* value)
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
    const char* p;
    nsAutoString s;

    // Ensure that the <children> element exists on the parent.
    if (NS_FAILED(rv = EnsureChildElement(parent, kChildrenTag, children)))
        goto done;

    // Create a new child that represents the "value"
    // resource. PR_TRUE indicates that we want the child to
    // dynamically generate its own kids.
    if (NS_FAILED(rv = NewChild(kFolderTag, value, child, PR_TRUE)))
        goto done;

    if (NS_FAILED(rv = value->GetValue(&p)))
        goto done;

    s = p;

    if (NS_FAILED(rv = child->SetAttribute(kNameSpaceID_HTML, kIdAtom, s, PR_FALSE)))
        goto done;

#define ALL_NODES_OPEN_HACK
#ifdef ALL_NODES_OPEN_HACK
    if (NS_FAILED(rv = child->SetAttribute(kNameSpaceID_None, kOpenAtom, "TRUE", PR_FALSE)))
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
                               nsIRDFResource* property,
                               nsIRDFLiteral* title)
{
    nsresult rv;

    const char* s;
    if (NS_FAILED(rv = property->GetValue(&s)))
        return rv;

    nsAutoString tag = s;

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


/*

  When columns are specified using a container, the data model is
  assumed to look something like the following example:

  <RDF:RDF>
    <RDF:Description RDF:about="nc:history">
      <NC:Columns>
        <RDF:Seq>
          <RDF:li>
            <NC:Description
                NC:Column="http://home.netscape.com/NC-rdf#LastVisitDate"
                NC:Title="Date Last Visited"/>
          </RDF:li>

          <RDF:li>
            <NC:Description
                NC:Column="http://home.netscape.com/NC-rdf#URL"
                NC:Title="URL"/>
          </RDF:li>

          <RDF:li>
            <NC:Description
                NC:Column="http://home.netscape.com/NC-rdf#Title"
                NC:Title="Title"/>
          </RDF:li>
        </RDF:Seq>
      </NC:Columns>

      <!-- ...whatever... -->

    </RDF:Description>
  </RDF:RDF>

 */

nsresult
RDFTreeDocumentImpl::AddColumnsFromContainer(nsIContent* parent,
                                             nsIRDFResource* columns)
{
    nsresult rv;

    nsIContent* columnsElement;
    if (NS_FAILED(rv = EnsureChildElement(parent, kColumnsTag, columnsElement)))
        return rv;

    nsIRDFResource* NC_Column = nsnull;
    nsIRDFResource* NC_Title  = nsnull;
    nsIRDFAssertionCursor* cursor  = nsnull;

    if (NS_FAILED(rv = mResourceMgr->GetResource(kURINC_Column, &NC_Column)))
        goto done;

    if (NS_FAILED(rv = mResourceMgr->GetResource(kURINC_Title, &NC_Title)))
        goto done;

    if (NS_FAILED(rv = NS_NewContainerCursor(mDB, columns, &cursor)))
        goto done;

    while (NS_SUCCEEDED(rv = cursor->Advance())) {
        nsIRDFNode* columnNode;
        if (NS_SUCCEEDED(rv = cursor->GetObject(&columnNode))) {
            nsIRDFResource* column;
            if (NS_SUCCEEDED(rv = columnNode->QueryInterface(kIRDFResourceIID, (void**) &column))) {
                // XXX okay, error recovery leaves a bit to be desired here...
                nsIRDFNode* propertyNode = nsnull;
                nsIRDFResource* property = nsnull;
                nsIRDFNode* titleNode    = nsnull;
                nsIRDFLiteral* title     = nsnull;

                rv = mDB->GetTarget(column, NC_Column, PR_TRUE, &propertyNode);
                PR_ASSERT(NS_SUCCEEDED(rv));

                rv = propertyNode->QueryInterface(kIRDFResourceIID, (void**) &property);
                PR_ASSERT(NS_SUCCEEDED(rv));

                rv = mDB->GetTarget(column, NC_Title, PR_TRUE, &titleNode);
                PR_ASSERT(NS_SUCCEEDED(rv));

                rv = titleNode->QueryInterface(kIRDFLiteralIID, (void**) &title);
                PR_ASSERT(NS_SUCCEEDED(rv));

                AddColumn(columnsElement, property, title);

                NS_IF_RELEASE(title);
                NS_IF_RELEASE(titleNode);
                NS_IF_RELEASE(property);
                NS_IF_RELEASE(propertyNode);

                NS_RELEASE(column);
            }
            NS_RELEASE(columnNode);
        }

        if (NS_FAILED(rv))
            break;
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY)
        // This is a "normal" return code from nsIRDFCursor::Advance().
        rv = NS_OK;

done:
    NS_IF_RELEASE(cursor);
    NS_IF_RELEASE(NC_Title);
    NS_IF_RELEASE(NC_Column);
    return rv;
}

/*

  When the columns are specified using multi-attributes, the data
  model assumed to look something like the following example:

  <RDF:RDF>
    <RDF:Description RDF:about="nc:history">
      <NC:Columns>
        <NC:LastVisitDate RDF:ID="#001">Date Last Visited</NC:LastVisitDate>
        <NC:URL RDF:ID="#002">URL</NC:URL>
        <NC:Title RDF:ID="#003">Title</NC:URL>
        <NC:Order>
          <RDF:Seq>
            <RDF:li RDF:resource="#003"/>
            <RDF:li RDF:resource="#002"/>
            <RDF:li RDF:resource="#001"/>
          </RDF:Seq>
        </NC:Order>
      </NC:Columns>

      <!-- ...whatever... -->

    </RDF:Description>
  </RDF:RDF>

  TODO: implement the NC:Order property on the column set. This is not
  exactly a slam dunk; it requires getting reification working
  properly in the RDF content sink.

 */
nsresult
RDFTreeDocumentImpl::AddColumnsFromMultiAttributes(nsIContent* parent,
                                                   nsIRDFResource* columns)
{
    nsresult rv;

    nsIContent* columnsElement;
    if (NS_FAILED(rv = EnsureChildElement(parent, kColumnsTag, columnsElement)))
        return rv;

    nsIRDFArcsOutCursor* cursor = nsnull;
    if (NS_FAILED(rv = mDB->ArcLabelsOut(columns, &cursor)))
        goto done;

    while (NS_SUCCEEDED(rv = cursor->Advance())) {
        nsIRDFResource* property;
        if (NS_SUCCEEDED(rv = cursor->GetPredicate(&property))) {
            nsIRDFNode* titleNode;
            if (NS_SUCCEEDED(rv = mDB->GetTarget(columns, property, PR_TRUE, &titleNode))) {
                nsIRDFLiteral* title;
                if (NS_SUCCEEDED(rv = titleNode->QueryInterface(kIRDFLiteralIID,
                                                                (void**) &title))) {
                    rv = AddColumn(columnsElement, property, title);
                    NS_RELEASE(title);
                }
                NS_RELEASE(titleNode);
            }
            NS_RELEASE(property);
        }

        if (NS_FAILED(rv))
            break;
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY)
        // This is a reasonable return code from nsIRDFCursor::Advance()
        rv = NS_OK;

    // XXX Now search for an "order" property and apply it to the
    // above to sort them...

done:
    NS_IF_RELEASE(cursor);
    return rv;
}

nsresult
RDFTreeDocumentImpl::AddColumns(nsIContent* parent,
                                nsIRDFResource* columns)
{
    nsresult rv;
    
    if (rdf_IsContainer(mResourceMgr, mDB, columns)) {
        rv = AddColumnsFromContainer(parent, columns);
    }
    else {
        rv = AddColumnsFromMultiAttributes(parent, columns);
    }
    return rv;
}


nsresult
RDFTreeDocumentImpl::AddPropertyChild(nsIRDFContent* parent,
                                      nsIRDFResource* property,
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
    const char* p;
    nsAutoString s;

    if (NS_FAILED(rv = EnsureChildElement(parent, kColumnsTag, columnsElement)))
        goto done;

    if (NS_FAILED(rv = property->GetValue(&p)))
        goto done;

    s = p;

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


nsresult
RDFTreeDocumentImpl::AddChild(nsIRDFContent* parent,
                              nsIRDFResource* property,
                              nsIRDFNode* value)
{
    nsresult rv;
    nsIRDFResource* resource;
    if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFResourceIID, (void**) &resource))) {
        PRBool isColumnsProperty;

        if (parent == mRootContent &&
            NS_SUCCEEDED(property->EqualsString(kURINC_Columns, &isColumnsProperty)) &&
            isColumnsProperty) {
            rv = AddColumns(parent, resource);

            NS_RELEASE(resource);
            return rv;
        }
        else if (IsTreeProperty(property) || rdf_IsContainer(mResourceMgr, mDB, resource)) {
            rv = AddTreeChild(parent, property, resource);

            NS_RELEASE(resource);
            return rv;
        }

        NS_RELEASE(resource);
    }

    return AddPropertyChild(parent, property, value);
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



