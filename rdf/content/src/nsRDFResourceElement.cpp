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

  Implementation for a "pseudo content element" that acts as a proxy
  to the RDF graph.

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


// #define the following if you want properties to show up as
// attributes on an element. I know, this sucks, but I'm just not
// really sure if this is necessary...
//#define CREATE_PROPERTIES_AS_ATTRIBUTES

#include "nsCOMPtr.h"
#include "nsDOMEvent.h"
#include "nsGenericAttribute.h"
#include "nsHashtable.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIEventStateManager.h"
#include "nsIJSScriptObject.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContent.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIScriptObjectOwner.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFContentUtils.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIContentIID,                NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMElementIID,             NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID,       NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMNodeIID,                NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMNodeListIID,            NS_IDOMNODELIST_IID);
static NS_DEFINE_IID(kIEventListenerManagerIID,   NS_IEVENTLISTENERMANAGER_IID);
static NS_DEFINE_IID(kIDocumentIID,               NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID,         NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,       NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID,        NS_IPRIVATEDOMEVENT_IID);
static NS_DEFINE_IID(kIRDFContentIID,             NS_IRDFCONTENT_IID);
static NS_DEFINE_IID(kIRDFCompositeDataSourceIID, NS_IRDFCOMPOSITEDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFDocumentIID,            NS_IRDFDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFServiceIID,             NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID,      NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID,               NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXMLContentIID,             NS_IXMLCONTENT_IID);

static NS_DEFINE_CID(kEventListenerManagerCID, NS_EVENTLISTENERMANAGER_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,     NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID,           NS_RDFSERVICE_CID);


////////////////////////////////////////////////////////////////////////

class RDFResourceElementImpl : public nsIDOMElement,
                               public nsIDOMEventReceiver,
                               public nsIScriptObjectOwner,
                               public nsIJSScriptObject,
                               public nsIRDFContent
{
public:
    RDFResourceElementImpl(nsIRDFResource* aResource,
                           PRInt32 aNameSpaceID,
                           nsIAtom* aTag);

    ~RDFResourceElementImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS
       
    // nsIDOMNode (from nsIDOMElement)
    NS_DECL_IDOMNODE
  
    // nsIDOMElement
    NS_DECL_IDOMELEMENT

    // nsIScriptObjectOwner
    NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
    NS_IMETHOD SetScriptObject(void *aScriptObject);

    // nsIContent (from nsIRDFContent via nsIXMLContent)

    // Any of the nsIContent methods that directly manipulate content
    // (e.g., AppendChildTo()), are assumed to "know what they're
    // doing" to the content model. No attempt is made to muck with
    // the underlying RDF representation.
    NS_IMETHOD GetDocument(nsIDocument*& aResult) const;
    NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep);
    NS_IMETHOD GetParent(nsIContent*& aResult) const;
    NS_IMETHOD SetParent(nsIContent* aParent);
    NS_IMETHOD CanContainChildren(PRBool& aResult) const;
    NS_IMETHOD ChildCount(PRInt32& aResult) const;
    NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
    NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const;
    NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
    NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
    NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify);
    NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);
    NS_IMETHOD IsSynthetic(PRBool& aResult);
    NS_IMETHOD GetNameSpaceID(PRInt32& aNameSpeceID) const;
    NS_IMETHOD GetTag(nsIAtom*& aResult) const;
    NS_IMETHOD ParseAttributeString(const nsString& aStr, nsIAtom*& aName, PRInt32& aNameSpaceID);
    NS_IMETHOD GetNameSpacePrefix(PRInt32 aNameSpaceID, nsIAtom*& aPrefix);
    NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, const nsString& aValue, PRBool aNotify);
    NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, nsString& aResult) const;
    NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify);
    NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex, PRInt32& aNameSpaceID, 
                                  nsIAtom*& aName) const;
    NS_IMETHOD GetAttributeCount(PRInt32& aResult) const;
    NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
    NS_IMETHOD BeginConvertToXIF(nsXIFConverter& aConverter) const;
    NS_IMETHOD ConvertContentToXIF(nsXIFConverter& aConverter) const;
    NS_IMETHOD FinishConvertToXIF(nsXIFConverter& aConverter) const;
    NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;
    NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus& aEventStatus);
    NS_IMETHOD RangeAdd(nsIDOMRange& aRange);
    NS_IMETHOD RangeRemove(nsIDOMRange& aRange); 
    NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const;

    // nsIXMLContent (from nsIRDFContent)
    //NS_IMETHOD SetContainingNameSpace(nsINameSpace* aNameSpace);
    //NS_IMETHOD GetContainingNameSpace(nsINameSpace*& aNameSpace) const;
    //NS_IMETHOD SetNameSpacePrefix(nsIAtom* aNameSpace);
    //NS_IMETHOD GetNameSpacePrefix(nsIAtom*& aNameSpace) const;
    //NS_IMETHOD SetNameSpaceID(PRInt32 aNameSpaceID);

    // nsIRDFContent
    NS_IMETHOD GetResource(nsIRDFResource*& aResource) const;
    NS_IMETHOD SetContainer(PRBool aIsContainer);

    // nsIDOMEventReceiver
    NS_IMETHOD AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
    NS_IMETHOD GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult);


    // nsIJSScriptObject
    virtual PRBool AddProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool GetProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool SetProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool EnumerateProperty(JSContext *aContext);
    virtual PRBool Resolve(JSContext *aContext, jsval aID);
    virtual PRBool Convert(JSContext *aContext, jsval aID);
    virtual void   Finalize(JSContext *aContext);

protected:
    /** The document in which the element lives. */
    nsIDocument*      mDocument;

    /** XXX */
    void*             mScriptObject;

    /** An array of child nodes */
    nsISupportsArray* mChildren;

    /** The element's parent. NOT refcounted. */
    nsIContent*       mParent;

    /** The element's namespace */
    PRInt32           mNameSpaceID;

    /** The element's tag */
    nsIAtom*          mTag;

    nsIEventListenerManager* mListenerManager;

    /** An array of attribute data. Instantiated
        lazily if attributes are required */
    nsVoidArray*      mAttributes;

    /** The RDF resource that the element corresponds to */
    nsIRDFResource*   mResource;

    PRBool mContentsMustBeGenerated;

    /**
     * Dynamically generate the element's children from the RDF graph
     */
    nsresult EnsureContentsGenerated(void) const;
};

////////////////////////////////////////////////////////////////////////
// RDFResourceElementImpl

static PRInt32 kNameSpaceID_RDF;
static nsIAtom* kIdAtom;

RDFResourceElementImpl::RDFResourceElementImpl(nsIRDFResource* aResource,
                                               PRInt32 aNameSpaceID,
                                               nsIAtom* aTag)
    : mDocument(nsnull),
      mScriptObject(nsnull),
      mChildren(nsnull),
      mParent(nsnull),
      mNameSpaceID(aNameSpaceID),
      mTag(aTag),
      mListenerManager(nsnull),
      mAttributes(nsnull),
      mResource(aResource),
      mContentsMustBeGenerated(PR_FALSE)
{
    NS_INIT_REFCNT();
    NS_ADDREF(aResource);
    NS_ADDREF(aTag);

    if (nsnull == kIdAtom) {
        kIdAtom = NS_NewAtom("id");

        nsresult rv;
        nsINameSpaceManager* mgr;
        if (NS_SUCCEEDED(rv = nsRepository::CreateInstance(kNameSpaceManagerCID,
                                                           nsnull,
                                                           kINameSpaceManagerIID,
                                                           (void**) &mgr))) {
static const char kRDFNameSpaceURI[]
    = RDF_NAMESPACE_URI;

            rv = mgr->RegisterNameSpace(kRDFNameSpaceURI, kNameSpaceID_RDF);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register RDF namespace");

            NS_RELEASE(mgr);
        }
        else {
            NS_ERROR("couldn't create namepsace manager");
        }
        
    }
    else {
        NS_ADDREF(kIdAtom);
    }
}

RDFResourceElementImpl::~RDFResourceElementImpl()
{
    if (nsnull != mAttributes) {
        PRInt32 count = mAttributes->Count();
        PRInt32 index;
        for (index = 0; index < count; index++) {
            nsGenericAttribute* attr = (nsGenericAttribute*)mAttributes->ElementAt(index);
            delete attr;
        }
        delete mAttributes;
    }

    //NS_IF_RELEASE(mDocument); // not refcounted
    //NS_IF_RELEASE(mParent)    // not refcounted
    NS_IF_RELEASE(mTag);
    NS_IF_RELEASE(mListenerManager);
    NS_IF_RELEASE(mChildren);
    NS_RELEASE(mResource);

    nsrefcnt refcnt;
    NS_RELEASE2(kIdAtom, refcnt);
}


nsresult
NS_NewRDFResourceElement(nsIRDFContent** aResult,
                         nsIRDFResource* aResource,
                         PRInt32 aNameSpaceId,
                         nsIAtom* aTag)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    RDFResourceElementImpl* element =
        new RDFResourceElementImpl(aResource, aNameSpaceId, aTag);

    if (! element)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(element);
    *aResult = element;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(RDFResourceElementImpl);
NS_IMPL_RELEASE(RDFResourceElementImpl);

NS_IMETHODIMP 
RDFResourceElementImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIRDFContentIID) ||
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
    NS_ADDREF(this);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIDOMNode interface

NS_IMETHODIMP
RDFResourceElementImpl::GetNodeName(nsString& aNodeName)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::GetNodeValue(nsString& aNodeValue)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFResourceElementImpl::SetNodeValue(const nsString& aNodeValue)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::GetNodeType(PRUint16* aNodeType)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::GetParentNode(nsIDOMNode** aParentNode)
{
    if (!mParent)
        return NS_ERROR_NOT_INITIALIZED;

    return mParent->QueryInterface(kIDOMNodeIID, (void**) aParentNode);
}


NS_IMETHODIMP
RDFResourceElementImpl::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    return NS_NewRDFDOMNodeList(aChildNodes, this);
}


NS_IMETHODIMP
RDFResourceElementImpl::GetFirstChild(nsIDOMNode** aFirstChild)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::GetLastChild(nsIDOMNode** aLastChild)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::GetNextSibling(nsIDOMNode** aNextSibling)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::HasChildNodes(PRBool* aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;

#if 0
    RDFResourceElementImpl* it = new RDFResourceElementImpl();
    if (! it)
        return NS_ERROR_OUT_OF_MEMORY;

    return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
#endif
}


////////////////////////////////////////////////////////////////////////
// nsIDOMElement interface

NS_IMETHODIMP
RDFResourceElementImpl::GetTagName(nsString& aTagName)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP 
RDFResourceElementImpl::ParseAttributeString(const nsString& aStr, 
                                             nsIAtom*& aName, 
                                             PRInt32& aNameSpaceID)
{
    // XXX Need to implement
    aName = nsnull;
    aNameSpaceID = kNameSpaceID_None;
    return NS_OK;
}

NS_IMETHODIMP
RDFResourceElementImpl::GetNameSpacePrefix(PRInt32 aNameSpaceID, 
                                           nsIAtom*& aPrefix)
{
    // XXX Need to implement
    aPrefix = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
RDFResourceElementImpl::GetAttribute(const nsString& aName, nsString& aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::SetAttribute(const nsString& aName, const nsString& aValue)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::RemoveAttribute(const nsString& aName)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::GetAttributeNode(const nsString& aName, nsIDOMAttr** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::GetElementsByTagName(const nsString& aName, nsIDOMNodeList** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFResourceElementImpl::Normalize()
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}



////////////////////////////////////////////////////////////////////////
// nsIDOMEventReceiver interface

NS_IMETHODIMP
RDFResourceElementImpl::AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    nsIEventListenerManager *manager;

    if (NS_OK == GetListenerManager(&manager)) {
        manager->AddEventListener(aListener, aIID);
        NS_RELEASE(manager);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
RDFResourceElementImpl::RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    if (nsnull != mListenerManager) {
        mListenerManager->RemoveEventListener(aListener, aIID);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
RDFResourceElementImpl::GetListenerManager(nsIEventListenerManager** aResult)
{
    if (nsnull != mListenerManager) {
        NS_ADDREF(mListenerManager);
        *aResult = mListenerManager;
        return NS_OK;
    }
    nsresult rv = nsRepository::CreateInstance(kEventListenerManagerCID,
                                               nsnull,
                                               kIEventListenerManagerIID,
                                               (void**) aResult);
    if (NS_OK == rv) {
        mListenerManager = *aResult;
        NS_ADDREF(mListenerManager);
    }
    return rv;
}

NS_IMETHODIMP
RDFResourceElementImpl::GetNewListenerManager(nsIEventListenerManager **aResult)
{
    return nsRepository::CreateInstance(kEventListenerManagerCID,
                                        nsnull,
                                        kIEventListenerManagerIID,
                                        (void**) aResult);
}



////////////////////////////////////////////////////////////////////////
// nsIScriptObjectOwner interface

NS_IMETHODIMP 
RDFResourceElementImpl::GetScriptObject(nsIScriptContext* aContext, void** aScriptObject)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
RDFResourceElementImpl::SetScriptObject(void *aScriptObject)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// nsIJSScriptObject interface

PRBool
RDFResourceElementImpl::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
RDFResourceElementImpl::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
RDFResourceElementImpl::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
RDFResourceElementImpl::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
RDFResourceElementImpl::EnumerateProperty(JSContext *aContext)
{
    return PR_FALSE;
}


PRBool
RDFResourceElementImpl::Resolve(JSContext *aContext, jsval aID)
{
    return PR_FALSE;
}


PRBool
RDFResourceElementImpl::Convert(JSContext *aContext, jsval aID)
{
    return PR_FALSE;
}


void
RDFResourceElementImpl::Finalize(JSContext *aContext)
{
}


////////////////////////////////////////////////////////////////////////
// nsIContent interface
//
//   Just to say this again (I said it in the header file), none of
//   the manipulators for nsIContent will do anything to the RDF
//   graph. These are assumed to be used only by the content model
//   constructor, who is presumed to be _using_ the RDF graph to
//   construct this content model.
//
//   You have been warned.
//

NS_IMETHODIMP
RDFResourceElementImpl::GetDocument(nsIDocument*& aResult) const
{
    aResult = mDocument;
    NS_IF_ADDREF(aResult);
    return NS_OK;
}

NS_IMETHODIMP
RDFResourceElementImpl::SetDocument(nsIDocument* aDocument, PRBool aDeep)
{
    nsresult rv;
    nsCOMPtr<nsIRDFDocument> rdfDoc;

    if (mDocument) {
        if (NS_SUCCEEDED(mDocument->QueryInterface(kIRDFDocumentIID, getter_AddRefs(rdfDoc)))) {
            rv = rdfDoc->RemoveElementForResource(mResource, this);
            NS_ASSERTION(NS_SUCCEEDED(rv), "error unmapping resource from element");
        }
    }
    mDocument = aDocument; // not refcounted
    if (mDocument) {
        if (NS_SUCCEEDED(mDocument->QueryInterface(kIRDFDocumentIID, getter_AddRefs(rdfDoc)))) {
            rv = rdfDoc->AddElementForResource(mResource, this);
            NS_ASSERTION(NS_SUCCEEDED(rv), "error mapping resource to element");
        }
    }

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
RDFResourceElementImpl::GetParent(nsIContent*& aResult) const
{
    aResult = mParent;
    NS_IF_ADDREF(mParent);
    return NS_OK;
}

NS_IMETHODIMP
RDFResourceElementImpl::SetParent(nsIContent* aParent)
{
    mParent = aParent; // no refcount
    return NS_OK;
}

NS_IMETHODIMP
RDFResourceElementImpl::CanContainChildren(PRBool& aResult) const
{
    // XXX Hmm -- not sure if this is unilaterally true...
    aResult = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
RDFResourceElementImpl::ChildCount(PRInt32& aResult) const
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    aResult = mChildren ? mChildren->Count() : 0;
    return NS_OK;
}

NS_IMETHODIMP
RDFResourceElementImpl::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    if (! mChildren) {
        aResult = nsnull;
        return NS_OK;
    }

    // XXX The ultraparanoid way to do this...
    //nsISupports* obj = mChildren->ElementAt(aIndex);
    //nsIContent* content;
    //nsresult rv = obj->QueryInterface(kIContentIID, (void**) &content);
    //obj->Release();
    //aResult = content;

    // But, since we're in a closed system, we can just do the following...
    aResult = (nsIContent*) mChildren->ElementAt(aIndex);
    return NS_OK;
}

NS_IMETHODIMP
RDFResourceElementImpl::IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    aResult = (mChildren) ? (mChildren->IndexOf(aPossibleChild)) : (-1);
    return NS_OK;
}

NS_IMETHODIMP
RDFResourceElementImpl::InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify)
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    NS_PRECONDITION(nsnull != aKid, "null ptr");

    if (! mChildren) {
        if (NS_FAILED(NS_NewISupportsArray(&mChildren)))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    PRBool insertOk = mChildren->InsertElementAt(aKid, aIndex);/* XXX fix up void array api to use nsresult's*/
    if (insertOk) {
        NS_ADDREF(aKid);
        aKid->SetParent(this);
        //nsRange::OwnerChildInserted(this, aIndex);
        nsIDocument* doc = mDocument;
        if (nsnull != doc) {
            aKid->SetDocument(doc, PR_FALSE);
            if (aNotify) {
                doc->ContentInserted(this, aKid, aIndex);
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFResourceElementImpl::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify)
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    NS_PRECONDITION(nsnull != mChildren, "illegal value");
    if (! mChildren)
        return NS_ERROR_ILLEGAL_VALUE;

    NS_PRECONDITION(nsnull != aKid, "null ptr");
    if (! aKid)
        return NS_ERROR_NULL_POINTER;

    nsIContent* oldKid = (nsIContent *)mChildren->ElementAt(aIndex);
    PRBool replaceOk = mChildren->ReplaceElementAt(aKid, aIndex);
    if (replaceOk) {
        NS_ADDREF(aKid);
        aKid->SetParent(this);
        //nsRange::OwnerChildReplaced(this, aIndex, oldKid);
        nsIDocument* doc = mDocument;
        if (nsnull != doc) {
            aKid->SetDocument(doc, PR_FALSE);
            if (aNotify) {
                doc->ContentReplaced(this, oldKid, aKid, aIndex);
            }
        }
        oldKid->SetDocument(nsnull, PR_TRUE);
        oldKid->SetParent(nsnull);
        NS_RELEASE(oldKid);
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFResourceElementImpl::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    NS_PRECONDITION((nsnull != aKid) && (aKid != this), "null ptr");

    if (! mChildren) {
        if (NS_FAILED(NS_NewISupportsArray(&mChildren)))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    PRBool appendOk = mChildren->AppendElement(aKid);
    if (appendOk) {
        NS_ADDREF(aKid);
        aKid->SetParent(this);
        // ranges don't need adjustment since new child is at end of list
        nsIDocument* doc = mDocument;
        if (nsnull != doc) {
            aKid->SetDocument(doc, PR_FALSE);
            if (aNotify) {
                doc->ContentAppended(this, mChildren->Count() - 1);
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFResourceElementImpl::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    NS_PRECONDITION(mChildren != nsnull, "illegal value");
    if (! mChildren)
        return NS_ERROR_ILLEGAL_VALUE;

    nsIContent* oldKid = (nsIContent *)mChildren->ElementAt(aIndex);
    if (nsnull != oldKid ) {
        nsIDocument* doc = mDocument;
        PRBool rv = mChildren->RemoveElementAt(aIndex);
        //nsRange::OwnerChildRemoved(this, aIndex, oldKid);
        if (aNotify) {
            if (nsnull != doc) {
                doc->ContentRemoved(this, oldKid, aIndex);
            }
        }
        oldKid->SetDocument(nsnull, PR_TRUE);
        oldKid->SetParent(nsnull);
        NS_RELEASE(oldKid);
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFResourceElementImpl::IsSynthetic(PRBool& aResult)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
RDFResourceElementImpl::GetNameSpaceID(PRInt32& aNameSpaceID) const
{
    aNameSpaceID = mNameSpaceID;
    return NS_OK;
}

NS_IMETHODIMP
RDFResourceElementImpl::GetTag(nsIAtom*& aResult) const
{
    aResult = mTag;
    NS_ADDREF(aResult);
    return NS_OK;
}


// XXX attribute code swiped from nsGenericContainerElement
// this class could probably just use nsGenericContainerElement
// needed to maintain attribute namespace ID as well as ordering
NS_IMETHODIMP 
RDFResourceElementImpl::SetAttribute(PRInt32 aNameSpaceID,
                                   nsIAtom* aName, 
                                   const nsString& aValue,
                                   PRBool aNotify)
{
    NS_ASSERTION(kNameSpaceID_Unknown != aNameSpaceID, "must have name space ID");
    if (kNameSpaceID_Unknown == aNameSpaceID) {
        return NS_ERROR_ILLEGAL_VALUE;
    }
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    if (nsnull == aName) {
        return NS_ERROR_NULL_POINTER;
    }

    if (nsnull == mAttributes) {
        if ((mAttributes = new nsVoidArray()) == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv;
    nsGenericAttribute* attr;
    PRInt32 index;
    PRInt32 count = mAttributes->Count();
    for (index = 0; index < count; index++) {
        attr = (nsGenericAttribute*)mAttributes->ElementAt(index);
        if ((aNameSpaceID == attr->mNameSpaceID) && (aName == attr->mName)) {
            attr->mValue = aValue;
            rv = NS_OK;
            break;
        }
    }
    
    if (index >= count) { // didn't find it
        attr = new nsGenericAttribute(aNameSpaceID, aName, aValue);
        if (nsnull != attr) {
            mAttributes->AppendElement(attr);
            rv = NS_OK;
        }
    }

    // XXX notify doc?
    return rv;
}


NS_IMETHODIMP
RDFResourceElementImpl::GetAttribute(PRInt32 aNameSpaceID,
                                     nsIAtom* aName,
                                     nsString& aResult) const
{
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    if (nsnull == aName) {
        return NS_ERROR_NULL_POINTER;
    }

    nsresult rv = NS_CONTENT_ATTR_NOT_THERE;

#if defined(CREATE_PROPERTIES_AS_ATTRIBUTES)
    // XXX I'm not sure if we should support properties as attributes
    // or not...
    nsIRDFService* mgr = nsnull;
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &mgr)))
        return rv;
    
    nsIRDFCompositeDataSource* db = nsnull;
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
    nsServiceManager::ReleaseService(kRDFServiceCID, mgr);

#endif // defined(CREATE_PROPERTIES_AS_ATTRIBUTES)

    // Simulate the RDF:ID attribute to be the resource's URI
    if (aNameSpaceID == kNameSpaceID_RDF && aName == kIdAtom) {
        const char* uri;
        if (NS_FAILED(rv = mResource->GetValue(&uri)))
            return rv;

        aResult = uri;
        return NS_OK;
    }

    if (nsnull != mAttributes) {
        PRInt32 count = mAttributes->Count();
        PRInt32 index;
        for (index = 0; index < count; index++) {
            const nsGenericAttribute* attr = (const nsGenericAttribute*)mAttributes->ElementAt(index);
            if ((attr->mNameSpaceID == aNameSpaceID) && (attr->mName == aName)) {
                aResult = attr->mValue;
                if (0 < aResult.Length()) {
                    rv = NS_CONTENT_ATTR_HAS_VALUE;
                }
                else {
                    rv = NS_CONTENT_ATTR_NO_VALUE;
                }
                break;
            }
        }
    }
    return rv;
}

NS_IMETHODIMP
RDFResourceElementImpl::UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify)
{
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    if (nsnull == aName) {
        return NS_ERROR_NULL_POINTER;
    }

    nsresult rv = NS_OK;

    if (nsnull != mAttributes) {
        PRInt32 count = mAttributes->Count();
        PRInt32 index;
        for (index = 0; index < count; index++) {
            nsGenericAttribute* attr = (nsGenericAttribute*)mAttributes->ElementAt(index);
            if ((attr->mNameSpaceID == aNameSpaceID) && (attr->mName == aName)) {
                mAttributes->RemoveElementAt(index);
                delete attr;
                break;
            }
        }

        // XXX notify document??
    }

    return rv;
}

NS_IMETHODIMP
RDFResourceElementImpl::GetAttributeNameAt(PRInt32 aIndex,
                                         PRInt32& aNameSpaceID,
                                         nsIAtom*& aName) const
{
#if defined(CREATE_PROPERTIES_AS_ATTRIBUTES)
    // XXX I'm not sure if we should support attributes or not...

    nsIRDFService* mgr = nsnull;
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &mgr)))
        return rv;
    
    nsIRDFCompositeDataSource* db = nsnull;
    nsIRDFCursor* properties = nsnull;
    PRBool moreProperties;

    if (NS_FAILED(rv = mDocument->GetDataBase(db)))
        goto done;

    if (NS_FAILED(rv = db->ArcLabelsOut(mResource, properties)))
        goto done;

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
    nsServiceManager::ReleaseService(kRDFServiceCID, mgr);
    return rv;
#endif // defined(CREATE_PROPERTIES_AS_ATTRIBUTES)

    if (aIndex == 0) {
        // The implicit RDF:ID property
        aNameSpaceID = kNameSpaceID_RDF;
        aName        = kIdAtom;
        NS_IF_ADDREF(aName);
        return NS_OK;
    }

    --aIndex;
    if (nsnull != mAttributes) {
        nsGenericAttribute* attr = (nsGenericAttribute*)mAttributes->ElementAt(aIndex);
        if (nsnull != attr) {
            aNameSpaceID = attr->mNameSpaceID;
            aName        = attr->mName;
            NS_IF_ADDREF(aName);
            return NS_OK;
        }
    }
    aNameSpaceID = kNameSpaceID_None;
    aName = nsnull;
    return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
RDFResourceElementImpl::GetAttributeCount(PRInt32& aResult) const
{
#if defined(CREATE_PROPERTIES_AS_ATTRIBUTES)
    PR_ASSERT(0);     // XXX need to write this...
#endif // defined(CREATE_PROPERTIES_AS_ATTRIBUTES)

    nsresult rv = NS_OK;
    if (nsnull != mAttributes) {
      aResult = mAttributes->Count();
    }
    else {
      aResult = 0;
    }

    ++aResult; // For the implicit RDF:ID property
    return rv;
}


static void
rdf_Indent(FILE* out, PRInt32 aIndent)
{
    for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
}

NS_IMETHODIMP
RDFResourceElementImpl::List(FILE* out, PRInt32 aIndent) const
{
    NS_PRECONDITION(mDocument != nsnull, "bad content");

    nsresult rv;
    {
        nsIAtom* tag;
        if (NS_FAILED(rv = GetTag(tag)))
            return rv;

        rdf_Indent(out, aIndent);
        fputs("[RDF ", out);
        fputs(tag->GetUnicode(), out);

        NS_RELEASE(tag);
    }

    {
        PRInt32 nattrs;

        if (NS_SUCCEEDED(rv = GetAttributeCount(nattrs))) {
            for (PRInt32 i = 0; i < nattrs; ++i) {
                nsIAtom* attr = nsnull;
                PRInt32 nameSpaceID;
                GetAttributeNameAt(i, nameSpaceID, attr);


                nsAutoString v;
                GetAttribute(nameSpaceID, attr, v);

                nsAutoString s;
                attr->ToString(s);
                NS_RELEASE(attr);

                fputs(" ", out);
                fputs(s, out);
                fputs("=", out);
                fputs(v, out);
            }
        }

        if (NS_FAILED(rv))
            return rv;
    }

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
RDFResourceElementImpl::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFResourceElementImpl::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFResourceElementImpl::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFResourceElementImpl::SizeOf(nsISizeOfHandler* aHandler) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP 
RDFResourceElementImpl::HandleDOMEvent(nsIPresContext& aPresContext,
                                       nsEvent* aEvent,
                                       nsIDOMEvent** aDOMEvent,
                                       PRUint32 aFlags,
                                       nsEventStatus& aEventStatus)
{
    nsresult ret = NS_OK;
  
    nsIDOMEvent* domEvent = nsnull;
    if (DOM_EVENT_INIT == aFlags) {
        aDOMEvent = &domEvent;
    }
  
    //Capturing stage
  
    //Local handling stage
    if (nsnull != mListenerManager) {
        mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aEventStatus);
    }

    //Bubbling stage
    if ((DOM_EVENT_CAPTURE != aFlags) && (mParent != nsnull)) {
        ret = mParent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                      DOM_EVENT_BUBBLE, aEventStatus);
    }

    if (DOM_EVENT_INIT == aFlags) {
        // We're leaving the DOM event loop so if we created a DOM event,
        // release here.
        if (nsnull != *aDOMEvent) {
            nsrefcnt rc;
            NS_RELEASE2(*aDOMEvent, rc);
            if (0 != rc) {
                // Okay, so someone in the DOM loop (a listener, JS object)
                // still has a ref to the DOM Event but the internal data
                // hasn't been malloc'd.  Force a copy of the data here so the
                // DOM Event is still valid.
                nsIPrivateDOMEvent *privateEvent;
                if (NS_OK == (*aDOMEvent)->QueryInterface(kIPrivateDOMEventIID, (void**)&privateEvent)) {
                    privateEvent->DuplicatePrivateData();
                    NS_RELEASE(privateEvent);
                }
            }
        }
        aDOMEvent = nsnull;
    }
    return ret;
}



NS_IMETHODIMP 
RDFResourceElementImpl::RangeAdd(nsIDOMRange& aRange) 
{  
    // rdf content does not yet support DOM ranges
    return NS_OK;
}

 
NS_IMETHODIMP 
RDFResourceElementImpl::RangeRemove(nsIDOMRange& aRange) 
{
    // rdf content does not yet support DOM ranges
    return NS_OK;
}                                                                        


NS_IMETHODIMP 
RDFResourceElementImpl::GetRangeList(nsVoidArray*& aResult) const
{
    // rdf content does not yet support DOM ranges
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIRDFContent

NS_IMETHODIMP
RDFResourceElementImpl::GetResource(nsIRDFResource*& aResource) const
{
    if (! mResource)
        return NS_ERROR_NOT_INITIALIZED;

    aResource = mResource;
    NS_ADDREF(aResource);
    return NS_OK;
}


NS_IMETHODIMP
RDFResourceElementImpl::SetContainer(PRBool aIsContainer)
{
    // If this item is a container, then we'll need to remember to
    // dynamically generate the contents of the container when asked.
    if (aIsContainer)
        mContentsMustBeGenerated = PR_TRUE;
    else
        mContentsMustBeGenerated = PR_FALSE;

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Implementation methods


nsresult
RDFResourceElementImpl::EnsureContentsGenerated(void) const
{
    if (! mContentsMustBeGenerated)
        return NS_OK;

    nsresult rv;

    NS_PRECONDITION(mResource != nsnull, "not initialized");
    if (!mResource)
        return NS_ERROR_NOT_INITIALIZED;

    NS_PRECONDITION(mDocument != nsnull, "not initialized");
    if (!mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    // XXX hack because we can't use "mutable"
    RDFResourceElementImpl* unconstThis = NS_CONST_CAST(RDFResourceElementImpl*, this);

    if (! unconstThis->mChildren) {
        if (NS_FAILED(rv = NS_NewISupportsArray(&unconstThis->mChildren)))
            return rv;
    }

    // Clear this value *first*, so we can re-enter the nsIContent
    // getters if needed.
    unconstThis->mContentsMustBeGenerated = PR_FALSE;

    nsCOMPtr<nsIRDFDocument> rdfDoc;
    if (NS_FAILED(rv = mDocument->QueryInterface(kIRDFDocumentIID,
                                                 (void**) getter_AddRefs(rdfDoc))))
        return rv;

    rv = rdfDoc->CreateContents(unconstThis);
    NS_ASSERTION(NS_SUCCEEDED(rv), "problem creating kids");
    return rv;
}


