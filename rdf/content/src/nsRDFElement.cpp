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

  TO DO

  1) Convert mBroadcastListeners to a _pointer_ to an nsVoidArray,
     instead of an automatic nsVoidArray. This should reduce the
     footprint per element by 40-50 bytes.

 */


// #define the following if you want properties to show up as
// attributes on an element. I know, this sucks, but I'm just not
// really sure if this is necessary...
//#define CREATE_PROPERTIES_AS_ATTRIBUTES

#include "nsCOMPtr.h"
#include "nsDOMCID.h"
#include "nsDOMEvent.h"
#include "nsXULAttributes.h"
#include "nsHashtable.h"
#include "nsIPresShell.h"
#include "nsIAtom.h"
#include "nsIXMLContent.h"
#include "nsIDOMAttr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMElementObserver.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNodeObserver.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDOMXULElement.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIEventStateManager.h"
#include "nsIJSScriptObject.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFContentUtils.h"
#include "nsRDFDOMNodeList.h"
#include "nsStyleConsts.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMPaintListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFormListener.h"
#include "nsIScriptContextOwner.h"
#include "nsIStyledContent.h"
#include "nsIStyleRule.h"
#include "nsIURL.h"
#include "nsXULTreeElement.h"
#include "rdfutil.h"
#include "prlog.h"
#include "rdf.h"

// The XUL interfaces implemented by the RDF content node.
#include "nsIDOMXULElement.h"

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
static const char kXULNameSpaceURI[] = XUL_NAMESPACE_URI;
static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;
// End of XUL interface includes

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIContentIID,                NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMElementIID,             NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID,       NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMNodeIID,                NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMNodeListIID,            NS_IDOMNODELIST_IID);
static NS_DEFINE_IID(kIDocumentIID,               NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIEventListenerManagerIID,   NS_IEVENTLISTENERMANAGER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID,         NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,       NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID,        NS_IPRIVATEDOMEVENT_IID);
static NS_DEFINE_IID(kIRDFCompositeDataSourceIID, NS_IRDFCOMPOSITEDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFDocumentIID,            NS_IRDFDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFServiceIID,             NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID,      NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID,               NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXMLContentIID,             NS_IXMLCONTENT_IID);

static NS_DEFINE_CID(kEventListenerManagerCID, NS_EVENTLISTENERMANAGER_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,     NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID,           NS_RDFSERVICE_CID);

static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID, NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMMouseMotionListenerIID, NS_IDOMMOUSEMOTIONLISTENER_IID);
static NS_DEFINE_IID(kIDOMFocusListenerIID, NS_IDOMFOCUSLISTENER_IID);
static NS_DEFINE_IID(kIDOMFormListenerIID, NS_IDOMFORMLISTENER_IID);
static NS_DEFINE_IID(kIDOMLoadListenerIID, NS_IDOMLOADLISTENER_IID);
static NS_DEFINE_IID(kIDOMPaintListenerIID, NS_IDOMPAINTLISTENER_IID);

////////////////////////////////////////////////////////////////////////

struct XULBroadcastListener
{
	nsString mAttribute;
	nsCOMPtr<nsIDOMElement> mListener;

	XULBroadcastListener(const nsString& attr, nsIDOMElement* listen)
		: mAttribute(attr), mListener( dont_QueryInterface(listen) )
	{ // Nothing else to do 
	}
};

////////////////////////////////////////////////////////////////////////

class RDFElementImpl : public nsIDOMXULElement,
                       public nsIDOMEventReceiver,
                       public nsIScriptObjectOwner,
                       public nsIJSScriptObject,
                       public nsIStyledContent
{
public:
    RDFElementImpl(PRInt32 aNameSpaceID, nsIAtom* aTag);

    virtual ~RDFElementImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS
       
    // nsIDOMNode (from nsIDOMElement)
    NS_DECL_IDOMNODE
  
    // nsIDOMElement
    NS_DECL_IDOMELEMENT

    // nsIScriptObjectOwner
    NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
    NS_IMETHOD SetScriptObject(void *aScriptObject);

    // nsIContent (from nsIStyledContent)

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
    NS_IMETHOD GetNameSpacePrefixFromId(PRInt32 aNameSpaceID, nsIAtom*& aPrefix);
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

    NS_IMETHOD GetID(nsIAtom*& aResult) const;
    NS_IMETHOD GetClasses(nsVoidArray& aArray) const;
    NS_IMETHOD HasClass(nsIAtom* aClass) const;

    NS_IMETHOD GetContentStyleRule(nsIStyleRule*& aResult);
    NS_IMETHOD GetInlineStyleRule(nsIStyleRule*& aResult);

    /** NRA ***
    * Get a hint that tells the style system what to do when 
    * an attribute on this node changes.
    */
    NS_IMETHOD GetStyleHintForAttributeChange(
      const nsIAtom* aAttribute,
      PRInt32 *aHint) const;

    // nsIXMLContent (no longer implemented)
    NS_IMETHOD SetContainingNameSpace(nsINameSpace* aNameSpace);
    NS_IMETHOD GetContainingNameSpace(nsINameSpace*& aNameSpace) const;
    NS_IMETHOD SetNameSpacePrefix(nsIAtom* aNameSpace);
    NS_IMETHOD GetNameSpacePrefix(nsIAtom*& aNameSpace) const;
    NS_IMETHOD SetNameSpaceID(PRInt32 aNameSpaceID);

    // nsIDOMEventReceiver
    NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
    NS_IMETHOD GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult);

    // nsIDOMEventTarget interface
    NS_IMETHOD AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                PRBool aPostProcess, PRBool aUseCapture);
    NS_IMETHOD RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                   PRBool aPostProcess, PRBool aUseCapture);


    // nsIJSScriptObject
    virtual PRBool AddProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool GetProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool SetProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool EnumerateProperty(JSContext *aContext);
    virtual PRBool Resolve(JSContext *aContext, jsval aID);
    virtual PRBool Convert(JSContext *aContext, jsval aID);
    virtual void   Finalize(JSContext *aContext);

    // nsIDOMXULElement
    NS_DECL_IDOMXULELEMENT

    // Implementation methods
    nsresult EnsureContentsGenerated(void) const;

    nsresult AddScriptEventListener(nsIAtom* aName, const nsString& aValue, REFNSIID aIID);

    nsresult ExecuteOnChangeHandler(nsIDOMElement* anElement, const nsString& attrName);

    static nsresult
    ExecuteJSCode(nsIDOMElement* anElement);

    static nsresult
    GetElementsByTagName(nsIDOMNode* aNode,
                         const nsString& aTagName,
                         nsRDFDOMNodeList* aElements);

    static nsresult
    GetElementsByAttribute(nsIDOMNode* aNode,
                           const nsString& aAttributeName,
                           const nsString& aAttributeValue,
                           nsRDFDOMNodeList* aElements);

private:
    // pseudo-constants
    static nsrefcnt             gRefCnt;
    static nsIRDFService*       gRDFService;
    static nsINameSpaceManager* gNameSpaceManager;
    static PRInt32              kNameSpaceID_RDF;
    static PRInt32              kNameSpaceID_XUL;
    static nsIAtom*             kIdAtom;
    static nsIAtom*             kClassAtom;
    static nsIAtom*             kStyleAtom;
    static nsIAtom*             kContainerAtom;
    static nsIAtom*             kTreeAtom;

    nsIDocument*      mDocument;
    void*             mScriptObject;
    nsISupportsArray* mChildren;
    nsIContent*       mParent;
    nsINameSpace*     mNameSpace;
    nsIAtom*          mNameSpacePrefix;
    PRInt32           mNameSpaceID;
    nsIAtom*          mTag;
    nsIEventListenerManager* mListenerManager;
    nsXULAttributes*  mAttributes;
    PRBool            mContentsMustBeGenerated;
    nsVoidArray*		  mBroadcastListeners;
    nsIDOMXULElement* mBroadcaster;
    nsXULElement*     mInnerXULElement;

};


nsrefcnt             RDFElementImpl::gRefCnt;
nsIRDFService*       RDFElementImpl::gRDFService;
nsINameSpaceManager* RDFElementImpl::gNameSpaceManager;
nsIAtom*             RDFElementImpl::kIdAtom;
nsIAtom*             RDFElementImpl::kClassAtom;
nsIAtom*             RDFElementImpl::kStyleAtom;
nsIAtom*             RDFElementImpl::kContainerAtom;
nsIAtom*             RDFElementImpl::kTreeAtom;
PRInt32              RDFElementImpl::kNameSpaceID_RDF;
PRInt32              RDFElementImpl::kNameSpaceID_XUL;

////////////////////////////////////////////////////////////////////////
// RDFElementImpl

RDFElementImpl::RDFElementImpl(PRInt32 aNameSpaceID, nsIAtom* aTag)
    : mDocument(nsnull),
      mScriptObject(nsnull),
      mChildren(nsnull),
      mParent(nsnull),
      mNameSpace(nsnull),
      mNameSpaceID(aNameSpaceID),
      mTag(aTag),
      mListenerManager(nsnull),
      mAttributes(nsnull),
      mContentsMustBeGenerated(PR_FALSE),
      mInnerXULElement(nsnull),
      mBroadcastListeners(nsnull),
      mBroadcaster(nsnull)
{
    NS_INIT_REFCNT();
    NS_ADDREF(aTag);

    if (gRefCnt++ == 0) {
        nsresult rv;
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          kIRDFServiceIID,
                                          (nsISupports**) &gRDFService);

        NS_VERIFY(NS_SUCCEEDED(rv), "unable to get RDF service");

        kIdAtom        = NS_NewAtom("id");
        kClassAtom     = NS_NewAtom("class");
        kStyleAtom     = NS_NewAtom("style");
        kContainerAtom = NS_NewAtom("container");
        kTreeAtom      = NS_NewAtom("tree");

        rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                          nsnull,
                                          kINameSpaceManagerIID,
                                          (void**) &gNameSpaceManager);

        NS_VERIFY(NS_SUCCEEDED(rv), "unable to create namespace manager");

        if (gNameSpaceManager) {
            gNameSpaceManager->RegisterNameSpace(kRDFNameSpaceURI, kNameSpaceID_RDF);
            gNameSpaceManager->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
        }
    }
}

RDFElementImpl::~RDFElementImpl()
{
    delete mAttributes; // nsXULAttributes destructor takes care of the rest.

    //NS_IF_RELEASE(mDocument); // not refcounted
    //NS_IF_RELEASE(mParent)    // not refcounted
    NS_IF_RELEASE(mTag);
    NS_IF_RELEASE(mListenerManager);
    NS_IF_RELEASE(mChildren);

    // Release our broadcast listeners
    if (mBroadcastListeners != nsnull)
    {
        PRInt32 count = mBroadcastListeners->Count();
        for (PRInt32 i = 0; i < count; i++) {
            XULBroadcastListener* xulListener = (XULBroadcastListener*)mBroadcastListeners->ElementAt(0);
            RemoveBroadcastListener(xulListener->mAttribute, xulListener->mListener);
        }
    }

    // We might be an observes element. We need to remove our parent from
    // the broadcaster's array
    if (mBroadcaster != nsnull)
    {
        nsCOMPtr<nsIContent> parentContent;
        GetParent(*getter_AddRefs(parentContent));

        nsCOMPtr<nsIDOMElement> parentElement;
        parentElement = do_QueryInterface(parentContent);

        mBroadcaster->RemoveBroadcastListener("*", parentElement);
        NS_RELEASE(mBroadcaster);
    }

    if (--gRefCnt == 0) {
        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }

        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kClassAtom);
        NS_IF_RELEASE(kStyleAtom);
        NS_IF_RELEASE(kContainerAtom);
        NS_IF_RELEASE(kTreeAtom);
        NS_IF_RELEASE(gNameSpaceManager);
    }

    delete mInnerXULElement;
}


nsresult
NS_NewRDFElement(PRInt32 aNameSpaceId,
                 nsIAtom* aTag,
                 nsIContent** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    RDFElementImpl* element =
        new RDFElementImpl(aNameSpaceId, aTag);

    if (! element)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(element);
    *aResult = element;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(RDFElementImpl);
NS_IMPL_RELEASE(RDFElementImpl);

NS_IMETHODIMP 
RDFElementImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(nsIStyledContent::GetIID()) ||
        iid.Equals(kIContentIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIContent*, this);
    }
    else if (iid.Equals(nsIDOMXULElement::GetIID()) ||
             iid.Equals(kIDOMElementIID) ||
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
    else if (iid.Equals(nsIDOMXULTreeElement::GetIID()) &&
             (mNameSpaceID == kNameSpaceID_XUL) &&
             (mTag == kTreeAtom)) {
        if (! mInnerXULElement) {
            if ((mInnerXULElement = new nsXULTreeElement(this)) == nsnull)
                return NS_ERROR_OUT_OF_MEMORY;
        }

        return mInnerXULElement->QueryInterface(iid, result);
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
RDFElementImpl::GetNodeName(nsString& aNodeName)
{
    mTag->ToString(aNodeName);
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::GetNodeValue(nsString& aNodeValue)
{
    aNodeValue.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::SetNodeValue(const nsString& aNodeValue)
{
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::ELEMENT_NODE;
  return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::GetParentNode(nsIDOMNode** aParentNode)
{
    if (mParent) {
        return mParent->QueryInterface(kIDOMNodeIID, (void**) aParentNode);
    }
    else if (mDocument) {
        // If we don't have a parent, but we're in the document, we must
        // be the root node of the document. The DOM says that the root
        // is the document.
        return mDocument->QueryInterface(kIDOMNodeIID, (void**)aParentNode);
    }
    else {
        // A standalone element (i.e. one without a parent or a document)
        // implicitly has a document fragment as its parent according to
        // the DOM.

        // XXX create a doc fragment here as a pseudo-parent.
        NS_NOTYETIMPLEMENTED("can't handle standalone RDF elements");
        return NS_ERROR_NOT_IMPLEMENTED;
    }
}


NS_IMETHODIMP
RDFElementImpl::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    nsresult rv;

    nsRDFDOMNodeList* children;
    if (NS_FAILED(rv = nsRDFDOMNodeList::Create(&children))) {
        NS_ERROR("unable to create DOM node list");
        return rv;
    }

    PRInt32 count;
    if (NS_SUCCEEDED(rv = ChildCount(count))) {
        for (PRInt32 index = 0; index < count; ++index) {
            nsCOMPtr<nsIContent> child;
            if (NS_FAILED(rv = ChildAt(index, *getter_AddRefs(child)))) {
                NS_ERROR("unable to get child");
                break;
            }

            nsCOMPtr<nsIDOMNode> domNode;
            if (NS_FAILED(rv = child->QueryInterface(kIDOMNodeIID, (void**) getter_AddRefs(domNode)))) {
                NS_WARNING("child content doesn't support nsIDOMNode");
                continue;
            }

            if (NS_FAILED(rv = children->AppendNode(domNode))) {
                NS_ERROR("unable to append node to list");
                break;
            }
        }
    }

    *aChildNodes = children;
    NS_ADDREF(*aChildNodes);
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::GetFirstChild(nsIDOMNode** aFirstChild)
{
    nsresult rv;
    nsIContent* child;
    if (NS_SUCCEEDED(rv = ChildAt(0, child))) {
	if (nsnull == child) return(NS_ERROR_FAILURE);
        rv = child->QueryInterface(kIDOMNodeIID, (void**) aFirstChild);
        NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM node");
        NS_RELEASE(child); // balance the AddRef in ChildAt()
        return rv;
    }
    else {
        *aFirstChild = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
RDFElementImpl::GetLastChild(nsIDOMNode** aLastChild)
{
    nsresult rv;
    PRInt32 count;
    if (NS_FAILED(rv = ChildCount(count))) {
        NS_ERROR("unable to get child count");
        return rv;
    }
    if (count) {
        nsIContent* child;
        if (NS_SUCCEEDED(rv = ChildAt(count - 1, child))) {
            rv = child->QueryInterface(kIDOMNodeIID, (void**) aLastChild);
            NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM node");
            NS_RELEASE(child); // balance the AddRef in ChildAt()
        }
        return rv;
    }
    else {
        *aLastChild = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
RDFElementImpl::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    if (nsnull != mParent) {
        PRInt32 pos;
        mParent->IndexOf(this, pos);
        if (pos > -1) {
            nsIContent* prev;
            mParent->ChildAt(--pos, prev);
            if (prev) {
                nsresult rv = prev->QueryInterface(kIDOMNodeIID, (void**) aPreviousSibling);
                NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM node");
                NS_RELEASE(prev); // balance the AddRef in ChildAt()
                return rv;
            }
        }
    }

    // XXX Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their previous sibling.
    *aPreviousSibling = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::GetNextSibling(nsIDOMNode** aNextSibling)
{
    if (nsnull != mParent) {
        PRInt32 pos;
        mParent->IndexOf(this, pos);
        if (pos > -1) {
            nsIContent* next;
            mParent->ChildAt(++pos, next);
            if (nsnull != next) {
                nsresult res = next->QueryInterface(kIDOMNodeIID, (void**) aNextSibling);
                NS_ASSERTION(NS_OK == res, "not a DOM Node");
                NS_RELEASE(next); // balance the AddRef in ChildAt()
                return res;
            }
        }
    }
    // XXX Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their next sibling.
    *aNextSibling = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFElementImpl::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    if (mDocument) {
        return mDocument->QueryInterface(nsIDOMDocument::GetIID(), (void**) aOwnerDocument);
    }
    else {
        *aOwnerDocument = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
RDFElementImpl::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    // It's possible that mDocument will be null for an element that's
    // not in the content model (e.g., somebody is working on a
    // "scratch" element that has been removed from the content tree).
    if (mDocument) {
        nsIDOMNodeObserver* obs;
        if (NS_SUCCEEDED(mDocument->QueryInterface(nsIDOMNodeObserver::GetIID(), (void**) &obs))) {
            obs->OnInsertBefore(this, aNewChild, aRefChild);
            NS_RELEASE(obs);
        }
    }

    NS_ADDREF(aNewChild);
    *aReturn = aNewChild;
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    // It's possible that mDocument will be null for an element that's
    // not in the content model (e.g., somebody is working on a
    // "scratch" element that has been removed from the content tree).
    if (mDocument) {
        nsIDOMNodeObserver* obs;
        if (NS_SUCCEEDED(mDocument->QueryInterface(nsIDOMNodeObserver::GetIID(), (void**) &obs))) {
            obs->OnReplaceChild(this, aNewChild, aOldChild);
            NS_RELEASE(obs);
        }
    }

    NS_ADDREF(aNewChild);
    *aReturn = aNewChild;
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    // It's possible that mDocument will be null for an element that's
    // not in the content model (e.g., somebody is working on a
    // "scratch" element that has been removed from the content tree).
    if (mDocument) {
        nsIDOMNodeObserver* obs;
        if (NS_SUCCEEDED(mDocument->QueryInterface(nsIDOMNodeObserver::GetIID(), (void**) &obs))) {
            obs->OnRemoveChild(this, aOldChild);
            NS_RELEASE(obs);
        }
    }

    NS_ADDREF(aOldChild);
    *aReturn = aOldChild;
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    // It's possible that mDocument will be null for an element that's
    // not in the content model (e.g., somebody is working on a
    // "scratch" element that has been removed from the content tree).
    if (mDocument) {
        nsIDOMNodeObserver* obs;
        if (NS_SUCCEEDED(mDocument->QueryInterface(nsIDOMNodeObserver::GetIID(), (void**) &obs))) {
            obs->OnAppendChild(this, aNewChild);
            NS_RELEASE(obs);
        }
    }

    NS_ADDREF(aNewChild);
    *aReturn = aNewChild;
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::HasChildNodes(PRBool* aReturn)
{
    nsresult rv;
    PRInt32 count;
    if (NS_FAILED(rv = ChildCount(count))) {
        NS_ERROR("unable to count kids");
        return rv;
    }
    *aReturn = (count > 0);
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// nsIDOMElement interface

NS_IMETHODIMP
RDFElementImpl::GetTagName(nsString& aTagName)
{
    mTag->ToString(aTagName);
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::GetAttribute(const nsString& aName, nsString& aReturn)
{
    nsresult rv;
    PRInt32 nameSpaceID;
    nsIAtom* nameAtom;

    if (NS_FAILED(rv = ParseAttributeString(aName, nameAtom, nameSpaceID))) {
        NS_WARNING("unable to parse attribute name");
        return rv;
    }

    GetAttribute(nameSpaceID, nameAtom, aReturn);
    NS_RELEASE(nameAtom);
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::SetAttribute(const nsString& aName, const nsString& aValue)
{
    nsIDOMElementObserver* obs;
    if (NS_SUCCEEDED(mDocument->QueryInterface(nsIDOMElementObserver::GetIID(), (void**) &obs))) {
        obs->OnSetAttribute(this, aName, aValue);
        NS_RELEASE(obs);
    }
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::RemoveAttribute(const nsString& aName)
{
    nsIDOMElementObserver* obs;
    if (NS_SUCCEEDED(mDocument->QueryInterface(nsIDOMElementObserver::GetIID(), (void**) &obs))) {
        obs->OnRemoveAttribute(this, aName);
        NS_RELEASE(obs);
    }
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::GetAttributeNode(const nsString& aName, nsIDOMAttr** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFElementImpl::SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsIDOMElementObserver* obs;
    if (NS_SUCCEEDED(mDocument->QueryInterface(nsIDOMElementObserver::GetIID(), (void**) &obs))) {
        obs->OnSetAttributeNode(this, aNewAttr);
        NS_RELEASE(obs);
    }
    NS_ADDREF(aNewAttr);
    *aReturn = aNewAttr;
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsIDOMElementObserver* obs;
    if (NS_SUCCEEDED(mDocument->QueryInterface(nsIDOMElementObserver::GetIID(), (void**) &obs))) {
        obs->OnRemoveAttributeNode(this, aOldAttr);
        NS_RELEASE(obs);
    }
    NS_ADDREF(aOldAttr);
    *aReturn = aOldAttr;
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::GetElementsByTagName(const nsString& aName, nsIDOMNodeList** aReturn)
{
    nsresult rv;
    nsRDFDOMNodeList* elements;
    if (NS_FAILED(rv = nsRDFDOMNodeList::Create(&elements))) {
        NS_ERROR("unable to create node list");
        return rv;
    }

    nsIDOMNode* domElement;
    if (NS_SUCCEEDED(rv = QueryInterface(nsIDOMNode::GetIID(), (void**) &domElement))) {
        rv = GetElementsByTagName(domElement, aName, elements);
        NS_RELEASE(domElement);
    }

    *aReturn = elements;
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::GetElementsByAttribute(const nsString& aAttribute,
                                       const nsString& aValue,
                                       nsIDOMNodeList** aReturn)
{
    nsresult rv;
    nsRDFDOMNodeList* elements;
    if (NS_FAILED(rv = nsRDFDOMNodeList::Create(&elements))) {
        NS_ERROR("unable to create node list");
        return rv;
    }

    nsIDOMNode* domElement;
    if (NS_SUCCEEDED(rv = QueryInterface(nsIDOMNode::GetIID(), (void**) &domElement))) {
        rv = GetElementsByAttribute(domElement, aAttribute, aValue, elements);
        NS_RELEASE(domElement);
    }

    *aReturn = elements;
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::Normalize()
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// nsIXMLContent interface

NS_IMETHODIMP
RDFElementImpl::SetContainingNameSpace(nsINameSpace* aNameSpace)
{
    NS_PRECONDITION(aNameSpace != nsnull, "null ptr");
    if (! aNameSpace)
        return NS_ERROR_NULL_POINTER;

    NS_IF_RELEASE(mNameSpace);
    mNameSpace = aNameSpace;
    NS_ADDREF(mNameSpace);
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::GetContainingNameSpace(nsINameSpace*& aNameSpace) const
{
    aNameSpace = mNameSpace;
    NS_IF_ADDREF(aNameSpace);
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::SetNameSpacePrefix(nsIAtom* aNameSpacePrefix)
{
    NS_PRECONDITION(aNameSpacePrefix != nsnull, "null ptr");
    if (! aNameSpacePrefix)
        return NS_ERROR_NULL_POINTER;

    NS_IF_RELEASE(mNameSpacePrefix);
    mNameSpacePrefix = aNameSpacePrefix;
    NS_ADDREF(mNameSpacePrefix);
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::GetNameSpacePrefix(nsIAtom*& aNameSpacePrefix) const
{
    aNameSpacePrefix = mNameSpacePrefix;
    NS_IF_ADDREF(aNameSpacePrefix);
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::SetNameSpaceID(PRInt32 aNameSpaceID)
{
    mNameSpaceID = aNameSpaceID;
    return NS_OK;
}



////////////////////////////////////////////////////////////////////////
// nsIDOMEventReceiver interface

NS_IMETHODIMP
RDFElementImpl::AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    nsIEventListenerManager *manager;

    if (NS_OK == GetListenerManager(&manager)) {
        manager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
        NS_RELEASE(manager);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
RDFElementImpl::RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    if (nsnull != mListenerManager) {
        mListenerManager->RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
RDFElementImpl::AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                 PRBool aPostProcess, PRBool aUseCapture)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    PRInt32 flags = (aPostProcess ? NS_EVENT_FLAG_POST_PROCESS : NS_EVENT_FLAG_NONE) |
                    (aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE);

    manager->AddEventListenerByType(aListener, aType, flags);
    NS_RELEASE(manager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
RDFElementImpl::RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                    PRBool aPostProcess, PRBool aUseCapture)
{
  if (nsnull != mListenerManager) {
    PRInt32 flags = (aPostProcess ? NS_EVENT_FLAG_POST_PROCESS : NS_EVENT_FLAG_NONE) |
                    (aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE);

    mListenerManager->RemoveEventListenerByType(aListener, aType, flags);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
RDFElementImpl::GetListenerManager(nsIEventListenerManager** aResult)
{
    if (nsnull != mListenerManager) {
        NS_ADDREF(mListenerManager);
        *aResult = mListenerManager;
        return NS_OK;
    }
    nsresult rv = nsComponentManager::CreateInstance(kEventListenerManagerCID,
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
RDFElementImpl::GetNewListenerManager(nsIEventListenerManager **aResult)
{
    return nsComponentManager::CreateInstance(kEventListenerManagerCID,
                                        nsnull,
                                        kIEventListenerManagerIID,
                                        (void**) aResult);
}



////////////////////////////////////////////////////////////////////////
// nsIScriptObjectOwner interface

NS_IMETHODIMP 
RDFElementImpl::GetScriptObject(nsIScriptContext* aContext, void** aScriptObject)
{
    nsresult rv = NS_OK;

    if (! mScriptObject) {
        nsIScriptGlobalObject *global = aContext->GetGlobalObject();

        nsresult (*fn)(nsIScriptContext* aContext, nsISupports* aSupports, nsISupports* aParent, void** aReturn);

        if (mTag == kTreeAtom) {
            fn = NS_NewScriptXULTreeElement;
        }
        else {
            fn = NS_NewScriptXULElement;
        }

        rv = fn(aContext, (nsIDOMXULElement*) this, global, (void**) &mScriptObject);
        NS_RELEASE(global);

        // Ensure that a reference exists to this element
        if (mDocument) {
            nsAutoString tag;
            mTag->ToString(tag);

            char buf[64];
            char* p = buf;
            if (tag.Length() >= sizeof(buf))
                p = new char[tag.Length() + 1];

            aContext->AddNamedReference((void*) &mScriptObject, mScriptObject, buf);

            if (p != buf)
                delete[] p;
        }
    }

    *aScriptObject = mScriptObject;
    return rv;
}

NS_IMETHODIMP 
RDFElementImpl::SetScriptObject(void *aScriptObject)
{
    mScriptObject = aScriptObject;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIJSScriptObject interface

PRBool
RDFElementImpl::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_FALSE;
}

PRBool
RDFElementImpl::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_FALSE;
}

PRBool
RDFElementImpl::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_TRUE;
}

PRBool
RDFElementImpl::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_FALSE;
}

PRBool
RDFElementImpl::EnumerateProperty(JSContext *aContext)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_FALSE;
}


PRBool
RDFElementImpl::Resolve(JSContext *aContext, jsval aID)
{
    return PR_TRUE;
}


PRBool
RDFElementImpl::Convert(JSContext *aContext, jsval aID)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_FALSE;
}


void
RDFElementImpl::Finalize(JSContext *aContext)
{
    NS_NOTYETIMPLEMENTED("write me");
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
RDFElementImpl::GetDocument(nsIDocument*& aResult) const
{
    aResult = mDocument;
    NS_IF_ADDREF(aResult);
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::SetDocument(nsIDocument* aDocument, PRBool aDeep)
{
    if (aDocument == mDocument)
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIRDFDocument> rdfDoc;
    if (mDocument) {
        // Need to do a GetResource() here, because changing the document
        // may actually change the element's URI.
        nsCOMPtr<nsIRDFResource> resource;
        GetResource(getter_AddRefs(resource));

        // Remove this element from the RDF resource-to-element map in
        // the old document.
        if (resource) {
            if (NS_SUCCEEDED(mDocument->QueryInterface(kIRDFDocumentIID, getter_AddRefs(rdfDoc)))) {
                rv = rdfDoc->RemoveElementForResource(resource, this);
                NS_ASSERTION(NS_SUCCEEDED(rv), "error unmapping resource from element");
            }
        }

        // Release the named reference to the script object so it can
        // be garbage collected.
        if (mScriptObject) {
            nsIScriptContextOwner *owner = mDocument->GetScriptContextOwner();
            if (nsnull != owner) {
                nsIScriptContext *context;
                if (NS_OK == owner->GetScriptContext(&context)) {
                    context->RemoveReference((void *) &mScriptObject,
                                             mScriptObject);

                    NS_RELEASE(context);
                }
                NS_RELEASE(owner);
            }
        }
    }

    mDocument = aDocument; // not refcounted

    if (mDocument) {
        // Need to do a GetResource() here, because changing the document
        // may actually change the element's URI.
        nsCOMPtr<nsIRDFResource> resource;
        GetResource(getter_AddRefs(resource));

        // Add this element to the RDF resource-to-element map in the
        // new document.
        if (resource) {
            if (NS_SUCCEEDED(mDocument->QueryInterface(kIRDFDocumentIID, getter_AddRefs(rdfDoc)))) {
                rv = rdfDoc->AddElementForResource(resource, this);
                NS_ASSERTION(NS_SUCCEEDED(rv), "error mapping resource to element");
            }
        }

        // Add a named reference to the script object.
        if (mScriptObject) {
            nsIScriptContextOwner *owner = mDocument->GetScriptContextOwner();
            if (nsnull != owner) {
                nsIScriptContext *context;
                if (NS_OK == owner->GetScriptContext(&context)) {
                    nsAutoString tag;
                    mTag->ToString(tag);

                    char buf[64];
                    char* p = buf;
                    if (tag.Length() >= sizeof(buf))
                        p = new char[tag.Length() + 1];

                    context->AddNamedReference((void*) &mScriptObject, mScriptObject, buf);

                    if (p != buf)
                        delete[] p;

                    NS_RELEASE(context);
                }
                NS_RELEASE(owner);
            }
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
RDFElementImpl::GetParent(nsIContent*& aResult) const
{
    aResult = mParent;
    NS_IF_ADDREF(mParent);
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::SetParent(nsIContent* aParent)
{
    mParent = aParent; // no refcount
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::CanContainChildren(PRBool& aResult) const
{
    // XXX Hmm -- not sure if this is unilaterally true...
    aResult = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::ChildCount(PRInt32& aResult) const
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    aResult = mChildren ? mChildren->Count() : 0;
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    aResult = nsnull;
    if (! mChildren)
        return NS_OK;

    // XXX The ultraparanoid way to do this...
    nsISupports* obj = mChildren->ElementAt(aIndex);  // this automatically addrefs
    if (obj) {
      nsIContent* content;
      rv = obj->QueryInterface(kIContentIID, (void**) &content);
      NS_ASSERTION(rv == NS_OK, "not a content");
      obj->Release();
      aResult = content;
    }

    // But, since we're in a closed system, we can just do the following...
    //aResult = (nsIContent*) mChildren->ElementAt(aIndex); // this automatically addrefs
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    aResult = (mChildren) ? (mChildren->IndexOf(aPossibleChild)) : (-1);
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify)
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
            aKid->SetDocument(doc, PR_TRUE);
            if (aNotify) {
                doc->ContentInserted(this, aKid, aIndex);
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify)
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
            aKid->SetDocument(doc, PR_TRUE);
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
RDFElementImpl::AppendChildTo(nsIContent* aKid, PRBool aNotify)
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
            aKid->SetDocument(doc, PR_TRUE);
            if (aNotify) {
                doc->ContentInserted(this, aKid, mChildren->Count() - 1);
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
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
RDFElementImpl::IsSynthetic(PRBool& aResult)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
RDFElementImpl::GetNameSpaceID(PRInt32& aNameSpaceID) const
{
    aNameSpaceID = mNameSpaceID;
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::GetTag(nsIAtom*& aResult) const
{
    aResult = mTag;
    NS_ADDREF(aResult);
    return NS_OK;
}

NS_IMETHODIMP 
RDFElementImpl::ParseAttributeString(const nsString& aStr, 
                                     nsIAtom*& aName, 
                                     PRInt32& aNameSpaceID)
{
static char kNameSpaceSeparator[] = ":";

    nsAutoString prefix;
    nsAutoString name(aStr);
    PRInt32 nsoffset = name.Find(kNameSpaceSeparator);
    if (-1 != nsoffset) {
        name.Left(prefix, nsoffset);
        name.Cut(0, nsoffset+1);
    }

    // Figure out the namespace ID

    // XXX This is currently broken, because _nobody_ will ever set
    // the namespace scope via the nsIXMLElement interface. To make
    // that work will require a bit more machinery: something that
    // remembers the XML namespace scoping as nodes get created in the
    // RDF graph, and can then extract them later when content nodes
    // are built via the content model builders.
    //
    // Since mNameSpace will _always_ be null, specifying
    // kNameSpaceID_None allows us to at least match on the
    // tag. You'll start seeing problems when the same name is used in
    // different namespaces.
    //
    // See http://bugzilla.mozilla.org/show_bug.cgi?id=3275 for more
    // info.

    aNameSpaceID = kNameSpaceID_None;
    if (0 < prefix.Length()) {
        nsIAtom* nameSpaceAtom = NS_NewAtom(prefix);
        if (mNameSpace) {
            mNameSpace->FindNameSpaceID(nameSpaceAtom, aNameSpaceID);
        }
        NS_RELEASE(nameSpaceAtom);
    }

    aName = NS_NewAtom(name);
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::GetNameSpacePrefixFromId(PRInt32 aNameSpaceID, 
                                         nsIAtom*& aPrefix)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



// XXX attribute code swiped from nsGenericContainerElement
// this class could probably just use nsGenericContainerElement
// needed to maintain attribute namespace ID as well as ordering
NS_IMETHODIMP 
RDFElementImpl::SetAttribute(PRInt32 aNameSpaceID,
                             nsIAtom* aName, 
                             const nsString& aValue,
                             PRBool aNotify)
{
    NS_ASSERTION(kNameSpaceID_Unknown != aNameSpaceID, "must have name space ID");
    if (kNameSpaceID_Unknown == aNameSpaceID)
        return NS_ERROR_ILLEGAL_VALUE;

    NS_ASSERTION(nsnull != aName, "must have attribute name");
    if (nsnull == aName)
        return NS_ERROR_NULL_POINTER;

    if (nsnull == mAttributes) {
        if ((mAttributes = new nsXULAttributes()) == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv = NS_OK;

    // Check to see if the CLASS attribute is being set.  If so, we need to rebuild our
    // class list.
    if (mDocument && (aNameSpaceID == kNameSpaceID_None) && aName == kClassAtom) {
      mAttributes->UpdateClassList(aValue);
    }

    // Check to see if the STYLE attribute is being set.  If so, we need to create a new
    // style rule based off the value of this attribute, and we need to let the document
    // know about the StyleRule change.
    if (mDocument && (aNameSpaceID == kNameSpaceID_None) && aName == kStyleAtom) {

        nsIURL* docURL = nsnull;
        if (nsnull != mDocument) {
            mDocument->GetBaseURL(docURL);
        }

        mAttributes->UpdateStyleRule(docURL, aValue);
        // XXX Some kind of special document update might need to happen here.
    }

    // XXX need to check if they're changing an event handler: if so, then we need
    // to unhook the old one.
    
    nsXULAttribute* attr;
    PRBool successful = PR_FALSE;
    PRInt32 index = 0;
    PRInt32 count = mAttributes->Count();
    while (index < count) {
        attr = mAttributes->ElementAt(index);
        if ((aNameSpaceID == attr->mNameSpaceID) && (aName == attr->mName))
            break;
        index++;
    }

    if (index < count) {
        attr->mValue = aValue;
    }
    else { // didn't find it
        attr = new nsXULAttribute(aNameSpaceID, aName, aValue);
        if (! attr)
          return NS_ERROR_OUT_OF_MEMORY;

        mAttributes->AppendElement(attr);
    }

    // Check for event handlers
    nsString attributeName;
    aName->ToString(attributeName);

    if (attributeName.EqualsIgnoreCase("onclick") ||
        attributeName.EqualsIgnoreCase("ondblclick") ||
        attributeName.EqualsIgnoreCase("onmousedown") ||
        attributeName.EqualsIgnoreCase("onmouseup") ||
        attributeName.EqualsIgnoreCase("onmouseover") ||
        attributeName.EqualsIgnoreCase("onmouseout"))
        AddScriptEventListener(aName, aValue, kIDOMMouseListenerIID);
    else if (attributeName.EqualsIgnoreCase("onkeydown") ||
             attributeName.EqualsIgnoreCase("onkeyup") ||
             attributeName.EqualsIgnoreCase("onkeypress"))
        AddScriptEventListener(aName, aValue, kIDOMKeyListenerIID);
    else if (attributeName.EqualsIgnoreCase("onmousemove"))
        AddScriptEventListener(aName, aValue, kIDOMMouseMotionListenerIID); 
    else if (attributeName.EqualsIgnoreCase("onload"))
        AddScriptEventListener(aName, aValue, kIDOMLoadListenerIID); 
    else if (attributeName.EqualsIgnoreCase("onunload") ||
             attributeName.EqualsIgnoreCase("onabort") ||
             attributeName.EqualsIgnoreCase("onerror"))
        AddScriptEventListener(aName, aValue, kIDOMLoadListenerIID);
    else if (attributeName.EqualsIgnoreCase("onfocus") ||
             attributeName.EqualsIgnoreCase("onblur"))
        AddScriptEventListener(aName, aValue, kIDOMFocusListenerIID);
    else if (attributeName.EqualsIgnoreCase("onsubmit") ||
             attributeName.EqualsIgnoreCase("onreset") ||
             attributeName.EqualsIgnoreCase("onchange"))
        AddScriptEventListener(aName, aValue, kIDOMFormListenerIID);
    else if (attributeName.EqualsIgnoreCase("onpaint"))
        AddScriptEventListener(aName, aValue, kIDOMPaintListenerIID); 

    // Notify any broadcasters that are listening to this node.
    if (mBroadcastListeners != nsnull)
    {
        count = mBroadcastListeners->Count();
        for (PRInt32 i = 0; i < count; i++) {
            XULBroadcastListener* xulListener = (XULBroadcastListener*)mBroadcastListeners->ElementAt(i);
            nsString aString;
            aName->ToString(aString);
            if (xulListener->mAttribute == aString) {
                nsCOMPtr<nsIDOMElement> element;
                element = do_QueryInterface(xulListener->mListener);
                if (element) {
                    // First we set the attribute in the observer.
                    element->SetAttribute(aString, aValue);
                    ExecuteOnChangeHandler(element, aString);
                }
            }
        }
    }

    if (NS_SUCCEEDED(rv) && aNotify && (nsnull != mDocument)) {
        mDocument->AttributeChanged(this, aName, NS_STYLE_HINT_UNKNOWN);
    }

    // Check to see if this is the RDF:container property; if so, and
    // the value is "true", then remember to generate our kids on
    // demand.
    if ((aNameSpaceID == kNameSpaceID_RDF) &&
        (aName == kContainerAtom) &&
        (aValue.EqualsIgnoreCase("true"))) {
        mContentsMustBeGenerated = PR_TRUE;
    }

    return rv;
}

nsresult
RDFElementImpl::AddScriptEventListener(nsIAtom* aName, const nsString& aValue, REFNSIID aIID)
{
    if (! mDocument)
        return NS_OK; // XXX

    nsresult ret = NS_OK;
    nsIScriptContext* context;
    nsIScriptContextOwner* owner;

    owner = mDocument->GetScriptContextOwner();
    if (NS_OK == owner->GetScriptContext(&context)) {
        nsIEventListenerManager *manager;
        if (NS_OK == GetListenerManager(&manager)) {
            nsIScriptObjectOwner* owner;
            if (NS_OK == QueryInterface(kIScriptObjectOwnerIID,
                                        (void**) &owner)) {
                ret = manager->AddScriptEventListener(context, owner,
                                                      aName, aValue, aIID);
                NS_RELEASE(owner);
            }
            NS_RELEASE(manager);
        }
        NS_RELEASE(context);
    }
    NS_RELEASE(owner);

    return ret;
}

NS_IMETHODIMP
RDFElementImpl::GetAttribute(PRInt32 aNameSpaceID,
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

    if (nsnull != mAttributes) {
        PRInt32 count = mAttributes->Count();
        PRInt32 index;
        for (index = 0; index < count; index++) {
            const nsXULAttribute* attr = (const nsXULAttribute*)mAttributes->ElementAt(index);
            if (((attr->mNameSpaceID == aNameSpaceID) ||
                 (aNameSpaceID == kNameSpaceID_Unknown) ||
                 (aNameSpaceID == kNameSpaceID_None)) &&
                (attr->mName == aName)) {
                aResult = attr->mValue;
                if (0 < aResult.Length()) {
                    rv = NS_CONTENT_ATTR_HAS_VALUE;
                }
                else {
                    rv = NS_CONTENT_ATTR_NO_VALUE;
                }
                if ((aNameSpaceID == kNameSpaceID_None) &&
                    (attr->mName == kIdAtom))
                {
                  aResult = attr->mValue;

                  // RDF will treat all document IDs as absolute URIs, so we'll need convert 
                  // a possibly-absolute URI into a relative ID attribute.
                  if (nsnull != mDocument) {
                    nsIURL* docURL = nsnull;
                    mDocument->GetBaseURL(docURL);
                    if (docURL) {
                      const char* url;
                      docURL->GetSpec(&url);
                      rdf_PossiblyMakeRelative(url, aResult);
                      NS_RELEASE(docURL);
                    }
                  }
                }
                break;
            }
        }
    }
    return rv;
}

NS_IMETHODIMP
RDFElementImpl::UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify)
{
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    if (nsnull == aName) {
        return NS_ERROR_NULL_POINTER;
    }

    // Check to see if the CLASS attribute is being unset.  If so, we need to delete our
    // class list.
    if (mDocument && (aNameSpaceID == kNameSpaceID_None) && aName == kClassAtom) {
      mAttributes->UpdateClassList("");
    }
    
    if (mDocument && (aNameSpaceID == kNameSpaceID_None) && aName == kStyleAtom) {

        nsIURL* docURL = nsnull;
        if (nsnull != mDocument) {
            mDocument->GetBaseURL(docURL);
        }

        mAttributes->UpdateStyleRule(docURL, "");
        // XXX Some kind of special document update might need to happen here.
    }

    nsresult rv = NS_OK;
    PRBool successful = PR_FALSE;
    if (nsnull != mAttributes) {
        PRInt32 count = mAttributes->Count();
        PRInt32 index;
        for (index = 0; index < count; index++) {
            nsXULAttribute* attr = (nsXULAttribute*)mAttributes->ElementAt(index);
            if ((attr->mNameSpaceID == aNameSpaceID) && (attr->mName == aName)) {
                mAttributes->RemoveElementAt(index);
                delete attr;
                successful = PR_TRUE;
                break;
            }
        }
    }

    // XUL Only. Find out if we have a broadcast listener for this element.
    if (successful) {
      if (mBroadcastListeners != nsnull) {
        PRInt32 count = mBroadcastListeners->Count();
        for (PRInt32 i = 0; i < count; i++)
        {
            XULBroadcastListener* xulListener = (XULBroadcastListener*)mBroadcastListeners->ElementAt(i);
            nsString aString;
            aName->ToString(aString);
            if (xulListener->mAttribute == aString) {
                // Unset the attribute in the broadcast listener.
                nsCOMPtr<nsIDOMElement> element;
                element = do_QueryInterface(xulListener->mListener);
                if (element)
                  element->RemoveAttribute(aString);
            }
        }
      }
   
      // Notify document
      if (NS_SUCCEEDED(rv) && aNotify && (nsnull != mDocument)) {
          mDocument->AttributeChanged(this, aName, NS_STYLE_HINT_UNKNOWN);
      }
    }

    // End XUL Only Code
    return rv;
}

NS_IMETHODIMP
RDFElementImpl::GetAttributeNameAt(PRInt32 aIndex,
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

    if (nsnull != mAttributes) {
        nsXULAttribute* attr = (nsXULAttribute*)mAttributes->ElementAt(aIndex);
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
RDFElementImpl::GetAttributeCount(PRInt32& aResult) const
{
#if defined(CREATE_PROPERTIES_AS_ATTRIBUTES)
    NS_NOTYETIMPLEMENTED("write me!");     // XXX need to write this...
#endif // defined(CREATE_PROPERTIES_AS_ATTRIBUTES)

    nsresult rv = NS_OK;
    if (nsnull != mAttributes) {
        aResult = mAttributes->Count();
    }
    else {
        aResult = 0;
    }

    return rv;
}


static void
rdf_Indent(FILE* out, PRInt32 aIndent)
{
    for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
}

NS_IMETHODIMP
RDFElementImpl::List(FILE* out, PRInt32 aIndent) const
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
RDFElementImpl::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFElementImpl::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFElementImpl::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFElementImpl::SizeOf(nsISizeOfHandler* aHandler) const
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP 
RDFElementImpl::HandleDOMEvent(nsIPresContext& aPresContext,
                                       nsEvent* aEvent,
                                       nsIDOMEvent** aDOMEvent,
                                       PRUint32 aFlags,
                                       nsEventStatus& aEventStatus)
{
    nsresult ret = NS_OK;
  
    nsIDOMEvent* domEvent = nsnull;
    if (NS_EVENT_FLAG_INIT == aFlags) {
        aDOMEvent = &domEvent;
        // In order for the event to have a proper target for menus (which have no corresponding
        // frame target in the visual model), we have to explicitly set the target of the
        // event to prevent it from trying to retrieve the target from a frame.
        nsString tagName;
        GetTagName(tagName);
        if (tagName == "menu" || tagName == "menuitem" ||
            tagName == "menubar") {
            nsCOMPtr<nsIEventListenerManager> listenerManager;
            if (NS_FAILED(ret = GetListenerManager(getter_AddRefs(listenerManager)))) {
                NS_ERROR("Unable to instantiate a listener manager on a menu event.");
                return ret;
            }
            if (NS_FAILED(ret = listenerManager->CreateEvent(aPresContext, aEvent, aDOMEvent))) {
                NS_ERROR("Menu event will fail without the ability to create the event early.");
                return ret;
            }
            domEvent->SetTarget(this);
        }
    }
  
    //Capturing stage
    //XXX Nees impl.  Talk to joki@netscape.com for help.
  
    //Local handling stage
    if (nsnull != mListenerManager) {
        mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aFlags, aEventStatus);
    }

    //Bubbling stage
    if ((NS_EVENT_FLAG_CAPTURE != aFlags) && (mParent != nsnull)) {
        ret = mParent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                      NS_EVENT_FLAG_BUBBLE, aEventStatus);
    }

    if (NS_EVENT_FLAG_INIT == aFlags) {
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
RDFElementImpl::RangeAdd(nsIDOMRange& aRange) 
{  
    // rdf content does not yet support DOM ranges
    return NS_OK;
}

 
NS_IMETHODIMP 
RDFElementImpl::RangeRemove(nsIDOMRange& aRange) 
{
    // rdf content does not yet support DOM ranges
    return NS_OK;
}                                                                        


NS_IMETHODIMP 
RDFElementImpl::GetRangeList(nsVoidArray*& aResult) const
{
    // rdf content does not yet support DOM ranges
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIDOMXULElement interface

NS_IMETHODIMP
RDFElementImpl::DoCommand()
{
	return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::AddBroadcastListener(const nsString& attr, nsIDOMElement* anElement) 
{ 
	// Add ourselves to the array.
	if (mBroadcastListeners == nsnull)
  {
      mBroadcastListeners = new nsVoidArray();
  }

	mBroadcastListeners->AppendElement(new XULBroadcastListener(attr, anElement));

	// We need to sync up the initial attribute value.
  nsCOMPtr<nsIContent> listener( do_QueryInterface(anElement) );

  // Find out if the attribute is even present at all.
  nsString attrValue;
  nsIAtom* kAtom = NS_NewAtom(attr);
	nsresult result = GetAttribute(kNameSpaceID_None, kAtom, attrValue);
	PRBool attrPresent = (result == NS_CONTENT_ATTR_NO_VALUE ||
                        result == NS_CONTENT_ATTR_HAS_VALUE);

	if (attrPresent)
  {
    // Set the attribute 
    anElement->SetAttribute(attr, attrValue);
  }
  else
  {
    // Unset the attribute
    anElement->RemoveAttribute(attr);
  }

  NS_RELEASE(kAtom);

	return NS_OK; 
}
	

NS_IMETHODIMP
RDFElementImpl::RemoveBroadcastListener(const nsString& attr, nsIDOMElement* anElement) 
{ 
  if (mBroadcastListeners == nsnull)
    return NS_OK;

	// Find the element.
	PRInt32 count = mBroadcastListeners->Count();
	for (PRInt32 i = 0; i < count; i++)
	{
		XULBroadcastListener* xulListener = (XULBroadcastListener*)mBroadcastListeners->ElementAt(i);
		
		if ((xulListener->mAttribute == attr || xulListener->mAttribute == "*") &&
			  xulListener->mListener == nsCOMPtr<nsIDOMElement>( dont_QueryInterface(anElement) ))
		{
			// Do the removal.
			mBroadcastListeners->RemoveElementAt(i);
			delete xulListener;
			return NS_OK;
		}
	}

	return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::GetResource(nsIRDFResource** aResource)
{
    nsAutoString uri;
    if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(kNameSpaceID_None, kIdAtom, uri)) {
        // RDF will treat all document IDs as absolute URIs, so we'll need convert 
        // a possibly-relative ID attribute into a fully-qualified (that is, with
        // the current document's URL) URI.
        NS_ASSERTION(mDocument != nsnull, "element has no document");
        if (nsnull != mDocument) {
          nsIURL* docURL = nsnull;
          mDocument->GetBaseURL(docURL);
          if (docURL) {
            const char* url;
            docURL->GetSpec(&url);
            rdf_PossiblyMakeAbsolute(url, uri);
            NS_RELEASE(docURL);
          }
        }
        return gRDFService->GetUnicodeResource(uri, aResource);
    }

    *aResource = nsnull;
    return NS_OK;
}



////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
RDFElementImpl::EnsureContentsGenerated(void) const
{
    if (! mContentsMustBeGenerated)
        return NS_OK;

    nsresult rv;

    // Ensure that the element is actually _in_ the document tree;
    // otherwise, somebody is trying to generate children for a node
    // that's not currently in the content model.
    NS_PRECONDITION(mDocument != nsnull, "element not in tree");
    if (!mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    // XXX hack because we can't use "mutable"
    RDFElementImpl* unconstThis = NS_CONST_CAST(RDFElementImpl*, this);

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


nsresult
RDFElementImpl::ExecuteOnChangeHandler(nsIDOMElement* anElement, const nsString& attrName)
{
    // Now we execute the onchange handler in the context of the
    // observer. We need to find the observer in order to
    // execute the handler.
    nsCOMPtr<nsIDOMNodeList> nodeList;
    if (NS_SUCCEEDED(anElement->GetElementsByTagName("observes",
                                                     getter_AddRefs(nodeList)))) {
        // We have a node list that contains some observes nodes.
        PRUint32 length;
        nodeList->GetLength(&length);
        for (PRUint32 i = 0; i < length; i++) {
            nsIDOMNode* domNode;
            nodeList->Item(i, &domNode);
            nsCOMPtr<nsIDOMElement> domElement;
            domElement = do_QueryInterface(domNode);
            if (domElement) {
                // We have a domElement. Find out if it was listening to us.
                nsAutoString listeningToID;
                domElement->GetAttribute("element", listeningToID);
                nsAutoString broadcasterID;
                GetAttribute("id", broadcasterID);
                if (listeningToID == broadcasterID) {
                    // We are observing the broadcaster, but is this the right
                    // attribute?
                    nsAutoString listeningToAttribute;
                    domElement->GetAttribute("attribute", listeningToAttribute);
                    if (listeningToAttribute == attrName) {
                        // This is the right observes node.
                        // Execute the onchange event handler
                        ExecuteJSCode(domElement);
                    }
                }
            }
            NS_IF_RELEASE(domNode);
        }
    }

    return NS_OK;
}

nsresult
RDFElementImpl::ExecuteJSCode(nsIDOMElement* anElement)
{ 
    // This code executes in every presentation context in which this
    // document is appearing.
    nsCOMPtr<nsIContent> content;
    content = do_QueryInterface(anElement);
    if (!content)
      return NS_OK;

    nsCOMPtr<nsIDocument> document;
    content->GetDocument(*getter_AddRefs(document));

    if (!document)
      return NS_OK;

    PRInt32 count = document->GetNumberOfShells();
    for (PRInt32 i = 0; i < count; i++) {
        nsIPresShell* shell = document->GetShellAt(i);
        if (nsnull == shell)
            continue;

        // Retrieve the context in which our DOM event will fire.
        nsCOMPtr<nsIPresContext> aPresContext;
        shell->GetPresContext(getter_AddRefs(aPresContext));
    
        NS_RELEASE(shell);

        // Handle the DOM event
        nsEventStatus status = nsEventStatus_eIgnore;
        nsEvent event;
        event.eventStructType = NS_EVENT;
        event.message = NS_FORM_CHANGE; // XXX: I feel dirty and evil for subverting this.
        content->HandleDOMEvent(*aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);
    }

    return NS_OK;
}



nsresult
RDFElementImpl::GetElementsByTagName(nsIDOMNode* aNode,
                                     const nsString& aTagName,
                                     nsRDFDOMNodeList* aElements)
{
    nsresult rv;

    nsCOMPtr<nsIDOMNodeList> children;
    if (NS_FAILED(rv = aNode->GetChildNodes( getter_AddRefs(children) ))) {
        NS_ERROR("unable to get node's children");
        return rv;
    }

    // no kids: terminate the recursion
    if (! children)
        return NS_OK;

    PRUint32 length;
    if (NS_FAILED(children->GetLength(&length))) {
        NS_ERROR("unable to get node list's length");
        return rv;
    }

    for (PRUint32 i = 0; i < length; ++i) {
        nsCOMPtr<nsIDOMNode> child;
        if (NS_FAILED(rv = children->Item(i, getter_AddRefs(child) ))) {
            NS_ERROR("unable to get child from list");
            return rv;
        }

        nsCOMPtr<nsIDOMElement> element;
        element = do_QueryInterface(child);
        if (!element)
          continue;

        if (aTagName.Equals("*")) {
            if (NS_FAILED(rv = aElements->AppendNode(child))) {
                NS_ERROR("unable to append element to node list");
                return rv;
            }
        }
        else {
            nsAutoString name;
            if (NS_FAILED(rv = child->GetNodeName(name))) {
                NS_ERROR("unable to get node name");
                return rv;
            }

            if (aTagName.Equals(name)) {
                if (NS_FAILED(rv = aElements->AppendNode(child))) {
                    NS_ERROR("unable to append element to node list");
                    return rv;
                }
            }
        }

        // Now recursively look for children
        if (NS_FAILED(rv = GetElementsByTagName(child, aTagName, aElements))) {
            NS_ERROR("unable to recursively get elements by tag name");
            return rv;
        }
    }

    return NS_OK;
}

nsresult
RDFElementImpl::GetElementsByAttribute(nsIDOMNode* aNode,
                                       const nsString& aAttribute,
                                       const nsString& aValue,
                                       nsRDFDOMNodeList* aElements)
{
    nsresult rv;

    nsCOMPtr<nsIDOMNodeList> children;
    if (NS_FAILED(rv = aNode->GetChildNodes( getter_AddRefs(children) ))) {
        NS_ERROR("unable to get node's children");
        return rv;
    }

    // no kids: terminate the recursion
    if (! children)
        return NS_OK;

    PRUint32 length;
    if (NS_FAILED(children->GetLength(&length))) {
        NS_ERROR("unable to get node list's length");
        return rv;
    }

    for (PRUint32 i = 0; i < length; ++i) {
        nsCOMPtr<nsIDOMNode> child;
        if (NS_FAILED(rv = children->Item(i, getter_AddRefs(child) ))) {
            NS_ERROR("unable to get child from list");
            return rv;
        }

        nsCOMPtr<nsIDOMElement> element;
        element = do_QueryInterface(child);
        if (!element)
          continue;

        nsAutoString attrValue;
        if (NS_FAILED(rv = element->GetAttribute(aAttribute, attrValue))) {
            NS_ERROR("unable to get attribute value");
            return rv;
        }

        if ((attrValue == aValue) || (attrValue.Length() > 0 && aValue == "*")) {
            if (NS_FAILED(rv = aElements->AppendNode(child))) {
                NS_ERROR("unable to append element to node list");
                return rv;
            }
        }
       
        // Now recursively look for children
        if (NS_FAILED(rv = GetElementsByAttribute(child, aAttribute, aValue, aElements))) {
            NS_ERROR("unable to recursively get elements by attribute");
            return rv;
        }
    }

    return NS_OK;
}

// nsIStyledContent Implementation
NS_IMETHODIMP
RDFElementImpl::GetID(nsIAtom*& aResult) const
{
  nsString value;
  GetAttribute(kNameSpaceID_None, kIdAtom, value);

  aResult = NS_NewAtom(value); // The NewAtom call does the AddRef.
  return NS_OK;
}
    
NS_IMETHODIMP
RDFElementImpl::GetClasses(nsVoidArray& aArray) const
{
  return NS_OK;
}

NS_IMETHODIMP 
RDFElementImpl::HasClass(nsIAtom* aClass) const
{
  return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::GetContentStyleRule(nsIStyleRule*& aResult)
{
  aResult = nsnull;
  return NS_OK;
}
    
NS_IMETHODIMP
RDFElementImpl::GetInlineStyleRule(nsIStyleRule*& aResult)
{
  // Fetch the cached style rule from the attributes.
  nsresult result = NS_ERROR_NULL_POINTER;
  aResult = nsnull;
  if (mAttributes != nsnull)
    result = mAttributes->GetInlineStyleRule(aResult);
  return result;
}

NS_IMETHODIMP
RDFElementImpl::GetStyleHintForAttributeChange(const nsIAtom* aAttribute, PRInt32 *aHint) const
{
  *aHint = NS_STYLE_HINT_CONTENT;
  if (mNameSpaceID == kNameSpaceID_XUL)
  {
      // We are a XUL tag and need to specify a style hint.
      *aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}
