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

  1) Instead of creating all the properties up front, instantiate them
     lazily as they are asked for (see Bug 3367).

 */

#include "nsCOMPtr.h"
#include "nsDOMCID.h"
#include "nsDOMEvent.h"
#include "nsXULAttributes.h"
#include "nsIXULPopupListener.h"
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
#include "nsIDOMMenuListener.h"
#include "nsIScriptContextOwner.h"
#include "nsIStyledContent.h"
#include "nsIStyleRule.h"
#include "nsIURL.h"
#include "nsXULTreeElement.h"
#include "rdfutil.h"
#include "prlog.h"
#include "rdf.h"

#include "nsIController.h"

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
static NS_DEFINE_IID(kIDOMEventTargetIID,         NS_IDOMEVENTTARGET_IID);
static NS_DEFINE_CID(kNameSpaceManagerCID,     NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID,           NS_RDFSERVICE_CID);

static NS_DEFINE_IID(kIXULPopupListenerIID, NS_IXULPOPUPLISTENER_IID);
static NS_DEFINE_CID(kXULPopupListenerCID, NS_XULPOPUPLISTENER_CID);

static NS_DEFINE_IID(kIDOMMouseListenerIID,       NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID,         NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMMouseMotionListenerIID, NS_IDOMMOUSEMOTIONLISTENER_IID);
static NS_DEFINE_IID(kIDOMFocusListenerIID,       NS_IDOMFOCUSLISTENER_IID);
static NS_DEFINE_IID(kIDOMFormListenerIID,        NS_IDOMFORMLISTENER_IID);
static NS_DEFINE_IID(kIDOMLoadListenerIID,        NS_IDOMLOADLISTENER_IID);
static NS_DEFINE_IID(kIDOMPaintListenerIID,       NS_IDOMPAINTLISTENER_IID);
static NS_DEFINE_IID(kIDOMMenuListenerIID,        NS_IDOMMENULISTENER_IID);

////////////////////////////////////////////////////////////////////////

struct XULBroadcastListener
{
	nsAutoString mAttribute;
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
                       public nsIStyledContent,
                       public nsIXMLContent
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

    NS_IMETHOD GetContentStyleRules(nsISupportsArray* aRules);
    NS_IMETHOD GetInlineStyleRules(nsISupportsArray* aRules);

    /** NRA ***
    * Get a hint that tells the style system what to do when 
    * an attribute on this node changes.
    */
    NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                        PRInt32& aHint) const;

    // nsIXMLContent
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
                                PRBool aUseCapture);
    NS_IMETHOD RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                   PRBool aUseCapture);


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
    nsresult GetRefResource(nsIRDFResource** aResult);

    nsresult EnsureContentsGenerated(void) const;

    nsresult AddScriptEventListener(nsIAtom* aName, const nsString& aValue, REFNSIID aIID);

    nsresult ExecuteOnChangeHandler(nsIDOMElement* anElement, const nsString& attrName);

    PRBool ElementIsInDocument();

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
    static nsIAtom*             kRefAtom;
    static nsIAtom*             kClassAtom;
    static nsIAtom*             kStyleAtom;
    static nsIAtom*             kLazyContentAtom;
    static nsIAtom*             kTreeAtom;

    static nsIAtom*             kPopupAtom;
    static nsIAtom*             kTooltipAtom;
    static nsIAtom*             kContextAtom;

    nsIDocument*           mDocument;           // [WEAK]
    void*                  mScriptObject;       // [OWNER]
    nsISupportsArray*      mChildren;           // [OWNER]
    nsIContent*            mParent;             // [WEAK]
    nsCOMPtr<nsINameSpace> mNameSpace;          // [OWNER]
    nsCOMPtr<nsIAtom>      mNameSpacePrefix;    // [OWNER]
    PRInt32                mNameSpaceID;
    nsIAtom*               mTag;                // [OWNER]
    nsIEventListenerManager* mListenerManager;  // [OWNER]
    nsXULAttributes*       mAttributes;         // [OWNER]
    PRBool                 mContentsMustBeGenerated;
    nsVoidArray*		   mBroadcastListeners; // [WEAK]
    nsIDOMXULElement*      mBroadcaster;        // [OWNER]
    nsXULElement*          mInnerXULElement;    // [OWNER]
    nsIController*         mController;         // [OWNER]
};


nsrefcnt             RDFElementImpl::gRefCnt;
nsIRDFService*       RDFElementImpl::gRDFService;
nsINameSpaceManager* RDFElementImpl::gNameSpaceManager;
nsIAtom*             RDFElementImpl::kIdAtom;
nsIAtom*             RDFElementImpl::kRefAtom;
nsIAtom*             RDFElementImpl::kClassAtom;
nsIAtom*             RDFElementImpl::kStyleAtom;
nsIAtom*             RDFElementImpl::kLazyContentAtom;
nsIAtom*             RDFElementImpl::kTreeAtom;
nsIAtom*             RDFElementImpl::kPopupAtom;
nsIAtom*             RDFElementImpl::kTooltipAtom;
nsIAtom*             RDFElementImpl::kContextAtom;
PRInt32              RDFElementImpl::kNameSpaceID_RDF;
PRInt32              RDFElementImpl::kNameSpaceID_XUL;

// This is a simple datastructure that maps an event handler attribute
// name to an appropriate IID. Atoms are computed to improve
// comparison efficiency. We do this because SetAttribute() ends up
// being a pretty hot method.
struct EventHandlerMapEntry {
    const char*  mAttributeName;
    nsIAtom*     mAttributeAtom;
    const nsIID* mHandlerIID;
};

static EventHandlerMapEntry kEventHandlerMap[] = {
    { "onclick",       nsnull, &kIDOMMouseListenerIID       },
    { "ondblclick",    nsnull, &kIDOMMouseListenerIID       },
    { "onmousedown",   nsnull, &kIDOMMouseListenerIID       },
    { "onmouseup",     nsnull, &kIDOMMouseListenerIID       },
    { "onmouseover",   nsnull, &kIDOMMouseListenerIID       },
    { "onmouseout",    nsnull, &kIDOMMouseListenerIID       },

    { "onmousemove",   nsnull, &kIDOMMouseMotionListenerIID },

    { "onkeydown",     nsnull, &kIDOMKeyListenerIID         },
    { "onkeyup",       nsnull, &kIDOMKeyListenerIID         },
    { "onkeypress",    nsnull, &kIDOMKeyListenerIID         },

    { "onload",        nsnull, &kIDOMLoadListenerIID        },
    { "onunload",      nsnull, &kIDOMLoadListenerIID        },
    { "onabort",       nsnull, &kIDOMLoadListenerIID        },
    { "onerror",       nsnull, &kIDOMLoadListenerIID        },

    { "oncreate",      nsnull, &kIDOMMenuListenerIID        },
    { "ondestroy",     nsnull, &kIDOMMenuListenerIID        },
    { "onaction",      nsnull, &kIDOMMenuListenerIID        },

    { "onfocus",       nsnull, &kIDOMFocusListenerIID       },
    { "onblur",        nsnull, &kIDOMFocusListenerIID       },

    { "onsubmit",      nsnull, &kIDOMFormListenerIID        },
    { "onreset",       nsnull, &kIDOMFormListenerIID        },
    { "onchange",      nsnull, &kIDOMFormListenerIID        },

    { "onpaint",       nsnull, &kIDOMPaintListenerIID       },

    { nsnull,          nsnull, nsnull                       }
};

////////////////////////////////////////////////////////////////////////
// RDFElementImpl

RDFElementImpl::RDFElementImpl(PRInt32 aNameSpaceID, nsIAtom* aTag)
    : mDocument(nsnull),
      mScriptObject(nsnull),
      mChildren(nsnull),
      mParent(nsnull),
      mNameSpaceID(aNameSpaceID),
      mTag(aTag),
      mListenerManager(nsnull),
      mAttributes(nsnull),
      mContentsMustBeGenerated(PR_FALSE),
      mBroadcastListeners(nsnull),
      mBroadcaster(nsnull),
      mInnerXULElement(nsnull),
      mController(nsnull)
{
    NS_INIT_REFCNT();
    NS_ADDREF(aTag);

    if (gRefCnt++ == 0) {
        nsresult rv;
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          kIRDFServiceIID,
                                          (nsISupports**) &gRDFService);

        NS_VERIFY(NS_SUCCEEDED(rv), "unable to get RDF service");

        kIdAtom          = NS_NewAtom("id");
        kRefAtom         = NS_NewAtom("ref");
        kClassAtom       = NS_NewAtom("class");
        kStyleAtom       = NS_NewAtom("style");
        kLazyContentAtom = NS_NewAtom("lazycontent");
        kTreeAtom        = NS_NewAtom("tree");
        kPopupAtom       = NS_NewAtom("popup");
        kTooltipAtom     = NS_NewAtom("tooltip");
        kContextAtom     = NS_NewAtom("context");

        EventHandlerMapEntry* entry = kEventHandlerMap;
        while (entry->mAttributeName) {
            entry->mAttributeAtom = NS_NewAtom(entry->mAttributeName);
            ++entry;
        }

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
    NS_IF_RELEASE(mAttributes);

    //NS_IF_RELEASE(mDocument); // not refcounted
    //NS_IF_RELEASE(mParent)    // not refcounted
    NS_IF_RELEASE(mTag);
    NS_IF_RELEASE(mListenerManager);

    if (mChildren) {
        // Force child's parent to be null. This ensures that we don't
        // have dangling pointers if a child gets leaked.
        PRUint32 cnt;
        mChildren->Count(&cnt);
        for (PRInt32 i = cnt - 1; i >= 0; --i) {
            nsISupports* isupports = mChildren->ElementAt(i);
            nsCOMPtr<nsIContent> child = do_QueryInterface(isupports);
            NS_RELEASE(isupports);

            child->SetParent(nsnull);
        }
        NS_RELEASE(mChildren);
    }

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

    NS_IF_RELEASE(mController);

    if (--gRefCnt == 0) {
        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }

        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kRefAtom);
        NS_IF_RELEASE(kClassAtom);
        NS_IF_RELEASE(kStyleAtom);
        NS_IF_RELEASE(kLazyContentAtom);
        NS_IF_RELEASE(kTreeAtom);
        NS_IF_RELEASE(kPopupAtom);
        NS_IF_RELEASE(kContextAtom);
        NS_IF_RELEASE(kTooltipAtom);
        NS_IF_RELEASE(gNameSpaceManager);

        EventHandlerMapEntry* entry = kEventHandlerMap;
        while (entry->mAttributeName) {
            NS_IF_RELEASE(entry->mAttributeAtom);
            ++entry;
        }
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
    *aResult = (nsIStyledContent*) element;
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
        *result = NS_STATIC_CAST(nsIStyledContent*, this);
    }
    else if (iid.Equals(nsIXMLContent::GetIID())) {
        *result = NS_STATIC_CAST(nsIXMLContent*, this);
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
    else if (iid.Equals(kIDOMEventTargetIID)) {
        *result = NS_STATIC_CAST(nsIDOMEventTarget*, this);
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
        // XXX This is a mess because of our fun multiple inheritance heirarchy
        nsCOMPtr<nsIContent> root = dont_AddRef( mDocument->GetRootContent() );
        nsCOMPtr<nsIContent> thisIContent;
        QueryInterface(kIContentIID, getter_AddRefs(thisIContent));

        if (root == thisIContent) {
            // If we don't have a parent, and we're the root content
            // of the document, DOM says that our parent is the
            // document.
            return mDocument->QueryInterface(kIDOMNodeIID, (void**)aParentNode);
        }
    }

    // A standalone element (i.e. one without a parent or a document)
    *aParentNode = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
RDFElementImpl::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    nsresult rv;

    nsRDFDOMNodeList* children;
    rv = nsRDFDOMNodeList::Create(&children);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create DOM node list");
    if (NS_FAILED(rv)) return rv;

    PRInt32 count;
    if (NS_SUCCEEDED(rv = ChildCount(count))) {
        for (PRInt32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIContent> child;
            rv = ChildAt(i, *getter_AddRefs(child));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get child");
            if (NS_FAILED(rv))
                break;

            nsCOMPtr<nsIDOMNode> domNode;
            rv = child->QueryInterface(kIDOMNodeIID, (void**) getter_AddRefs(domNode));
            if (NS_FAILED(rv)) {
                NS_WARNING("child content doesn't support nsIDOMNode");
                continue;
            }

            rv = children->AppendNode(domNode);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to append node to list");
            if (NS_FAILED(rv))
                break;
        }
    }

    // Create() addref'd for us
    *aChildNodes = children;
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
        mParent->IndexOf(NS_STATIC_CAST(nsIStyledContent*, this), pos);
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
        mParent->IndexOf(NS_STATIC_CAST(nsIStyledContent*, this), pos);
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
    nsresult rv;
    if (! mAttributes) {
        rv = NS_NewXULAttributes(&mAttributes, NS_STATIC_CAST(nsIStyledContent*, this));
        if (NS_FAILED(rv))
            return rv;
    }

    NS_ADDREF(mAttributes);
    *aAttributes = mAttributes;
    return NS_OK;
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
    if (kNameSpaceID_Unknown == nameSpaceID) {
      nameSpaceID = kNameSpaceID_None;  // ignore unknown prefix XXX is this correct?
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
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsIDOMNamedNodeMap* map;
    nsresult rv = GetAttributes(&map);

    if (NS_SUCCEEDED(rv)) {
        nsIDOMNode* node;
        rv = map->GetNamedItem(aName, &node);
        if (NS_SUCCEEDED(rv) && node) {
            rv = node->QueryInterface(nsIDOMAttr::GetIID(), (void**) aReturn);
            NS_RELEASE(node);
        }
        NS_RELEASE(map);
    }

    return rv;
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
    mNameSpace = dont_QueryInterface(aNameSpace);
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::GetContainingNameSpace(nsINameSpace*& aNameSpace) const
{
    nsresult rv;

    if (mNameSpace) {
        // If we have a namespace, return it.
        aNameSpace = mNameSpace;
        NS_ADDREF(aNameSpace);
        return NS_OK;
    }

    // Next, try our parent.
    nsCOMPtr<nsIContent> parent( dont_QueryInterface(mParent) );
    while (parent) {
        nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(parent) );
        if (xml)
            return xml->GetContainingNameSpace(aNameSpace);

        nsCOMPtr<nsIContent> temp = parent;
        rv = temp->GetParent(*getter_AddRefs(parent));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get parent");
        if (NS_FAILED(rv)) return rv;
    }

    // Allright, we walked all the way to the top of our containment
    // hierarchy and couldn't find a parent that supported
    // nsIXMLContent. If we're in a document, try to doc's root
    // element.
    if (mDocument) {
        nsCOMPtr<nsIContent> docroot
            = dont_AddRef( mDocument->GetRootContent() );

        if (docroot) {
            nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(docroot) );
            if (xml)
                return xml->GetContainingNameSpace(aNameSpace);
        }
    }

    aNameSpace = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::SetNameSpacePrefix(nsIAtom* aNameSpacePrefix)
{
    mNameSpacePrefix = aNameSpacePrefix;
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
                                 PRBool aUseCapture)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    manager->AddEventListenerByType(aListener, aType, flags);
    NS_RELEASE(manager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
RDFElementImpl::RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                    PRBool aUseCapture)
{
  if (nsnull != mListenerManager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

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
            if (tag.Length() >= PRInt32(sizeof buf))
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
        nsCOMPtr<nsIRDFDocument> rdfdoc = do_QueryInterface(mDocument);
        NS_ASSERTION(rdfdoc != nsnull, "ack! not in an RDF document");
        if (rdfdoc) {
            // Need to do a GetResource() here, because changing the document
            // may actually change the element's URI.
            nsCOMPtr<nsIRDFResource> resource;
            GetResource(getter_AddRefs(resource));

            // Remove this element from the RDF resource-to-element map in
            // the old document.
            if (resource) {
                rv = rdfdoc->RemoveElementForResource(resource, NS_STATIC_CAST(nsIStyledContent*, this));
                NS_ASSERTION(NS_SUCCEEDED(rv), "error unmapping resource from element");
            }

            GetRefResource(getter_AddRefs(resource));
            if (resource) {
                rv = rdfdoc->RemoveElementForResource(resource, NS_STATIC_CAST(nsIStyledContent*, this));
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
        nsCOMPtr<nsIRDFDocument> rdfdoc = do_QueryInterface(mDocument);
        NS_ASSERTION(rdfdoc != nsnull, "ack! not in an RDF document");
        if (rdfdoc) {
            // Need to do a GetResource() here, because changing the document
            // may actually change the element's URI.
            nsCOMPtr<nsIRDFResource> resource;
            GetResource(getter_AddRefs(resource));

            // Add this element to the RDF resource-to-element map in the
            // new document.
            if (resource) {
                rv = rdfdoc->AddElementForResource(resource, NS_STATIC_CAST(nsIStyledContent*, this));
                NS_ASSERTION(NS_SUCCEEDED(rv), "error mapping resource to element");
            }

            GetRefResource(getter_AddRefs(resource));
            if (resource) {
                rv = rdfdoc->AddElementForResource(resource, NS_STATIC_CAST(nsIStyledContent*, this));
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
                    if (tag.Length() >= PRInt32(sizeof buf))
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
        PRUint32 cnt;
        rv = mChildren->Count(&cnt);
        if (NS_FAILED(rv)) return rv;
        for (PRInt32 i = cnt - 1; i >= 0; --i) {
            // XXX this entire block could be more rigorous about
            // dealing with failure.
            nsCOMPtr<nsISupports> isupports = dont_AddRef( mChildren->ElementAt(i) );

            NS_ASSERTION(isupports != nsnull, "null ptr");
            if (! isupports)
                continue;

            nsCOMPtr<nsIContent> child = do_QueryInterface(isupports);
            NS_ASSERTION(child != nsnull, "not an nsIContent");
            if (! child)
                continue;

            child->SetDocument(aDocument, aDeep);
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

    if (mChildren) {
        PRUint32 cnt;
        rv = mChildren->Count(&cnt);
        if (NS_FAILED(rv)) return rv;
        aResult = cnt;
    }
    else 
        aResult = 0;
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

    nsCOMPtr<nsISupports> isupports = dont_AddRef( mChildren->ElementAt(aIndex) );
    if (! isupports)
        return NS_OK; // It's okay to ask for an element off the end.

    nsIContent* content;
    rv = isupports->QueryInterface(kIContentIID, (void**) &content);
    if (NS_FAILED(rv)) return rv;

    aResult = content; // take the AddRef() from the QI
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

    // Make sure that we're not trying to insert the same child
    // twice. If we do, the DOM APIs (e.g., GetNextSibling()), will
    // freak out.
    PRInt32 i = mChildren->IndexOf(aKid);
    NS_ASSERTION(i < 0, "element is already a child");
    if (i >= 0)
        return NS_ERROR_FAILURE;

    PRBool insertOk = mChildren->InsertElementAt(aKid, aIndex);/* XXX fix up void array api to use nsresult's*/
    if (insertOk) {
        aKid->SetParent(NS_STATIC_CAST(nsIStyledContent*, this));
        //nsRange::OwnerChildInserted(this, aIndex);
        aKid->SetDocument(mDocument, PR_TRUE);
        if (aNotify && ElementIsInDocument()) {
                mDocument->ContentInserted(NS_STATIC_CAST(nsIStyledContent*, this), aKid, aIndex);
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

    nsCOMPtr<nsISupports> isupports = dont_AddRef( mChildren->ElementAt(aIndex) );
    if (! isupports)
        return NS_OK; // XXX No kid at specified index; just silently ignore?

    nsCOMPtr<nsIContent> oldKid = do_QueryInterface(isupports);
    NS_ASSERTION(oldKid != nsnull, "old kid not nsIContent");
    if (! oldKid)
        return NS_ERROR_FAILURE;

    if (oldKid.get() == aKid)
        return NS_OK;

    // Make sure that we're not trying to insert the same child
    // twice. If we do, the DOM APIs (e.g., GetNextSibling()), will
    // freak out.
    PRInt32 i = mChildren->IndexOf(aKid);
    NS_ASSERTION(i < 0, "element is already a child");
    if (i >= 0)
        return NS_ERROR_FAILURE;

    PRBool replaceOk = mChildren->ReplaceElementAt(aKid, aIndex);
    if (replaceOk) {
        aKid->SetParent(NS_STATIC_CAST(nsIStyledContent*, this));
        //nsRange::OwnerChildReplaced(this, aIndex, oldKid);
        aKid->SetDocument(mDocument, PR_TRUE);
        if (aNotify && ElementIsInDocument()) {
            mDocument->ContentReplaced(NS_STATIC_CAST(nsIStyledContent*, this), oldKid, aKid, aIndex);
        }
        oldKid->SetParent(nsnull);
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    NS_PRECONDITION((nsnull != aKid) && (aKid != NS_STATIC_CAST(nsIStyledContent*, this)), "null ptr");

    if (! mChildren) {
        if (NS_FAILED(NS_NewISupportsArray(&mChildren)))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    // Make sure that we're not trying to insert the same child
    // twice. If we do, the DOM APIs (e.g., GetNextSibling()), will
    // freak out.
    PRInt32 i = mChildren->IndexOf(aKid);
    NS_ASSERTION(i < 0, "element is already a child");
    if (i >= 0)
        return NS_ERROR_FAILURE;

    PRBool appendOk = mChildren->AppendElement(aKid);
    if (appendOk) {
        aKid->SetParent(NS_STATIC_CAST(nsIStyledContent*, this));
        // ranges don't need adjustment since new child is at end of list
        aKid->SetDocument(mDocument, PR_TRUE);
        if (aNotify && ElementIsInDocument()) {
            PRUint32 cnt;
            rv = mChildren->Count(&cnt);
            if (NS_FAILED(rv)) return rv;
            
            mDocument->ContentAppended(NS_STATIC_CAST(nsIStyledContent*, this), cnt - 1);
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

    nsCOMPtr<nsISupports> isupports = dont_AddRef( mChildren->ElementAt(aIndex) );
    if (! isupports)
        return NS_OK; // XXX No kid at specified index; just silently ignore?

    nsCOMPtr<nsIContent> oldKid = do_QueryInterface(isupports);
    NS_ASSERTION(oldKid != nsnull, "old kid not nsIContent");
    if (! oldKid)
        return NS_ERROR_FAILURE;

    if (oldKid) {
        nsIDocument* doc = mDocument;
        PRBool removeOk = mChildren->RemoveElementAt(aIndex);
        //nsRange::OwnerChildRemoved(this, aIndex, oldKid);
        if (aNotify && removeOk && ElementIsInDocument()) {
            doc->ContentRemoved(NS_STATIC_CAST(nsIStyledContent*, this), oldKid, aIndex);
        }
        oldKid->SetParent(nsnull);
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
static char kNameSpaceSeparator = ':';

    nsAutoString prefix;
    nsAutoString name(aStr);
    PRInt32 nsoffset = name.FindChar(kNameSpaceSeparator);
    if (-1 != nsoffset) {
        name.Left(prefix, nsoffset);
        name.Cut(0, nsoffset+1);
    }

    // Figure out the namespace ID, defaulting to none if there is no
    // namespace prefix.
    aNameSpaceID = kNameSpaceID_None;
    if (0 < prefix.Length()) {
        nsCOMPtr<nsIAtom> nameSpaceAtom( getter_AddRefs(NS_NewAtom(prefix)) );
        if (! nameSpaceAtom)
            return NS_ERROR_FAILURE;

        nsresult rv;
        nsCOMPtr<nsINameSpace> ns;
        rv = GetContainingNameSpace(*getter_AddRefs(ns));
        if (NS_FAILED(rv)) return rv;

        if (ns) {
            rv = ns->FindNameSpaceID(nameSpaceAtom, aNameSpaceID);
            if (NS_FAILED(rv)) return rv;
        }
    }

    aName = NS_NewAtom(name);
    return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::GetNameSpacePrefixFromId(PRInt32 aNameSpaceID, 
                                         nsIAtom*& aPrefix)
{
    nsresult rv;

    nsCOMPtr<nsINameSpace> ns;
    rv = GetContainingNameSpace(*getter_AddRefs(ns));
    if (NS_FAILED(rv)) return rv;

    if (ns) {
        return ns->FindNameSpacePrefix(aNameSpaceID, aPrefix);
    }

    aPrefix = nsnull;
    return NS_OK;
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

    nsresult rv = NS_OK;

    if (! mAttributes) {
        rv = NS_NewXULAttributes(&mAttributes, NS_STATIC_CAST(nsIStyledContent*, this));
        if (NS_FAILED(rv))
            return rv;
    }

    // XXX Class and Style attribute setting should be checking for the XUL namespace!

    // Check to see if the CLASS attribute is being set.  If so, we need to rebuild our
    // class list.
    if (mDocument && (aNameSpaceID == kNameSpaceID_None) && aName == kClassAtom) {
      mAttributes->UpdateClassList(aValue);
    }

    // Check to see if the STYLE attribute is being set.  If so, we need to create a new
    // style rule based off the value of this attribute, and we need to let the document
    // know about the StyleRule change.
    if (mDocument && (aNameSpaceID == kNameSpaceID_None) && aName == kStyleAtom) {

        nsIURI* docURL = nsnull;
        if (nsnull != mDocument) {
            mDocument->GetBaseURL(docURL);
        }

        mAttributes->UpdateStyleRule(docURL, aValue);
        // XXX Some kind of special document update might need to happen here.
    }

    // Check to see if the POPUP attribute is being set.  If so, we need to attach
    // a new instance of our popup handler to the node.
    if (mDocument && (aNameSpaceID == kNameSpaceID_None) && 
        (aName == kPopupAtom || aName == kTooltipAtom || aName == kContextAtom))
    {
        // Do a create instance of our popup listener.
        nsIXULPopupListener* popupListener;
        rv = nsComponentManager::CreateInstance(kXULPopupListenerCID,
                                                nsnull,
                                                kIXULPopupListenerIID,
                                                (void**) &popupListener);
        if (NS_FAILED(rv))
        {
          NS_ERROR("Unable to create an instance of the popup listener object.");
          return rv;
        }

        XULPopupType popupType = eXULPopupType_popup;
        if (aName == kTooltipAtom)
          popupType = eXULPopupType_tooltip;
        else if (aName == kContextAtom)
          popupType = eXULPopupType_context;

        // Add a weak reference to the node.
        popupListener->Init(this, popupType);

        // Add the popup as a listener on this element.
        nsCOMPtr<nsIDOMEventListener> eventListener = do_QueryInterface(popupListener);
        AddEventListener("mousedown", eventListener, PR_FALSE);  
        
        NS_IF_RELEASE(popupListener);
    }

    // Check to see if the REF attribute is being set. If so, we need
    // to update the element map.  First, remove the old mapping, if
    // necessary...
    if (mDocument && (aNameSpaceID == kNameSpaceID_None) && (aName == kRefAtom)) {
        nsCOMPtr<nsIRDFDocument> rdfdoc = do_QueryInterface(mDocument);
        if (rdfdoc) {
            nsCOMPtr<nsIRDFResource> resource;

            GetRefResource(getter_AddRefs(resource));
            if (resource) {
                rdfdoc->RemoveElementForResource(resource, NS_STATIC_CAST(nsIStyledContent*, this));
            }
        }
    }

    // XXX need to check if they're changing an event handler: if so, then we need
    // to unhook the old one.
    
    nsXULAttribute* attr;
    PRInt32 i = 0;
    PRInt32 count = mAttributes->Count();
    while (i < count) {
        attr = mAttributes->ElementAt(i);
        if ((aNameSpaceID == attr->mNameSpaceID) && (aName == attr->mName))
            break;
        i++;
    }

    if (i < count) {
        attr->mValue = aValue;
    }
    else { // didn't find it
        rv = NS_NewXULAttribute(&attr, NS_STATIC_CAST(nsIStyledContent*, this), aNameSpaceID, aName, aValue);
        if (NS_FAILED(rv))
            return rv;

        mAttributes->AppendElement(attr);
    }

    // Check for REF attribute, part deux. Add the new REF to the map,
    // if appropriate.
    if (mDocument && (aNameSpaceID == kNameSpaceID_None) && (aName == kRefAtom)) {
        nsCOMPtr<nsIRDFDocument> rdfdoc = do_QueryInterface(mDocument);
        if (rdfdoc) {
            nsCOMPtr<nsIRDFResource> resource;

            GetRefResource(getter_AddRefs(resource));
            if (resource) {
                rdfdoc->AddElementForResource(resource, NS_STATIC_CAST(nsIStyledContent*, this));
            }
        }
    }
        

    // Check for event handlers and add a script listener if necessary.
    EventHandlerMapEntry* entry = kEventHandlerMap;
    while (entry->mAttributeAtom) {
        if (entry->mAttributeAtom == aName) {
            AddScriptEventListener(aName, aValue, *entry->mHandlerIID);
            break;
        }
        ++entry;
    }

    // Notify any broadcasters that are listening to this node.
    if (mBroadcastListeners != nsnull)
    {
        nsAutoString attribute;
        aName->ToString(attribute);
        count = mBroadcastListeners->Count();
        for (i = 0; i < count; i++) {
            XULBroadcastListener* xulListener = (XULBroadcastListener*)mBroadcastListeners->ElementAt(i);
            if ((xulListener->mAttribute == attribute) && (xulListener->mListener != nsnull)) {
                // First we set the attribute in the observer.
                xulListener->mListener->SetAttribute(attribute, aValue);
                ExecuteOnChangeHandler(xulListener->mListener, attribute);
            }
        }
    }

    if (NS_SUCCEEDED(rv) && aNotify && ElementIsInDocument()) {
        mDocument->AttributeChanged(NS_STATIC_CAST(nsIStyledContent*, this), aName, NS_STYLE_HINT_UNKNOWN);
    }

    // Check to see if this is the RDF:container property; if so, and
    // the value is "true", then remember to generate our kids on
    // demand.
    if ((aNameSpaceID == kNameSpaceID_None) &&
        (aName == kLazyContentAtom) &&
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

    // This can happen normally as part of teardown code.
    if (! owner)
        return NS_OK;

    nsAutoString tagStr;
    mTag->ToString(tagStr);

    if (NS_OK == owner->GetScriptContext(&context)) {
      if (tagStr == "window") {
        nsIDOMEventReceiver *receiver;
        nsIScriptGlobalObject *global = context->GetGlobalObject();

        if (nsnull != global && NS_OK == global->QueryInterface(kIDOMEventReceiverIID, (void**)&receiver)) {
          nsIEventListenerManager *manager;
          if (NS_OK == receiver->GetListenerManager(&manager)) {
            nsIScriptObjectOwner *mObjectOwner;
            if (NS_OK == global->QueryInterface(kIScriptObjectOwnerIID, (void**)&mObjectOwner)) {
              ret = manager->AddScriptEventListener(context, mObjectOwner, aName, aValue, aIID);
              NS_RELEASE(mObjectOwner);
            }
            NS_RELEASE(manager);
          }
          NS_RELEASE(receiver);
        }
        NS_IF_RELEASE(global);
      }
      else {
        nsIEventListenerManager *manager;
        if (NS_OK == GetListenerManager(&manager)) {
            nsIScriptObjectOwner* owner2;
            if (NS_OK == QueryInterface(kIScriptObjectOwnerIID,
                                        (void**) &owner2)) {
                ret = manager->AddScriptEventListener(context, owner2,
                                                      aName, aValue, aIID);
                NS_RELEASE(owner2);
            }
            NS_RELEASE(manager);
        }
        NS_RELEASE(context);
      }
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

    if (nsnull != mAttributes) {
        PRInt32 count = mAttributes->Count();
        PRInt32 i;
        for (i = 0; i < count; i++) {
            const nsXULAttribute* attr = (const nsXULAttribute*)mAttributes->ElementAt(i);
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
                  // RDF will treat all document IDs as absolute URIs, so we'll need convert 
                  // a possibly-absolute URI into a relative ID attribute.
                    NS_ASSERTION(mDocument != nsnull, "not initialized");
                    if (nsnull != mDocument) {
                        nsRDFContentUtils::MakeElementID(mDocument, attr->mValue, aResult);
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

        nsIURI* docURL = nsnull;
        if (nsnull != mDocument) {
            mDocument->GetBaseURL(docURL);
        }

        mAttributes->UpdateStyleRule(docURL, "");
        // XXX Some kind of special document update might need to happen here.
    }

    // XXX Know how to remove POPUP event listeners when an attribute is unset?

    nsresult rv = NS_OK;
    PRBool successful = PR_FALSE;
    if (nsnull != mAttributes) {
        PRInt32 count = mAttributes->Count();
        PRInt32 i;
        for (i = 0; i < count; i++) {
            nsXULAttribute* attr = (nsXULAttribute*)mAttributes->ElementAt(i);
            if ((attr->mNameSpaceID == aNameSpaceID) && (attr->mName == aName)) {
                mAttributes->RemoveElementAt(i);
                NS_RELEASE(attr);
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
            nsAutoString str;
            aName->ToString(str);
            if (xulListener->mAttribute == str) {
                // Unset the attribute in the broadcast listener.
                nsCOMPtr<nsIDOMElement> element;
                element = do_QueryInterface(xulListener->mListener);
                if (element)
                  element->RemoveAttribute(str);
            }
        }
      }
   
      // Notify document
      if (NS_SUCCEEDED(rv) && aNotify && (nsnull != mDocument)) {
          mDocument->AttributeChanged(NS_STATIC_CAST(nsIStyledContent*, this),
                                      aName,
                                      NS_STYLE_HINT_UNKNOWN);
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
        nsAutoString as;
        tag->ToString(as);
        fputs(as, out);
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
        aEvent->flags = NS_EVENT_FLAG_NONE;
        // In order for the event to have a proper target for menus (which have no corresponding
        // frame target in the visual model), we have to explicitly set the target of the
        // event to prevent it from trying to retrieve the target from a frame.
        nsAutoString tagName;
        GetTagName(tagName);
        if (tagName == "menu" || tagName == "menuitem" ||
            tagName == "menubar" || tagName == "key" || tagName == "keyset") {
            nsCOMPtr<nsIEventListenerManager> listenerManager;
            if (NS_FAILED(ret = GetListenerManager(getter_AddRefs(listenerManager)))) {
                NS_ERROR("Unable to instantiate a listener manager on a menu/key event.");
                return ret;
            }
            if (NS_FAILED(ret = listenerManager->CreateEvent(aPresContext, aEvent, aDOMEvent))) {
                NS_ERROR("Menu/key event will fail without the ability to create the event early.");
                return ret;
            }
            
            // We need to explicitly set the target here, because the
            // DOM implementation will try to compute the target from
            // the frame. If we don't have a frame (e.g., we're a
            // menu), then that breaks.
            nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(domEvent);
            if (privateEvent) {
              privateEvent->SetTarget(this);
            }
            else return NS_ERROR_FAILURE;
        }
    }
  
    //Capturing stage
    // XXX Needs to be implemented.  Copy from nsGenericElement at some point.
    // Talk to joki@netscape.com for help.
    // For now at least handle document capture.
    if ((NS_EVENT_FLAG_BUBBLE != aFlags) && (mDocument != nsnull)) {
        ret = mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                        NS_EVENT_FLAG_CAPTURE, aEventStatus);
    }

    //Local handling stage
    if (mListenerManager && !(aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH)) {
        aEvent->flags = aFlags;
        mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aFlags, aEventStatus);
    }

    //Bubbling stage
    if ((NS_EVENT_FLAG_CAPTURE != aFlags) && (mParent != nsnull)) {
        // We have a parent. Let them field the event.
        ret = mParent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                      NS_EVENT_FLAG_BUBBLE, aEventStatus);
    }
    else if ((NS_EVENT_FLAG_CAPTURE != aFlags) && (mDocument != nsnull)) {
        // We must be the document root. The event should bubble to the
        // document.
        ret = mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
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
  nsAutoString attrValue;
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


// XXX This _should_ be an implementation method, _not_ publicly exposed :-(
NS_IMETHODIMP
RDFElementImpl::GetResource(nsIRDFResource** aResource)
{
    if (mAttributes) {
        for (PRInt32 i = mAttributes->Count() - 1; i >= 0; --i) {
            const nsXULAttribute* attr = (const nsXULAttribute*) mAttributes->ElementAt(i);
            if ((attr->mNameSpaceID == kNameSpaceID_None) &&
                (attr->mName == kIdAtom)) {
                return gRDFService->GetUnicodeResource(attr->mValue.GetUnicode(), aResource);
            }
        }
    }

    // No resource associated with this element.
    *aResource = nsnull;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
RDFElementImpl::GetRefResource(nsIRDFResource** aResource)
{
    NS_PRECONDITION(mDocument != nsnull, "not initialized");
    if (! mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    if (mAttributes) {
        for (PRInt32 i = mAttributes->Count() - 1; i >= 0; --i) {
            const nsXULAttribute* attr = (const nsXULAttribute*) mAttributes->ElementAt(i);
            if (attr->mNameSpaceID != kNameSpaceID_None)
                continue;

            if (attr->mName != kRefAtom)
                continue;

            // Found it!
            nsresult rv;

            // ...now resolve it to an absolute URI.
            nsCOMPtr<nsIURI> base = dont_AddRef(mDocument->GetDocumentURL());

            nsAutoString uri(attr->mValue);
            rv = rdf_MakeAbsoluteURI(base, uri);
            if (NS_FAILED(rv)) return rv;

            // ...then, setup the new mapping.
            return gRDFService->GetUnicodeResource(uri.GetUnicode(), aResource);
        }
    }

    // If we get here, there was no 'ref' attribute. So return a null resource.
    *aResource = nsnull;
    return NS_OK;
}


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

    rv = rdfDoc->CreateContents(NS_STATIC_CAST(nsIStyledContent*, unconstThis));
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


PRBool
RDFElementImpl::ElementIsInDocument()
{
    // Check to see if the element is really _in_ the document; that
    // is, that it actually is in the tree rooted at the document's
    // root content.
    if (! mDocument)
        return PR_FALSE;

    nsresult rv;

    nsCOMPtr<nsIContent> root = dont_AddRef( mDocument->GetRootContent() );
    if (! root)
        return PR_FALSE;

    // Hack to get off scc's evil-use-of-do_QueryInterface() radar.
    nsIStyledContent* p = NS_STATIC_CAST(nsIStyledContent*, this);
    nsCOMPtr<nsIContent> node = do_QueryInterface(p);

    while (node) {
        if (node == root)
            return PR_TRUE;

        nsCOMPtr<nsIContent> oldNode = node;
        rv = oldNode->GetParent(*getter_AddRefs(node));
        if (NS_FAILED(rv)) return PR_FALSE;
    }

    return PR_FALSE;
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
  nsAutoString value;
  GetAttribute(kNameSpaceID_None, kIdAtom, value);

  aResult = NS_NewAtom(value); // The NewAtom call does the AddRef.
  return NS_OK;
}
    
NS_IMETHODIMP
RDFElementImpl::GetClasses(nsVoidArray& aArray) const
{
	nsresult rv = NS_ERROR_NULL_POINTER;
  if (mAttributes != nsnull)
		rv = mAttributes->GetClasses(aArray);
	return rv;
}

NS_IMETHODIMP 
RDFElementImpl::HasClass(nsIAtom* aClass) const
{
	nsresult rv = NS_ERROR_NULL_POINTER;
	if (mAttributes != nsnull)
		rv = mAttributes->HasClass(aClass);
  return rv;
}

NS_IMETHODIMP
RDFElementImpl::GetContentStyleRules(nsISupportsArray* aRules)
{
  return NS_OK;
}
    
NS_IMETHODIMP
RDFElementImpl::GetInlineStyleRules(nsISupportsArray* aRules)
{
  // Fetch the cached style rule from the attributes.
  nsresult result = NS_ERROR_NULL_POINTER;
  nsIStyleRule* rule = nsnull;
  if (aRules && mAttributes)
    result = mAttributes->GetInlineStyleRule(rule);
  if (rule) {
    aRules->AppendElement(rule);
    NS_RELEASE(rule);
  }
  return result;
}

NS_IMETHODIMP
RDFElementImpl::GetMappedAttributeImpact(const nsIAtom* aAttribute, 
                                         PRInt32& aHint) const
{
  aHint = NS_STYLE_HINT_CONTENT;  // we never map attribtes to style
  return NS_OK;
}

// Controller Methods
NS_IMETHODIMP
RDFElementImpl::GetController(nsIController** aResult)
{
  NS_IF_ADDREF(mController);
  *aResult = mController;
  return NS_OK;
}

NS_IMETHODIMP
RDFElementImpl::SetController(nsIController* aController)
{
  NS_IF_RELEASE(mController);
  mController = aController;
  NS_IF_ADDREF(mController);
  return NS_OK;
}

// Methods for setting/getting attributes from nsIDOMXULElement
nsresult
RDFElementImpl::GetId(nsString& aId)
{
  GetAttribute("id", aId);
  return NS_OK;
}

nsresult
RDFElementImpl::SetId(const nsString& aId)
{
  SetAttribute("id", aId);
  return NS_OK;
}

nsresult
RDFElementImpl::GetClassName(nsString& aClassName)
{
  GetAttribute("class", aClassName);
  return NS_OK;
}

nsresult
RDFElementImpl::SetClassName(const nsString& aClassName)
{
  SetAttribute("class", aClassName);
  return NS_OK;
}

nsresult    
RDFElementImpl::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}
