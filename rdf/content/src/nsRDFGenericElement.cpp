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

  Implementation for a generic element in an RDF content
  model. Unfortunately, no such "generic" nodes exist over in the
  layout world, so we just have to implement our own.

 */


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
#include "nsIScriptObjectOwner.h"
#include "nsISupportsArray.h"
#include "nsRDFContentUtils.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "prlog.h"

// The XUL interfaces implemented by the RDF content node.
#include "nsIDOMXULNode.h"
// End of XUL interface includes

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIContentIID,            NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID,   NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDocumentIID,           NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID,     NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID,  NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

struct XULBroadcastListener
{
	nsString mAttribute;
	nsCOMPtr<nsIDOMNode> mListener;

	XULBroadcastListener(const nsString& attr, nsIDOMNode* listen)
		: mAttribute(attr), mListener(listen)
	{ // Nothing else to do 
	}
};

////////////////////////////////////////////////////////////////////////

class RDFGenericElementImpl : public nsIDOMXULNode,
							  public nsIDOMElement,
                              public nsIDOMEventReceiver,
                              public nsIScriptObjectOwner,
                              public nsIJSScriptObject,
                              public nsIContent
{
public:
    RDFGenericElementImpl(PRInt32 aNameSpaceID, nsIAtom* aTag);
    ~RDFGenericElementImpl(void);

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
    NS_IMETHOD GetNameSpaceID(PRInt32& aResult) const;
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

	// nsIDOMXULNode
	NS_IMETHOD DoCommand();

	NS_IMETHOD AddBroadcastListener(const nsString& attr, nsIDOMNode* aNode);
	NS_IMETHOD RemoveBroadcastListener(const nsString& attr, nsIDOMNode* aNode);

	NS_IMETHOD GetNodeWithID(const nsString& id, nsIDOMNode** aNode);

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

    /** An array of attribute data. Instantiated
        lazily if attributes are required */
    nsVoidArray*      mAttributes;

	/** A pointer to a broadcaster. Only non-null if we are observing someone. **/
	nsIDOMNode*		  mBroadcaster;

	/** An array of broadcast listeners. **/
	nsVoidArray		  mBroadcastListeners;
};



////////////////////////////////////////////////////////////////////////
// RDFGenericElementImpl

nsresult
NS_NewRDFGenericElement(nsIContent** result, PRInt32 aNameSpaceID, nsIAtom* aTag)
{
    NS_PRECONDITION(result, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = NS_STATIC_CAST(nsIContent*, new RDFGenericElementImpl(aNameSpaceID, aTag));
    if (! *result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*result);
    return NS_OK;
}

RDFGenericElementImpl::RDFGenericElementImpl(PRInt32 aNameSpaceID, nsIAtom* aTag)
    : mDocument(nsnull),
      mScriptObject(nsnull),
      mChildren(nsnull),
      mParent(nsnull),
      mNameSpaceID(aNameSpaceID),
      mTag(aTag),
      mAttributes(nsnull),
	  mBroadcaster(nsnull)
{
    NS_INIT_REFCNT();
    NS_ADDREF(aTag);
}

RDFGenericElementImpl::~RDFGenericElementImpl()
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

    //NS_IF_RELEASE(mDocument) // not refcounted
    //NS_IF_RELEASE(mParent)   // not refcounted
    //NS_IF_RELEASE(mScriptObject); XXX don't forget!
    NS_IF_RELEASE(mTag);
    NS_IF_RELEASE(mChildren);

	// Release our broadcast listeners
	PRInt32 count = mBroadcastListeners.Count();
	for (PRInt32 i = 0; i < count; i++)
	{
		XULBroadcastListener* xulListener = (XULBroadcastListener*)mBroadcastListeners[0];
		RemoveBroadcastListener(xulListener->mAttribute, xulListener->mListener);
	}
}

NS_IMPL_ADDREF(RDFGenericElementImpl);
NS_IMPL_RELEASE(RDFGenericElementImpl);

NS_IMETHODIMP 
RDFGenericElementImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIContentIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIContent*, this);
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
RDFGenericElementImpl::GetNodeName(nsString& aNodeName)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::GetNodeValue(nsString& aNodeValue)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFGenericElementImpl::SetNodeValue(const nsString& aNodeValue)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::GetNodeType(PRUint16* aNodeType)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::GetParentNode(nsIDOMNode** aParentNode)
{
    if (!mParent)
        return NS_ERROR_NOT_INITIALIZED;

    return mParent->QueryInterface(kIDOMNodeIID, (void**) aParentNode);
}


NS_IMETHODIMP
RDFGenericElementImpl::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    return NS_NewRDFDOMNodeList(aChildNodes, this);
}


NS_IMETHODIMP
RDFGenericElementImpl::GetFirstChild(nsIDOMNode** aFirstChild)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::GetLastChild(nsIDOMNode** aLastChild)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::GetNextSibling(nsIDOMNode** aNextSibling)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::HasChildNodes(PRBool* aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;

#if 0
    RDFGenericElementImpl* it = new RDFGenericElementImpl();
    if (! it)
        return NS_ERROR_OUT_OF_MEMORY;

    return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
#endif
}


////////////////////////////////////////////////////////////////////////
// nsIDOMElement interface

NS_IMETHODIMP
RDFGenericElementImpl::GetTagName(nsString& aTagName)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::GetAttribute(const nsString& aName, nsString& aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::SetAttribute(const nsString& aName, const nsString& aValue)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::RemoveAttribute(const nsString& aName)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::GetAttributeNode(const nsString& aName, nsIDOMAttr** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::GetElementsByTagName(const nsString& aName, nsIDOMNodeList** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFGenericElementImpl::Normalize()
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}



////////////////////////////////////////////////////////////////////////
// nsIDOMEventReceiver interface

NS_IMETHODIMP
RDFGenericElementImpl::AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFGenericElementImpl::RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFGenericElementImpl::GetListenerManager(nsIEventListenerManager** aInstancePtrResult)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFGenericElementImpl::GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}



////////////////////////////////////////////////////////////////////////
// nsIScriptObjectOwner interface

NS_IMETHODIMP 
RDFGenericElementImpl::GetScriptObject(nsIScriptContext* aContext, void** aScriptObject)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
RDFGenericElementImpl::SetScriptObject(void *aScriptObject)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// nsIJSScriptObject interface

PRBool
RDFGenericElementImpl::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
RDFGenericElementImpl::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
RDFGenericElementImpl::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
RDFGenericElementImpl::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
RDFGenericElementImpl::EnumerateProperty(JSContext *aContext)
{
    return PR_FALSE;
}


PRBool
RDFGenericElementImpl::Resolve(JSContext *aContext, jsval aID)
{
    return PR_FALSE;
}


PRBool
RDFGenericElementImpl::Convert(JSContext *aContext, jsval aID)
{
    return PR_FALSE;
}


void
RDFGenericElementImpl::Finalize(JSContext *aContext)
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
RDFGenericElementImpl::GetDocument(nsIDocument*& aResult) const
{
    aResult = mDocument;
    NS_IF_ADDREF(aResult);
    return NS_OK;
}

NS_IMETHODIMP
RDFGenericElementImpl::SetDocument(nsIDocument* aDocument, PRBool aDeep)
{
    mDocument = aDocument; // not refcounted

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
RDFGenericElementImpl::GetParent(nsIContent*& aResult) const
{
    aResult = mParent;
    NS_IF_ADDREF(mParent);
    return NS_OK;
}

NS_IMETHODIMP
RDFGenericElementImpl::SetParent(nsIContent* aParent)
{
    mParent = aParent; // no refcount
    return NS_OK;
}

NS_IMETHODIMP
RDFGenericElementImpl::CanContainChildren(PRBool& aResult) const
{
    aResult = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
RDFGenericElementImpl::ChildCount(PRInt32& aResult) const
{
    aResult = mChildren ? mChildren->Count() : 0;
    return NS_OK;
}

NS_IMETHODIMP
RDFGenericElementImpl::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
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
RDFGenericElementImpl::IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const
{
    aResult = (mChildren) ? (mChildren->IndexOf(aPossibleChild)) : (-1);
    return NS_OK;
}

// implementation copied from mozilla/layout/base/src/nsGenericElement.cpp
NS_IMETHODIMP
RDFGenericElementImpl::InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify)
{
    NS_PRECONDITION(nsnull != aKid, "null ptr");

    if (! mChildren) {
        if (NS_FAILED(NS_NewISupportsArray(&mChildren)))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    PRBool rv = mChildren->InsertElementAt(aKid, aIndex);/* XXX fix up void array api to use nsresult's*/
    if (rv) {
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

// implementation copied from mozilla/layout/base/src/nsGenericElement.cpp
NS_IMETHODIMP
RDFGenericElementImpl::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify)
{
    NS_PRECONDITION(nsnull != mChildren, "illegal value");
    if (! mChildren)
        return NS_ERROR_ILLEGAL_VALUE;

    NS_PRECONDITION(nsnull != aKid, "null ptr");
    if (! aKid)
        return NS_ERROR_NULL_POINTER;

    nsIContent* oldKid = (nsIContent *)mChildren->ElementAt(aIndex);
    PRBool rv = mChildren->ReplaceElementAt(aKid, aIndex);
    if (rv) {
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

// implementation copied from mozilla/layout/base/src/nsGenericElement.cpp
NS_IMETHODIMP
RDFGenericElementImpl::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
    NS_PRECONDITION((nsnull != aKid) && (aKid != this), "null ptr");

    if (! mChildren) {
        if (NS_FAILED(NS_NewISupportsArray(&mChildren)))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    PRBool rv = mChildren->AppendElement(aKid);
    if (rv) {
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

// implementation copied from mozilla/layout/base/src/nsGenericElement.cpp
NS_IMETHODIMP
RDFGenericElementImpl::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
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
RDFGenericElementImpl::IsSynthetic(PRBool& aResult)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
RDFGenericElementImpl::GetNameSpaceID(PRInt32& aNameSpaceID) const
{
    aNameSpaceID = mNameSpaceID;
    return NS_OK;
}

NS_IMETHODIMP
RDFGenericElementImpl::GetTag(nsIAtom*& aResult) const
{
    aResult = mTag;
    NS_ADDREF(aResult);
    return NS_OK;
}

NS_IMETHODIMP 
RDFGenericElementImpl::ParseAttributeString(const nsString& aStr, 
                                            nsIAtom*& aName, 
                                            PRInt32& aNameSpaceID)
{
    // XXX Need to implement
    aName = nsnull;
    aNameSpaceID = kNameSpaceID_None;
    return NS_OK;
}

NS_IMETHODIMP
RDFGenericElementImpl::GetNameSpacePrefix(PRInt32 aNameSpaceID, 
                                          nsIAtom*& aPrefix)
{
    // XXX Need to implement
    aPrefix = nsnull;
    return NS_OK;
}

// XXX attribute code swiped from nsGenericContainerElement
// this class could probably just use nsGenericContainerElement
// needed to maintain attribute namespace ID as well as ordering
NS_IMETHODIMP 
RDFGenericElementImpl::SetAttribute(PRInt32 aNameSpaceID,
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
	PRBool successful = PR_FALSE;
    PRInt32 index;
    PRInt32 count = mAttributes->Count();
    for (index = 0; index < count; index++) {
        attr = (nsGenericAttribute*)mAttributes->ElementAt(index);
        if ((aNameSpaceID == attr->mNameSpaceID) && (aName == attr->mName)) {
            attr->mValue = aValue;
            rv = NS_OK;
			successful = PR_TRUE;
            break;
        }
    }
    
    if (index >= count) { // didn't find it
        attr = new nsGenericAttribute(aNameSpaceID, aName, aValue);
        if (nsnull != attr) {
            mAttributes->AppendElement(attr);
            rv = NS_OK;
			successful = PR_TRUE;
        }
    }

	// XUL Only. Find out if we have a broadcast listener for this element.
	if (successful)
	{
		count = mBroadcastListeners.Count();
		for (PRInt32 i = 0; i < count; i++)
		{
			XULBroadcastListener* xulListener = (XULBroadcastListener*)mBroadcastListeners[i];
			nsString aString;
			aName->ToString(aString);
			if (xulListener->mAttribute.EqualsIgnoreCase(aString))
			{
				// Set the attribute in the broadcast listener.
				nsCOMPtr<nsIContent> contentNode(xulListener->mListener);
				if (contentNode)
				{
					contentNode->SetAttribute(aNameSpaceID, aName, aValue, aNotify);
				}
			}
		}
	}
	// End XUL Only Code

    // XXX notify doc?
    return rv;
}


NS_IMETHODIMP
RDFGenericElementImpl::GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, nsString& aResult) const
{
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    if (nsnull == aName) {
        return NS_ERROR_NULL_POINTER;
    }

    nsresult rv = NS_CONTENT_ATTR_NOT_THERE;

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
RDFGenericElementImpl::UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify)
{
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    if (nsnull == aName) {
        return NS_ERROR_NULL_POINTER;
    }

    nsresult rv = NS_OK;
	PRBool successful = PR_FALSE;
    if (nsnull != mAttributes) {
        PRInt32 count = mAttributes->Count();
        PRInt32 index;
        for (index = 0; index < count; index++) {
            nsGenericAttribute* attr = (nsGenericAttribute*)mAttributes->ElementAt(index);
            if ((attr->mNameSpaceID == aNameSpaceID) && (attr->mName == aName)) {
                mAttributes->RemoveElementAt(index);
                delete attr;
				successful = PR_TRUE;
                break;
            }
        }

        // XXX notify document??
    }

	// XUL Only. Find out if we have a broadcast listener for this element.
	if (successful)
	{
		PRInt32 count = mBroadcastListeners.Count();
		for (PRInt32 i = 0; i < count; i++)
		{
			XULBroadcastListener* xulListener = (XULBroadcastListener*)mBroadcastListeners[i];
			nsString aString;
			aName->ToString(aString);
			if (xulListener->mAttribute.EqualsIgnoreCase(aString))
			{
				// Set the attribute in the broadcast listener.
				nsCOMPtr<nsIContent> contentNode(xulListener->mListener);
				if (contentNode)
				{
					contentNode->UnsetAttribute(aNameSpaceID, aName, aNotify);
				}
			}
		}
	}
	// End XUL Only Code

    return rv;
}

NS_IMETHODIMP
RDFGenericElementImpl::GetAttributeNameAt(PRInt32 aIndex,
                                          PRInt32& aNameSpaceID, 
                                          nsIAtom*& aName) const
{
    if (nsnull != mAttributes) {
        nsGenericAttribute* attr = (nsGenericAttribute*)mAttributes->ElementAt(aIndex);
        if (nsnull != attr) {
            aNameSpaceID = attr->mNameSpaceID;
            aName = attr->mName;
            NS_IF_ADDREF(aName);
            return NS_OK;
        }
    }
    aNameSpaceID = kNameSpaceID_None;
    aName = nsnull;
    return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
RDFGenericElementImpl::GetAttributeCount(PRInt32& aResult) const
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
RDFGenericElementImpl::List(FILE* out, PRInt32 aIndent) const
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
RDFGenericElementImpl::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFGenericElementImpl::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFGenericElementImpl::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFGenericElementImpl::SizeOf(nsISizeOfHandler* aHandler) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP 
RDFGenericElementImpl::HandleDOMEvent(nsIPresContext& aPresContext,
                             nsEvent* aEvent,
                             nsIDOMEvent** aDOMEvent,
                             PRUint32 aFlags,
                             nsEventStatus& aEventStatus)
{
    return NS_OK;
}



NS_IMETHODIMP 
RDFGenericElementImpl::RangeAdd(nsIDOMRange& aRange) 
{  
    // rdf content does not yet support DOM ranges
    return NS_OK;
}


 
NS_IMETHODIMP 
RDFGenericElementImpl::RangeRemove(nsIDOMRange& aRange) 
{
    // rdf content does not yet support DOM ranges
    return NS_OK;
}                                                                        


NS_IMETHODIMP 
RDFGenericElementImpl::GetRangeList(nsVoidArray*& aResult) const
{
    // rdf content does not yet support DOM ranges
    return NS_OK;
}

// nsIDOMXULNode
NS_IMETHODIMP
RDFGenericElementImpl::DoCommand()
{
	return NS_OK;
}

NS_IMETHODIMP
RDFGenericElementImpl::AddBroadcastListener(const nsString& attr, nsIDOMNode* aNode) 
{ 
	// Add ourselves to the array.
	NS_ADDREF(aNode);
	mBroadcastListeners.AppendElement(new XULBroadcastListener(attr, aNode));

	// XXX: Sync up the initial attribute value.

	return NS_OK; 
}
	

NS_IMETHODIMP
RDFGenericElementImpl::RemoveBroadcastListener(const nsString& attr, nsIDOMNode* aNode) 
{ 
	// Find the node.
	PRInt32 count = mBroadcastListeners.Count();
	for (PRInt32 i = 0; i < count; i++)
	{
		XULBroadcastListener* xulListener = (XULBroadcastListener*)mBroadcastListeners[i];
		
		if (xulListener->mAttribute == attr &&
			xulListener->mListener == aNode)
		{
			// Do the removal.
			mBroadcastListeners.RemoveElementAt(i);
			delete xulListener;
			return NS_OK;
		}
	}

	return NS_OK;
}

NS_IMETHODIMP
RDFGenericElementImpl::GetNodeWithID(const nsString& id, nsIDOMNode** node)
{
	return NS_OK;
}