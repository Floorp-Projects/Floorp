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

  A "pseudo content element" that acts as a proxy to RDF.

  Unfortunately, there is no one right way to transform RDF into a
  document model. Ideally, one would like to use something like XSL to
  define how to do it in a declarative, user-definable way. But since
  we don't have XSL yet, that's not an option. So here we have a
  hard-coded implementation that does the job.

  TO DO

  1) In the absence of XSL, at least factor out the strategy used to
     build the content model so that multiple models can be build
     (e.g., table-like HTML vs. XUI tree control, etc.)

     This involves both hacking the code that generates children for
     presentation, and the code that manipulates children via the DOM,
     which leads us to the next item...

  2) Implement DOM interfaces.

 */

#include "nsDOMEvent.h"
#include "nsIAtom.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIEventStateManager.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataBase.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFResourceManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsITextContent.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFElement.h"

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

static NS_DEFINE_IID(kIDOMNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIContentIID,            NS_ICONTENT_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID,  NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID,   NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID,     NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIDOMNodeListIID,        NS_IDOMNODELIST_IID);
static NS_DEFINE_IID(kIDocumentIID,           NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFContentIID,         NS_IRDFCONTENT_IID);
static NS_DEFINE_IID(kIRDFDataBaseIID,        NS_IRDFDATABASE_IID);
static NS_DEFINE_IID(kIRDFDocumentIID,        NS_IRDFDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFResourceManagerIID, NS_IRDFRESOURCEMANAGER_IID);
static NS_DEFINE_IID(kITextContentIID,        NS_ITEXT_CONTENT_IID); // XXX grr...
static NS_DEFINE_IID(kIXMLContentIID,         NS_IXMLCONTENT_IID);

static NS_DEFINE_CID(kRDFResourceManagerCID,  NS_RDFRESOURCEMANAGER_CID);
static NS_DEFINE_CID(kTextNodeCID,            NS_TEXTNODE_CID);

////////////////////////////////////////////////////////////////////////
// Utility functions

// XXX all of these should go to some generic RDF utility interface
// that lives in rdf.dll

static PRBool
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


static PRBool
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
static PRBool
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



////////////////////////////////////////////////////////////////////////
// RDFDOMNodeListImpl
//
//   Helper class to implement the nsIDOMNodeList interface. It's
//   probably wrong in some sense, as it uses the "naked" content
//   interface to look for kids. (I assume in general this is bad
//   because there may be pseudo-elements created for presentation
//   that aren't visible to the DOM.)
//

class RDFDOMNodeListImpl : public nsIDOMNodeList {
private:
    nsRDFElement* mElement;

public:
    RDFDOMNodeListImpl(nsRDFElement* element) : mElement(element) {
        NS_IF_ADDREF(mElement);
    }

    virtual ~RDFDOMNodeListImpl(void) {
        NS_IF_RELEASE(mElement);
    }

    // nsISupports interface
    NS_DECL_ISUPPORTS

    NS_DECL_IDOMNODELIST
};

NS_IMPL_ISUPPORTS(RDFDOMNodeListImpl, kIDOMNodeListIID);

NS_IMETHODIMP
RDFDOMNodeListImpl::GetLength(PRUint32* aLength)
{
    PRInt32 count;
    nsresult rv;
    if (NS_FAILED(rv = mElement->ChildCount(count)))
        return rv;
    *aLength = count;
    return NS_OK;
}


NS_IMETHODIMP
RDFDOMNodeListImpl::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
    // XXX naive. probably breaks when there are pseudo elements or something.
    nsresult rv;
    nsIContent* contentChild;
    if (NS_FAILED(rv = mElement->ChildAt(aIndex, contentChild)))
        return rv;

    rv = contentChild->QueryInterface(kIDOMNodeIID, (void**) aReturn);
    NS_RELEASE(contentChild);

    return rv;
}

////////////////////////////////////////////////////////////////////////
// nsRDFElement

nsresult
NS_NewRDFElement(nsIRDFContent** result)
{
    NS_PRECONDITION(result, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = NS_STATIC_CAST(nsIRDFContent*, new nsRDFElement());
    if (! *result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*result);
    return NS_OK;
}

nsRDFElement::nsRDFElement(void)
    : mDocument(nsnull),
      mNameSpace(nsnull),
      mNameSpaceId(gNameSpaceId_Unknown),
      mScriptObject(nsnull),
      mResource(nsnull),
      mChildren(nsnull),
      mParent(nsnull)
{
    NS_INIT_REFCNT();
}
 
nsRDFElement::~nsRDFElement()
{
    NS_IF_RELEASE(mNameSpace);
    NS_IF_RELEASE(mResource);
}

NS_IMPL_ADDREF(nsRDFElement);
NS_IMPL_RELEASE(nsRDFElement);

NS_IMETHODIMP 
nsRDFElement::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIRDFContentIID) ||
        iid.Equals(kIXMLContentIID) ||
        iid.Equals(kIContentIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFContent*, this);
    }
    else if (iid.Equals(kIDOMElementIID) ||
             iid.Equals(kIDOMNodeIID)) {
        *result = NS_STATIC_CAST(nsIDOMElement*, this);
    }
    else if (iid.Equals(kIScriptObjectOwnerIID)) {
        *result = NS_STATIC_CAST(nsIScriptObjectOwner*, this);
    }
    else if (iid.Equals(kIDOMEventReceiverIID)) {
        *result = NS_STATIC_CAST(nsIDOMEventReceiver*, this);
    }
    else if (iid.Equals(kIJSScriptObjectIID)) {
        *result = NS_STATIC_CAST(nsIJSScriptObject*, this);
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }

    // if we get here, we know one of the above IIDs was ok.
    AddRef();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIDOMNode interface

NS_IMETHODIMP
nsRDFElement::GetNodeName(nsString& aNodeName)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetNodeValue(nsString& aNodeValue)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::SetNodeValue(const nsString& aNodeValue)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetNodeType(PRUint16* aNodeType)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetParentNode(nsIDOMNode** aParentNode)
{
    if (!mParent)
        return NS_ERROR_NOT_INITIALIZED;

    return mParent->QueryInterface(kIDOMNodeIID, (void**) aParentNode);
}


NS_IMETHODIMP
nsRDFElement::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    nsresult rv;
    if (!mChildren) {
        if (NS_FAILED(rv = GenerateChildren()))
            return rv;
    }

    RDFDOMNodeListImpl* list = new RDFDOMNodeListImpl(this);
    if (! list)
        return NS_ERROR_OUT_OF_MEMORY;

    return list->QueryInterface(kIDOMNodeListIID, (void**) aChildNodes);
}


NS_IMETHODIMP
nsRDFElement::GetFirstChild(nsIDOMNode** aFirstChild)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetLastChild(nsIDOMNode** aLastChild)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetNextSibling(nsIDOMNode** aNextSibling)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::HasChildNodes(PRBool* aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;

#if 0
    nsRDFElement* it = new nsRDFElement();
    if (! it)
        return NS_ERROR_OUT_OF_MEMORY;

    return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
#endif
}


////////////////////////////////////////////////////////////////////////
// nsIDOMElement interface

NS_IMETHODIMP
nsRDFElement::GetTagName(nsString& aTagName)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetDOMAttribute(const nsString& aName, nsString& aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::SetDOMAttribute(const nsString& aName, const nsString& aValue)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::RemoveAttribute(const nsString& aName)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetAttributeNode(const nsString& aName, nsIDOMAttr** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetElementsByTagName(const nsString& aName, nsIDOMNodeList** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::Normalize()
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}



////////////////////////////////////////////////////////////////////////
// nsIDOMEventReceiver interface

NS_IMETHODIMP
nsRDFElement::AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::GetListenerManager(nsIEventListenerManager** aInstancePtrResult)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}



////////////////////////////////////////////////////////////////////////
// nsIScriptObjectOwner interface

NS_IMETHODIMP 
nsRDFElement::GetScriptObject(nsIScriptContext* aContext, void** aScriptObject)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsRDFElement::SetScriptObject(void *aScriptObject)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// nsIJSScriptObject interface

PRBool
nsRDFElement::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
nsRDFElement::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
nsRDFElement::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
nsRDFElement::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
nsRDFElement::EnumerateProperty(JSContext *aContext)
{
    return PR_FALSE;
}


PRBool
nsRDFElement::Resolve(JSContext *aContext, jsval aID)
{
    return PR_FALSE;
}


PRBool
nsRDFElement::Convert(JSContext *aContext, jsval aID)
{
    return PR_FALSE;
}


void
nsRDFElement::Finalize(JSContext *aContext)
{
}



////////////////////////////////////////////////////////////////////////
// nsIConent interface

NS_IMETHODIMP
nsRDFElement::GetDocument(nsIDocument*& aResult) const
{
    nsIDocument* doc;
    nsresult rv = mDocument->QueryInterface(kIDocumentIID, (void**) &doc);
    aResult = doc; // implicit AddRef() from QI
    return rv;
}

NS_IMETHODIMP
nsRDFElement::SetDocument(nsIDocument* aDocument, PRBool aDeep)
{
    NS_IF_RELEASE(mDocument);

    nsresult rv;

    if (aDocument) {
        if (NS_FAILED(rv = aDocument->QueryInterface(kIRDFDocumentIID,
                                                     (void**) &mDocument)))
            return rv;
    }

    // implicit AddRef() from QI

    if (aDeep && mChildren) {
        for (PRInt32 i = mChildren->Count() - 1; i >= 0; --i) {
            // XXX this entire block could be more rigorous about
            // dealing with failure.
            nsISupports* obj = mChildren->ElementAt(i);

            PR_ASSERT(obj);
            if (! obj)
                continue;

            nsIContent* child;
            if (NS_SUCCEEDED(obj->QueryInterface(kIContentIID, (void**) &child))) {
                child->SetDocument(aDocument, aDeep);
                NS_RELEASE(child);
            }

            NS_RELEASE(obj);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::GetParent(nsIContent*& aResult) const
{
    if (!mParent)
        return NS_ERROR_NOT_INITIALIZED;

    aResult = mParent;
    NS_ADDREF(aResult);

    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::SetParent(nsIContent* aParent)
{
    // XXX don't allow modification of parents through this interface method?
    PR_ASSERT(! mParent);
    if (mParent)
        return NS_ERROR_ALREADY_INITIALIZED;

    PR_ASSERT(aParent);
    if (!aParent)
        return NS_ERROR_NULL_POINTER;

    mParent = aParent;
    NS_IF_ADDREF(mParent);

    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::CanContainChildren(PRBool& aResult) const
{
    aResult = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::ChildCount(PRInt32& aResult) const
{
    nsresult rv;
    if (!mChildren) {
        nsRDFElement* unconstThis = const_cast<nsRDFElement*>(this);
        if (NS_FAILED(rv = unconstThis->GenerateChildren()))
            return rv;
    }

    aResult = mChildren->Count();
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
    nsresult rv;
    if (!mChildren) {
        nsRDFElement* unconstThis = const_cast<nsRDFElement*>(this);
        if (NS_FAILED(rv = unconstThis->GenerateChildren()))
            return rv;
    }

    nsISupports* obj = mChildren->ElementAt(aIndex);
    nsIContent* content;
    rv = obj->QueryInterface(kIContentIID, (void**) &content);
    obj->Release();

    aResult = content;
    return rv;
}

NS_IMETHODIMP
nsRDFElement::IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify)
{
    PR_ASSERT(0); // this should be done via RDF
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsRDFElement::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify)
{
    PR_ASSERT(0); // this should be done via RDF
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsRDFElement::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
    PR_ASSERT(0); // this should be done via RDF
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsRDFElement::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
    PR_ASSERT(0); // this should be done via RDF
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsRDFElement::IsSynthetic(PRBool& aResult)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::GetTag(nsIAtom*& aResult) const
{
    PR_ASSERT(mResource);
    if (! mResource)
        return NS_ERROR_NOT_INITIALIZED;

    // XXX the problem with this is that we only have a
    // fully-qualified URI, not "just" the tag. And I think the style
    // system ain't gonna work right with that. So this is a complete
    // hack to parse what probably _was_ the tag out.
    nsresult rv;

    nsAutoString s;
    if (NS_FAILED(rv = mResource->GetStringValue(s)))
        return rv;

    PRInt32 index;

    if ((index = s.RFind('#')) >= 0)
        s.Cut(0, index + 1);
    else if ((index = s.RFind('/')) >= 0)
        s.Cut(0, index + 1);

    s.ToUpperCase(); // because that's how CSS works

    aResult = NS_NewAtom(s);
    return NS_OK;
}


NS_IMETHODIMP 
nsRDFElement::SetAttribute(const nsString& aName, 
                           const nsString& aValue,
                           PRBool aNotify)
{
    PR_ASSERT(0); // this should be done via RDF
    return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsRDFElement::GetAttribute(const nsString& aName, nsString& aResult) const
{
    nsresult rv;
    nsIRDFResourceManager* mgr = nsnull;
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFResourceManagerCID,
                                                    kIRDFResourceManagerIID,
                                                    (nsISupports**) &mgr)))
        return rv;
    
    nsIRDFDataBase* db    = nsnull;
    nsIRDFNode* property = nsnull;
    nsIRDFNode* value    = nsnull;

    if (NS_FAILED(rv = mDocument->GetDataBase(db)))
        goto done;

    if (NS_FAILED(rv = mgr->GetNode(aName, property)))
        goto done;

    // XXX Only returns the first value. yer screwed for
    // multi-attributes, I guess.

    if (NS_FAILED(rv = db->GetTarget(mResource, property, PR_TRUE, value)))
        goto done;

    rv = value->GetStringValue(aResult);

done:
    NS_IF_RELEASE(property);
    NS_IF_RELEASE(value);
    NS_IF_RELEASE(db);
    nsServiceManager::ReleaseService(kRDFResourceManagerCID, mgr);

    return rv;
}

NS_IMETHODIMP
nsRDFElement::UnsetAttribute(nsIAtom* aAttribute, PRBool aNotify)
{
    PR_ASSERT(0); // this should be done via RDF
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsRDFElement::GetAllAttributeNames(nsISupportsArray* aArray, PRInt32& aResult) const
{
    if (! aArray)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    nsIRDFResourceManager* mgr = nsnull;
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFResourceManagerCID,
                                                    kIRDFResourceManagerIID,
                                                    (nsISupports**) &mgr)))
        return rv;
    
    nsIRDFDataBase* db       = nsnull;
    nsIRDFCursor* properties = nsnull;
    PRBool moreProperties;

    if (NS_FAILED(rv = mDocument->GetDataBase(db)))
        goto done;

    if (NS_FAILED(rv = db->ArcLabelsOut(mResource, properties)))
        goto done;

    aArray->Clear(); // XXX or did you want me to append?
    aResult = 0;

    while (NS_SUCCEEDED(rv = properties->HasMoreElements(moreProperties)) && moreProperties) {
        nsIRDFNode* property = nsnull;
        PRBool tv;

        if (NS_FAILED(rv = properties->GetNext(property, tv /* ignored */)))
            break;

        nsAutoString uri;
        if (NS_SUCCEEDED(rv = property->GetStringValue(uri))) {
            nsIAtom* atom = NS_NewAtom(uri);
            if (atom) {
                aArray->AppendElement(atom);
                ++aResult;
            } else {
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
        }

        NS_RELEASE(property);

        if (NS_FAILED(rv))
            break;
    }

done:
    NS_IF_RELEASE(properties);
    NS_IF_RELEASE(db);
    nsServiceManager::ReleaseService(kRDFResourceManagerCID, mgr);

    return rv;
}

NS_IMETHODIMP
nsRDFElement::GetAttributeCount(PRInt32& aResult) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


static void
rdf_Indent(FILE* out, PRInt32 aIndent)
{
    for (PRInt32 i = aIndent; --i >= 0; ) fputs(" ", out);
}

NS_IMETHODIMP
nsRDFElement::List(FILE* out, PRInt32 aIndent) const
{
    if (! mResource)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    {
        nsIAtom* tag;
        if (NS_FAILED(rv = GetTag(tag)))
            return rv;

        rdf_Indent(out, aIndent);
        fputs("[RDF ", out);
        fputs(tag->GetUnicode(), out);
        fputs("\n", out);

        NS_RELEASE(tag);
    }

    {
        nsISupportsArray* attrs;
        PRInt32 nattrs;

        if (NS_FAILED(rv = NS_NewISupportsArray(&attrs)))
            return rv;

        if (NS_SUCCEEDED(rv = GetAllAttributeNames(attrs, nattrs))) {
            for (PRInt32 i = 0; i < nattrs; ++i) {
                nsIAtom* attr = (nsIAtom*) attrs->ElementAt(i);

                nsAutoString s;
                attr->ToString(s);
                NS_RELEASE(attr);

                nsAutoString v;
                GetAttribute(s, v);

                rdf_Indent(out, aIndent);
                fputs(" ", out);
                fputs(s, out);
                fputs("=", out);
                fputs(v, out);
                fputs("\n", out);
            }
        }

        NS_RELEASE(attrs);

        if (NS_FAILED(rv))
            return rv;
    }

    rdf_Indent(out, aIndent);
    fputs("]\n", out);

    {
        PRInt32 nchildren;
        if (NS_FAILED(rv = ChildCount(nchildren)))
            return rv;

        for (PRInt32 i = 0; i < nchildren; ++i) {
            nsIContent* child;
            if (NS_FAILED(rv = ChildAt(i, child)))
                return rv;

            rv = child->List(out, aIndent + 1);
            NS_RELEASE(child);

            if (NS_FAILED(rv))
                return rv;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::SizeOf(nsISizeOfHandler* aHandler) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP 
nsRDFElement::HandleDOMEvent(nsIPresContext& aPresContext,
                             nsEvent* aEvent,
                             nsIDOMEvent** aDOMEvent,
                             PRUint32 aFlags,
                             nsEventStatus& aEventStatus)
{
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIXMLContent

NS_IMETHODIMP 
nsRDFElement::SetNameSpace(nsIAtom* aNameSpace)
{
    NS_IF_RELEASE(mNameSpace);
    mNameSpace = aNameSpace;
    NS_IF_ADDREF(mNameSpace);
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFElement::GetNameSpace(nsIAtom*& aNameSpace)
{
    aNameSpace = mNameSpace;
    NS_IF_ADDREF(mNameSpace);
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::SetNameSpaceIdentifier(PRInt32 aNameSpaceId)
{
    mNameSpaceId = aNameSpaceId;
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFElement::GetNameSpaceIdentifier(PRInt32& aNameSpaceId)
{
    aNameSpaceId = mNameSpaceId;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIRDFContent

NS_IMETHODIMP
nsRDFElement::SetResource(const nsString& aURI)
{
    if (mResource)
        return NS_ERROR_ALREADY_INITIALIZED;

    nsresult rv;

    nsIRDFResourceManager* mgr = nsnull;
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFResourceManagerCID,
                                                    kIRDFResourceManagerIID,
                                                    (nsISupports**) &mgr)))
        return rv;
    
    rv = mgr->GetNode(aURI, mResource); // implicit AddRef()
    nsServiceManager::ReleaseService(kRDFResourceManagerCID, mgr);

    return rv;
}

NS_IMETHODIMP
nsRDFElement::GetResource(nsString& rURI) const
{
    if (! mResource)
        return NS_ERROR_NOT_INITIALIZED;

    return mResource->GetStringValue(rURI);
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::SetProperty(const nsString& aPropertyURI, const nsString& aValue)
{
    if (!mResource || !mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    nsString resource;
    mResource->GetStringValue(resource);

#ifdef DEBUG_waterson
    char buf[256];
    printf("assert [\n");
    printf("  %s\n", resource.ToCString(buf, sizeof(buf)));
    printf("  %s\n", aPropertyURI.ToCString(buf, sizeof(buf)));
    printf("  %s\n", aValue.ToCString(buf, sizeof(buf)));
    printf("]\n");
#endif

    nsresult rv;
    nsIRDFResourceManager* mgr = nsnull;

    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFResourceManagerCID,
                                                    kIRDFResourceManagerIID,
                                                    (nsISupports**) &mgr)))
        return rv;
    
    nsIRDFNode* property = nsnull;
    nsIRDFNode* value = nsnull;
    nsIRDFDataBase* db = nsnull;

    if (NS_FAILED(rv = mgr->GetNode(aPropertyURI, property)))
        goto done;

    if (NS_FAILED(rv = mgr->GetNode(aValue, value)))
        goto done;

    if (NS_FAILED(rv = mDocument->GetDataBase(db)))
        goto done;

    rv = db->Assert(mResource, property, value, PR_TRUE);

done:
    NS_IF_RELEASE(db);
    NS_IF_RELEASE(value);
    NS_IF_RELEASE(property);
    nsServiceManager::ReleaseService(kRDFResourceManagerCID, mgr);

    return rv;
}

NS_IMETHODIMP
nsRDFElement::GetProperty(const nsString& aPropertyURI, nsString& rValue) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// Implementation methods


// GenerateChildren
//
//   This is the money, baby. It's where we do the work of traversing
//   the RDF graph to produce new nsRDFElements and hook them up into
//   a semblance of a content model.
//
//   Ideally, this would be a "strategy", along with the logic for
//   manipulating the generated content. (Well, actually, ideally,
//   this would be hard coded to produce one kind of tree, and XSL
//   would do the work of transforming it for presentation.)
//
nsresult
nsRDFElement::GenerateChildren(void)
{
    nsresult rv;

    if (!mResource || !mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    if (! mChildren) {
        if (NS_FAILED(rv = NS_NewISupportsArray(&mChildren)))
            return rv;
    }
    else {
        mChildren->Clear();
    }

    nsIRDFResourceManager* mgr;
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFResourceManagerCID,
                                                    kIRDFResourceManagerIID,
                                                    (nsISupports**) &mgr)))
        return rv;

    nsIRDFDataBase* db = nsnull;
    nsIRDFCursor* properties = nsnull;
    PRBool moreProperties;

    if (NS_FAILED(rv = mDocument->GetDataBase(db)))
        goto done;

#ifdef ONLY_CREATE_RDF_CONTAINERS_AS_CONTENT
    if (! rdf_IsContainer(mgr, db, mResource))
        goto done;
#endif

    // Create a cursor that'll enumerate all of the outbound arcs
    if (NS_FAILED(rv = db->ArcLabelsOut(mResource, properties)))
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
        if (NS_FAILED(rv = db->GetTargets(mResource, property, PR_TRUE, values))) {
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
            mChildren->AppendElement(child);

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
    NS_IF_RELEASE(db);
    nsServiceManager::ReleaseService(kRDFResourceManagerCID, mgr);

    return rv;
}



nsresult
nsRDFElement::CreateChild(nsIRDFNode* value,
                          nsIRDFContent*& result)
{
    // XXX I wish that we could avoid doing it "by hand" like this
    // (i.e., use interface methods so that we could extend to other
    // kinds of kids later); however, there is a cascade of const-ness
    // out to things like ChildCount() that is tough to get around.

    nsRDFElement* child = new nsRDFElement();
    if (! child)
        return NS_ERROR_OUT_OF_MEMORY;

    child->mDocument = mDocument;
    NS_ADDREF(child->mDocument);

    child->mResource = value;
    NS_ADDREF(child->mResource);

    child->mParent   = NS_STATIC_CAST(nsIContent*, const_cast<nsRDFElement*>(this));
    NS_ADDREF(child->mParent);

    result = child;
    NS_ADDREF(result);

    return NS_OK;
}


nsresult
nsRDFElement::CreateChild(nsIRDFNode* property,
                          nsIRDFNode* value,
                          nsIRDFContent*& result)
{
    nsresult rv;
    nsRDFElement* child = nsnull;
    nsRDFElement* grandchild = nsnull;
    nsIContent* grandchild2 = nsnull;
    nsITextContent* text = nsnull;
    nsIDocument* doc = nsnull;
    nsAutoString v;

    child = new nsRDFElement();
    if (! child) {
        return NS_ERROR_OUT_OF_MEMORY;
        goto error;
    }

    NS_ADDREF(child);
    child->mDocument = mDocument;
    child->mResource = property;
    child->mParent   = NS_STATIC_CAST(nsIContent*, const_cast<nsRDFElement*>(this));

    NS_ADDREF(child->mDocument);
    NS_ADDREF(child->mResource);
    NS_ADDREF(child->mParent);

    if (NS_FAILED(rv = NS_NewISupportsArray(&child->mChildren)))
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

        if (NS_FAILED(rv = mDocument->QueryInterface(kIDocumentIID, (void**) &doc)))
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

        child->mChildren->AppendElement(NS_STATIC_CAST(nsIContent*, grandchild2));
    }

    // Construct a grandchild which is another RDF node.
    grandchild = new nsRDFElement();
    if (! grandchild) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto error;
    }

    grandchild->mDocument = mDocument;
    grandchild->mResource = value;
    grandchild->mParent   = NS_STATIC_CAST(nsIContent*, const_cast<nsRDFElement*>(child));

    NS_ADDREF(grandchild->mDocument);
    NS_ADDREF(grandchild->mResource);
    NS_ADDREF(grandchild->mParent);

    child->mChildren->AppendElement(NS_STATIC_CAST(nsIContent*, grandchild));

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


