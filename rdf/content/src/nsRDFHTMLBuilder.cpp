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

  An nsIRDFDocument implementation that builds an HTML-like model,
  complete with text nodes. The model can be displayed in a vanilla
  HTML content viewer by applying CSS2 styles to the text.

 */

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsISupportsArray.h"
#include "nsRDFContentUtils.h"
#include "nsXPIDLString.h"
#include "rdf.h"
#include "rdfutil.h"
#include "prlog.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIContentIID,                NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDocumentIID,               NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFResourceIID,            NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,             NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFContentModelBuilderIID, NS_IRDFCONTENTMODELBUILDER_IID);

////////////////////////////////////////////////////////////////////////

class RDFHTMLBuilderImpl : public nsIRDFContentModelBuilder
{
private:
    nsIRDFDocument*            mDocument;
    nsIRDFCompositeDataSource* mDB;

    // pseudo constants
    PRInt32 kNameSpaceID_RDF; // note that this is a bona fide member

    static nsrefcnt gRefCnt;
    static nsIAtom* kIdAtom;

public:
    RDFHTMLBuilderImpl();
    virtual ~RDFHTMLBuilderImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFContentModelBuilder interface
    NS_IMETHOD SetDocument(nsIRDFDocument* aDocument);
    NS_IMETHOD SetDataBase(nsIRDFCompositeDataSource* aDataBase);
    NS_IMETHOD GetDataBase(nsIRDFCompositeDataSource** aDataBase);
    NS_IMETHOD CreateRootContent(nsIRDFResource* aResource);
    NS_IMETHOD SetRootContent(nsIContent* aElement);
    NS_IMETHOD CreateContents(nsIContent* aElement);
    NS_IMETHOD OnAssert(nsIContent* aElement, nsIRDFResource* aProperty, nsIRDFNode* aValue);
    NS_IMETHOD OnUnassert(nsIContent* aElement, nsIRDFResource* aProperty, nsIRDFNode* aValue);

    // Implementation methods
    nsresult AddTreeChild(nsIContent* aParent,
                          nsIRDFResource* property,
                          nsIRDFResource* value);

    nsresult AddLeafChild(nsIContent* parent,
                          nsIRDFResource* property,
                          nsIRDFLiteral* value);

    PRBool IsTreeProperty(nsIRDFResource* aProperty);

    nsresult CreateResourceElement(PRInt32 aNameSpaceID,
                                   nsIAtom* aTag,
                                   nsIRDFResource* aResource,
                                   nsIContent** aResult);
};

////////////////////////////////////////////////////////////////////////

nsrefcnt RDFHTMLBuilderImpl::gRefCnt;
nsIAtom* RDFHTMLBuilderImpl::kIdAtom;

RDFHTMLBuilderImpl::RDFHTMLBuilderImpl(void)
    : mDocument(nsnull),
      mDB(nsnull)
{
	NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        kIdAtom = NS_NewAtom("id");
    }
}

RDFHTMLBuilderImpl::~RDFHTMLBuilderImpl(void)
{
    NS_IF_RELEASE(mDB);
    // NS_IF_RELEASE(mDocument) not refcounted

    if (--gRefCnt == 0) {
        NS_RELEASE(kIdAtom);
    }
}

////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(RDFHTMLBuilderImpl, kIRDFContentModelBuilderIID);

////////////////////////////////////////////////////////////////////////

nsresult
RDFHTMLBuilderImpl::AddTreeChild(nsIContent* parent,
                                 nsIRDFResource* property,
                                 nsIRDFResource* value)
{
    // If it's a tree property, then create a child element whose
    // value is the value of the property. We'll also attach an "ID="
    // attribute to the new child; e.g.,
    //
    // <parent>
    //   <property id="value">
    //      <!-- recursively generated -->
    //   </property>
    //   ...
    // </parent>

    nsresult rv;
    PRInt32 nameSpaceID;
    nsIAtom* tag = nsnull;
    nsIContent* child = nsnull;
    nsXPIDLCString p;

    if (NS_FAILED(rv = mDocument->SplitProperty(property, &nameSpaceID, &tag)))
        goto done;

    if (NS_FAILED(rv = CreateResourceElement(nameSpaceID, tag, value, &child)))
        goto done;

    if (NS_FAILED(rv = value->GetValue( getter_Copies(p) )))
        goto done;

    if (NS_FAILED(rv = child->SetAttribute(kNameSpaceID_HTML, kIdAtom, (const char*) p, PR_FALSE)))
        goto done;

    rv = parent->AppendChildTo(child, PR_TRUE);

done:
    NS_IF_RELEASE(child);
    NS_IF_RELEASE(tag);
    return rv;
}


nsresult
RDFHTMLBuilderImpl::AddLeafChild(nsIContent* parent,
                                 nsIRDFResource* property,
                                 nsIRDFLiteral* value)
{
    // Otherwise, it's not a tree property. So we'll just create a
    // new element for the property, and a simple text node for
    // its value; e.g.,
    //
    // <parent>
    //   <property>value</property>
    //   ...
    // </parent>

    nsresult rv;
    PRInt32 nameSpaceID;
    nsIAtom* tag = nsnull;
    nsIContent* child = nsnull;

    if (NS_FAILED(rv = mDocument->SplitProperty(property, &nameSpaceID, &tag)))
        goto done;

    if (NS_FAILED(rv = CreateResourceElement(nameSpaceID, tag, property, &child)))
        goto done;

    if (NS_FAILED(rv = parent->AppendChildTo(child, PR_TRUE)))
        goto done;

    rv = nsRDFContentUtils::AttachTextNode(child, value);

done:
    NS_IF_RELEASE(tag);
    NS_IF_RELEASE(child);
    return rv;
}


NS_IMETHODIMP
RDFHTMLBuilderImpl::SetDocument(nsIRDFDocument* aDocument)
{
    // note: document can now be null to indicate going away
    mDocument = aDocument; // not refcounted
    if (aDocument != nsnull)
    {
	    nsCOMPtr<nsIDocument> doc( do_QueryInterface(mDocument) );
	    if (doc) {
		nsCOMPtr<nsINameSpaceManager> mgr;
		doc->GetNameSpaceManager( *getter_AddRefs(mgr) );
		if (mgr) {
	static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;
		    mgr->GetNameSpaceID(kRDFNameSpaceURI, kNameSpaceID_RDF);
		}
	    }
    }
    return NS_OK;
}


NS_IMETHODIMP
RDFHTMLBuilderImpl::SetDataBase(nsIRDFCompositeDataSource* aDataBase)
{
    NS_PRECONDITION(aDataBase != nsnull, "null ptr");
    if (! aDataBase)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(mDB == nsnull, "already initialized");
    if (mDB)
        return NS_ERROR_ALREADY_INITIALIZED;

    mDB = aDataBase;
    NS_ADDREF(mDB);
    return NS_OK;
}


NS_IMETHODIMP
RDFHTMLBuilderImpl::GetDataBase(nsIRDFCompositeDataSource** aDataBase)
{
    NS_PRECONDITION(aDataBase != nsnull, "null ptr");
    if (! aDataBase)
        return NS_ERROR_NULL_POINTER;

    *aDataBase = mDB;
    NS_ADDREF(mDB);
    return NS_OK;
}



NS_IMETHODIMP
RDFHTMLBuilderImpl::CreateRootContent(nsIRDFResource* aResource)
{
    NS_PRECONDITION(mDocument != nsnull, "not initialized");
    if (! mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;
    nsIAtom* tag        = nsnull;
    nsIDocument* doc    = nsnull;
    nsIContent* root    = nsnull;
    nsIContent* body = nsnull;

    if (NS_FAILED(rv = mDocument->QueryInterface(kIDocumentIID, (void**) &doc)))
        goto done;

    rv = NS_ERROR_OUT_OF_MEMORY;
    if ((tag = NS_NewAtom("document")) == nsnull)
        goto done;

    if (NS_FAILED(rv = NS_NewRDFElement(kNameSpaceID_None, tag, &root)))
        goto done;

    doc->SetRootContent(NS_STATIC_CAST(nsIContent*, root));

    NS_RELEASE(tag);

    rv = NS_ERROR_OUT_OF_MEMORY;
    if ((tag = NS_NewAtom("body")) == nsnull)
        goto done;

    // PR_TRUE indicates that children should be recursively generated on demand
    if (NS_FAILED(rv = CreateResourceElement(kNameSpaceID_None, tag, aResource, &body)))
        goto done;

    if (NS_FAILED(rv = root->AppendChildTo(body, PR_FALSE)))
        goto done;

done:
    NS_IF_RELEASE(body); 
    NS_IF_RELEASE(root);
    NS_IF_RELEASE(tag);
    NS_IF_RELEASE(doc);
    return NS_OK;
}


NS_IMETHODIMP
RDFHTMLBuilderImpl::SetRootContent(nsIContent* aElement)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFHTMLBuilderImpl::CreateContents(nsIContent* aElement)
{
    NS_NOTYETIMPLEMENTED("Adapt the implementation from RDFTreeBuilderImpl");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFHTMLBuilderImpl::OnAssert(nsIContent* parent,
                             nsIRDFResource* property,
                             nsIRDFNode* value)
{
    NS_PRECONDITION(mDocument != nsnull, "not initialized");
    if (! mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    nsIRDFResource* valueResource;
    if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFResourceIID, (void**) &valueResource))) {
        // If it's a tree property or an RDF container, then add it as
        // a tree child and return.
        if (IsTreeProperty(property) /* || rdf_IsContainer(mDB, valueResource) */) {
            rv = AddTreeChild(parent, property, valueResource);
            NS_RELEASE(valueResource);
            return rv;
        }

        // Otherwise, fall through and add try to add it as a property
        NS_RELEASE(valueResource);
    }

    nsIRDFLiteral* valueLiteral;
    if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFLiteralIID, (void**) &valueLiteral))) {
        rv = AddLeafChild(parent, property, valueLiteral);
        NS_RELEASE(valueLiteral);
    }
	else {
		return NS_OK; // ignore resources on the leaf.
	}
    return rv;
}


NS_IMETHODIMP
RDFHTMLBuilderImpl::OnUnassert(nsIContent* parent,
                               nsIRDFResource* property,
                               nsIRDFNode* value)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


PRBool
RDFHTMLBuilderImpl::IsTreeProperty(nsIRDFResource* aProperty)
{
    // XXX This whole method is a mega-kludge. This should be read off
    // of the element somehow...

#define TREE_PROPERTY_HACK
#if defined(TREE_PROPERTY_HACK)
    nsXPIDLCString p;
    aProperty->GetValue( getter_Copies(p) );
    nsAutoString s(p);
    if (s.Equals(NC_NAMESPACE_URI "child") ||
        s.Equals(NC_NAMESPACE_URI "Folder") ||
		s.Equals(NC_NAMESPACE_URI "Columns") ||
		s.Equals(RDF_NAMESPACE_URI "child")) {
        return PR_TRUE;
    }
#endif // defined(TREE_PROPERTY_HACK)
#if 0
    if (rdf_IsOrdinalProperty(aProperty)) {
        return PR_TRUE;
    }
#endif
    return PR_FALSE;
}



nsresult
RDFHTMLBuilderImpl::CreateResourceElement(PRInt32 aNameSpaceID,
                                          nsIAtom* aTag,
                                          nsIRDFResource* aResource,
                                          nsIContent** aResult)
{
    nsresult rv;

    nsCOMPtr<nsIContent> result;
    if (NS_FAILED(rv = NS_NewRDFElement(aNameSpaceID, aTag, getter_AddRefs(result))))
        return rv;

    nsXPIDLCString uri;
    if (NS_FAILED(rv = aResource->GetValue( getter_Copies(uri) )))
        return rv;

    if (NS_FAILED(rv = result->SetAttribute(kNameSpaceID_None, kIdAtom, (const char*) uri, PR_FALSE)))
        return rv;

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}



////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFHTMLBuilder(nsIRDFContentModelBuilder** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    RDFHTMLBuilderImpl* builder = new RDFHTMLBuilderImpl();
    if (! builder)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(builder);
    *result = builder;
    return NS_OK;
}
