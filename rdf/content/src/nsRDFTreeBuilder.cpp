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

  An nsIRDFDocument implementation that builds a tree widget XUL
  content model that is to be used with a tree control.

 */

#include "nsIAtom.h"
#include "nsIDocument.h"
#include "nsIRDFContent.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataBase.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "rdf.h"
#include "rdfutil.h"

// XXX should go in a header file...
extern nsresult
rdf_AttachTextNode(nsIContent* parent, nsIRDFNode* value);


////////////////////////////////////////////////////////////////////////

#define NC_NAMESPACE_URI "http://home.netscape.com/NC-rdf#"
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Columns);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Column);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Title);

static const char* kColumnsTag  = "COLUMNS";

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIDocumentIID,               NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFResourceIID,            NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,             NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFContentModelBuilderIID, NS_IRDFCONTENTMODELBUILDER_IID);
static NS_DEFINE_IID(kIRDFServiceIID,             NS_IRDFSERVICE_IID);

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);

////////////////////////////////////////////////////////////////////////

class RDFTreeBuilderImpl : public nsIRDFContentModelBuilder
{
private:
    nsIRDFDocument* mDocument;
    nsIRDFService*  mRDFService;
    nsIRDFDataBase* mDB;

public:
    RDFTreeBuilderImpl();
    virtual ~RDFTreeBuilderImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFContentModelBuilder interface
    NS_IMETHOD SetDocument(nsIRDFDocument* aDocument);
    NS_IMETHOD CreateRoot(nsIRDFResource* aResource);
    NS_IMETHOD CreateChildrenFor(nsIRDFContent* aElement);
    NS_IMETHOD OnAssert(nsIRDFContent* aElement, nsIRDFResource* aProperty, nsIRDFNode* aValue);
    NS_IMETHOD OnUnassert(nsIRDFContent* aElement, nsIRDFResource* aProperty, nsIRDFNode* aValue);


    // Implementation methods

    /**
     * Ensure that the specified tag exists as a child of the
     * parent element.
     */
    nsresult
    EnsureChildElement(nsIContent* parent,
                       nsIAtom* tag,
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

    /**
     * Determine if the specified property on the content element is
     * a tree property.
     */
    PRBool
    IsTreeProperty(nsIRDFContent* parent,
                   nsIRDFResource* property,
                   nsIRDFResource* value);

    /**
     * Determine if the specified property on the content element is
     * the root columns property.
     */
    PRBool
    IsRootColumnsProperty(nsIRDFContent* parent,
                          nsIRDFResource* property);
};

////////////////////////////////////////////////////////////////////////

static nsIAtom* kIdAtom;
static nsIAtom* kOpenAtom;
static nsIAtom* kFolderAtom;
static nsIAtom* kChildrenAtom;
static nsIAtom* kColumnsAtom;

nsresult
NS_NewRDFTreeBuilder(nsIRDFContentModelBuilder** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    RDFTreeBuilderImpl* builder = new RDFTreeBuilderImpl();
    if (! builder)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(builder);
    *result = builder;
    return NS_OK;
}



RDFTreeBuilderImpl::RDFTreeBuilderImpl(void)
{
    if (nsnull == kIdAtom) {
        kIdAtom = NS_NewAtom("ID");
        kOpenAtom = NS_NewAtom("OPEN");
        kFolderAtom = NS_NewAtom("FOLDER");
        kChildrenAtom = NS_NewAtom("CHILDREN");
        kColumnsAtom = NS_NewAtom("COLUMNS");
    }
    else {
        NS_ADDREF(kIdAtom);
        NS_ADDREF(kOpenAtom);
        NS_ADDREF(kFolderAtom);
        NS_ADDREF(kChildrenAtom);
        NS_ADDREF(kColumnsAtom);
    }
}

RDFTreeBuilderImpl::~RDFTreeBuilderImpl(void)
{
    NS_IF_RELEASE(mDB);

    if (mRDFService)
        nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);

    nsrefcnt refcnt;
    NS_RELEASE2(kIdAtom, refcnt);
    NS_RELEASE2(kOpenAtom, refcnt);
    NS_RELEASE2(kFolderAtom, refcnt);
    NS_RELEASE2(kChildrenAtom, refcnt);
    NS_RELEASE2(kColumnsAtom, refcnt);
}

////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(RDFTreeBuilderImpl, kIRDFContentModelBuilderIID);

////////////////////////////////////////////////////////////////////////
// nsIRDFContentModelBuilder methods

NS_IMETHODIMP
RDFTreeBuilderImpl::SetDocument(nsIRDFDocument* aDocument)
{
    NS_PRECONDITION(aDocument != nsnull, "null ptr");
    if (! aDocument)
        return NS_ERROR_NULL_POINTER;

    mDocument = aDocument; // not refcounted

    nsresult rv;
    if (NS_FAILED(rv = mDocument->GetDataBase(mDB)))
        return rv;

    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &mRDFService)))
        return rv;

    return NS_OK;
}

NS_IMETHODIMP
RDFTreeBuilderImpl::CreateRoot(nsIRDFResource* aResource)
{
    nsresult rv;
    nsIAtom* tag        = nsnull;
    nsIRDFContent* root = nsnull;
    nsIDocument* doc    = nsnull;

    if ((tag = NS_NewAtom("ROOT")) == nsnull)
        goto done;

    // PR_TRUE indicates that children should be recursively generated on demand
    if (NS_FAILED(rv = NS_NewRDFResourceElement(&root, aResource, kNameSpaceID_None, tag, PR_TRUE)))
        goto done;

    if (NS_FAILED(rv = mDocument->QueryInterface(kIDocumentIID, (void**) &doc)))
        goto done;

    doc->SetRootContent(root);

done:
    NS_IF_RELEASE(doc);
    NS_IF_RELEASE(root); 
    NS_IF_RELEASE(tag);
    return NS_OK;
}


NS_IMETHODIMP
RDFTreeBuilderImpl::CreateChildrenFor(nsIRDFContent* aElement)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFTreeBuilderImpl::OnAssert(nsIRDFContent* parent,
                             nsIRDFResource* property,
                             nsIRDFNode* value)
{
    nsresult rv;
    nsIRDFResource* resource;

    if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFResourceIID, (void**) &resource))) {
        if (IsRootColumnsProperty(parent, property)) {
            rv = AddColumns(parent, resource);
            NS_RELEASE(resource);
            return rv;
        }
        else if (IsTreeProperty(parent, property, resource)) {
            rv = AddTreeChild(parent, property, resource);
            NS_RELEASE(resource);
            return rv;
        }

        // fall through and add as a property
        NS_RELEASE(resource);
    }

    rv = AddPropertyChild(parent, property, value);
    return rv;
}


NS_IMETHODIMP
RDFTreeBuilderImpl::OnUnassert(nsIRDFContent* aElement,
                               nsIRDFResource* aProperty,
                               nsIRDFNode* aValue)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
RDFTreeBuilderImpl::EnsureChildElement(nsIContent* parent,
                                        nsIAtom* tag,
                                        nsIContent*& result)
{
    nsresult rv;
    result = nsnull; // reasonable default

    PRInt32 count;
    if (NS_FAILED(rv = parent->ChildCount(count)))
        return rv;

    for (PRInt32 i = 0; i < count; ++i) {
        nsIContent* kid;
        if (NS_FAILED(rv = parent->ChildAt(i, kid)))
            break;

        nsIAtom* kidTag;
        if (NS_FAILED(rv = kid->GetTag(kidTag))) {
            NS_RELEASE(kid);
            break;
        }

        if (kidTag == tag) {
            NS_RELEASE(kidTag);
            result = kid; // no need to addref, got it from ChildAt().
            return NS_OK;
        }

        NS_RELEASE(kidTag);
        NS_RELEASE(kid);
    }

    nsIContent* element = nsnull;

    if (NS_FAILED(rv))
        goto done;

    // if we get here, we need to construct a new <children> element.

    // XXX no namespace?
    if (NS_FAILED(rv = NS_NewRDFGenericElement(&element, kNameSpaceID_None, tag)))
        goto done;

    if (NS_FAILED(rv = parent->AppendChildTo(element, PR_FALSE)))
        goto done;

    result = element;
    NS_ADDREF(result);

done:
    NS_IF_RELEASE(element);
    return rv;
}

nsresult
RDFTreeBuilderImpl::AddTreeChild(nsIRDFContent* parent,
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

    // Ensure that the <children> element exists on the parent.
    if (NS_FAILED(rv = EnsureChildElement(parent, kChildrenAtom, children)))
        goto done;

    // Create a new child that represents the "value"
    // resource. PR_TRUE indicates that we want the child to
    // dynamically generate its own kids.
    if (NS_FAILED(rv = NS_NewRDFResourceElement(&child, value, kNameSpaceID_None, kFolderAtom, PR_TRUE)))
        goto done;

    if (NS_FAILED(rv = value->GetValue(&p)))
        goto done;

    if (NS_FAILED(rv = child->SetAttribute(kNameSpaceID_HTML, kIdAtom, p, PR_FALSE)))
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
RDFTreeBuilderImpl::AddColumn(nsIContent* parent,
                               nsIRDFResource* property,
                               nsIRDFLiteral* title)
{
    nsresult rv;

    PRInt32 nameSpaceID;
    nsIAtom* tag = nsnull;
    nsIContent* column = nsnull;

    if (NS_FAILED(rv = mDocument->SplitProperty(property, &nameSpaceID, &tag)))
        goto done;

    if (NS_FAILED(rv = NS_NewRDFGenericElement(&column, nameSpaceID, tag)))
        goto done;

    if (NS_FAILED(rv = parent->AppendChildTo(column, PR_FALSE)))
        goto done;

    rv = rdf_AttachTextNode(column, title);

done:
    NS_IF_RELEASE(tag);
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
RDFTreeBuilderImpl::AddColumnsFromContainer(nsIContent* parent,
                                             nsIRDFResource* columns)
{
    nsresult rv;

    nsIContent* columnsElement;
    if (NS_FAILED(rv = EnsureChildElement(parent, kColumnsAtom, columnsElement)))
        return rv;

    nsIRDFResource* NC_Column = nsnull;
    nsIRDFResource* NC_Title  = nsnull;
    nsIRDFAssertionCursor* cursor  = nsnull;

    if (NS_FAILED(rv = mRDFService->GetResource(kURINC_Column, &NC_Column)))
        goto done;

    if (NS_FAILED(rv = mRDFService->GetResource(kURINC_Title, &NC_Title)))
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
RDFTreeBuilderImpl::AddColumnsFromMultiAttributes(nsIContent* parent,
                                                   nsIRDFResource* columns)
{
    nsresult rv;

    nsIContent* columnsElement;
    if (NS_FAILED(rv = EnsureChildElement(parent, kColumnsAtom, columnsElement)))
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

    NS_ASSERTION(rv == NS_ERROR_RDF_CURSOR_EMPTY, "error reading from cursor");
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
RDFTreeBuilderImpl::AddColumns(nsIContent* parent,
                                nsIRDFResource* columns)
{
    nsresult rv;

    if (rdf_IsContainer(mDB, columns)) {
        rv = AddColumnsFromContainer(parent, columns);
    }
    else {
        rv = AddColumnsFromMultiAttributes(parent, columns);
    }

    return rv;
}


nsresult
RDFTreeBuilderImpl::AddPropertyChild(nsIRDFContent* parent,
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
    PRInt32 nameSpaceID;
    nsIAtom* tag = nsnull;

    if (NS_FAILED(rv = EnsureChildElement(parent, kColumnsAtom, columnsElement)))
        goto done;

    if (NS_FAILED(rv = mDocument->SplitProperty(property, &nameSpaceID, &tag)))
        goto done;

    if (NS_FAILED(rv = NS_NewRDFResourceElement(&child, property, nameSpaceID, tag, PR_FALSE)))
        goto done;

    if (NS_FAILED(rv = columnsElement->AppendChildTo(child, PR_TRUE)))
        goto done;

    rv = rdf_AttachTextNode(child, value);

done:
    NS_IF_RELEASE(tag);
    NS_IF_RELEASE(columnsElement);
    NS_IF_RELEASE(child);
    return rv;
}


PRBool
RDFTreeBuilderImpl::IsTreeProperty(nsIRDFContent* parent,
                                   nsIRDFResource* property,
                                   nsIRDFResource* value)
{
    nsresult rv;

    PRBool isTreeProperty = PR_FALSE;
    if (NS_FAILED(rv = mDocument->IsTreeProperty(property, &isTreeProperty)))
        goto done;

    if (isTreeProperty)
        goto done;

    isTreeProperty = rdf_IsContainer(mDB, value);

done:
    return isTreeProperty;
}

PRBool
RDFTreeBuilderImpl::IsRootColumnsProperty(nsIRDFContent* parent,
                                          nsIRDFResource* property)
{
    nsresult rv;

    nsIDocument* doc         = nsnull;
    nsIContent* rootContent  = nsnull;

    PRBool isColumnsProperty = PR_FALSE;;

    if (NS_FAILED(rv = mDocument->QueryInterface(kIDocumentIID, (void**) &doc)))
        goto done;

    if ((rootContent = doc->GetRootContent()) == nsnull)
        goto done;
            
    if (parent != rootContent)
        goto done;

    if (NS_FAILED(property->EqualsString(kURINC_Columns, &isColumnsProperty)))
        goto done;

done:
    NS_IF_RELEASE(rootContent);
    NS_IF_RELEASE(doc);
    return isColumnsProperty;
}
