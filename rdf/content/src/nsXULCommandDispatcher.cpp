/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
  This file provides the implementation for the sort service manager.
 */

#include "nsCOMPtr.h"

#include "nsVoidArray.h"

#include "nsIDOMElement.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIXULCommandDispatcher.h"
#include "nsIDOMFocusListener.h"
#include "nsRDFCID.h"

#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIScriptContextOwner.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMUIEvent.h"

#include "nsIDOMXULElement.h"
#include "nsIController.h"

#include "nsIPresContext.h"
#include "nsIPresShell.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIScriptObjectOwnerIID,      NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIDomNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDomElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDomEventListenerIID,   NS_IDOMEVENTLISTENER_IID);

////////////////////////////////////////////////////////////////////////
// XULCommandDispatcherImpl
//
//   This is the focus manager for XUL documents.
//
class XULCommandDispatcherImpl : public nsIDOMXULCommandDispatcher,
                                 public nsIXULCommandDispatcher,
                                 public nsIDOMFocusListener,
                                 public nsIScriptObjectOwner
{
public:
    XULCommandDispatcherImpl(void);
    virtual ~XULCommandDispatcherImpl(void);

public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIDOMXULCommandDispatcher interface
    NS_DECL_IDOMXULCOMMANDDISPATCHER
   
    // nsIDOMFocusListener
    virtual nsresult Focus(nsIDOMEvent* aEvent);
    virtual nsresult Blur(nsIDOMEvent* aEvent);

    // nsIDOMEventListener
    virtual nsresult HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; };

    // nsIScriptObjectOwner interface
    NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD SetScriptObject(void *aScriptObject);

protected:
    void*                      mScriptObject;       // ????

    // XXX THis was supposed to be WEAK, but c'mon, that's an accident
    // waiting to happen! If somebody deletes the node, then asks us
    // for the focus, we'll get killed!
    nsCOMPtr<nsIDOMElement> mCurrentElement; // [OWNER]

    class Updater {
    public:
        Updater(nsIDOMElement* aElement,
                const nsString& aEvents,
                const nsString& aTargets)
            : mElement(aElement),
              mEvents(aEvents),
              mTargets(aTargets),
              mNext(nsnull)
        {}

        nsIDOMElement* mElement; // [WEAK]
        nsString       mEvents;
        nsString       mTargets;
        Updater*       mNext;
    };

    Updater* mUpdaters;

    PRBool Matches(const nsString& aList, const nsString& aElement);
};

////////////////////////////////////////////////////////////////////////

XULCommandDispatcherImpl::XULCommandDispatcherImpl(void)
    : mScriptObject(nsnull), mCurrentElement(nsnull), mUpdaters(nsnull)
{
	NS_INIT_REFCNT();
}

XULCommandDispatcherImpl::~XULCommandDispatcherImpl(void)
{
    while (mUpdaters) {
        Updater* doomed = mUpdaters;
        mUpdaters = mUpdaters->mNext;
        delete doomed;
    }
}

NS_IMPL_ADDREF(XULCommandDispatcherImpl)
NS_IMPL_RELEASE(XULCommandDispatcherImpl)

NS_IMETHODIMP
XULCommandDispatcherImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kISupportsIID)) {
        *result = (nsISupports*)(nsIXULCommandDispatcher*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(nsIXULCommandDispatcher::GetIID())) {
        *result = NS_STATIC_CAST(nsIXULCommandDispatcher*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    } 
    else if (iid.Equals(nsIDOMXULCommandDispatcher::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMXULCommandDispatcher*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(nsIDOMFocusListener::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMFocusListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(kIDomEventListenerIID)) {
        *result = (nsIDOMEventListener*)(nsIDOMFocusListener*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(kIScriptObjectOwnerIID)) {
        *result = NS_STATIC_CAST(nsIScriptObjectOwner*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}


////////////////////////////////////////////////////////////////
// nsIDOMXULTracker Interface

NS_IMETHODIMP
XULCommandDispatcherImpl::GetFocusedElement(nsIDOMElement** aElement)
{
  *aElement = mCurrentElement;
  NS_IF_ADDREF(*aElement);
  return NS_OK;
}

NS_IMETHODIMP
XULCommandDispatcherImpl::SetFocusedElement(nsIDOMElement* aElement)
{
  mCurrentElement = aElement;
  UpdateCommands(nsAutoString(aElement ? "focus" : "blur"));
  return NS_OK;
}

NS_IMETHODIMP
XULCommandDispatcherImpl::GetFocusedWindow(nsIDOMWindow** aElement)
{
  // XXX Implement 
  return NS_OK;
}

NS_IMETHODIMP
XULCommandDispatcherImpl::SetFocusedWindow(nsIDOMWindow* aElement)
{
  // XXX Implement
  return NS_OK;
}

NS_IMETHODIMP
XULCommandDispatcherImpl::AddCommandUpdater(nsIDOMElement* aElement,
                                            const nsString& aEvents,
                                            const nsString& aTargets)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    Updater* updater = mUpdaters;
    Updater** link = &mUpdaters;

    while (updater) {
        if (updater->mElement == aElement) {
            // If the updater was already in the list, then replace
            // (?) the 'events' and 'targets' filters with the new
            // specification.
            updater->mEvents  = aEvents;
            updater->mTargets = aTargets;
            return NS_OK;
        }

        link = &(updater->mNext);
        updater = updater->mNext;
    }

    // If we get here, this is a new updater. Append it to the list.
    updater = new Updater(aElement, aEvents, aTargets);
    if (! updater)
        return NS_ERROR_OUT_OF_MEMORY;

    *link = updater;
    return NS_OK;
}

NS_IMETHODIMP
XULCommandDispatcherImpl::RemoveCommandUpdater(nsIDOMElement* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    Updater* updater = mUpdaters;
    Updater** link = &mUpdaters;

    while (updater) {
        if (updater->mElement == aElement) {
            *link = updater->mNext;
            delete updater;
            return NS_OK;
        }

        link = &(updater->mNext);
        updater = updater->mNext;
    }

    // Hmm. Not found. Oh well.
    return NS_OK;
}

NS_IMETHODIMP
XULCommandDispatcherImpl::UpdateCommands(const nsString& aEventName)
{
    nsresult rv;

    nsAutoString id;
    if (mCurrentElement) {
        rv = mCurrentElement->GetAttribute(nsAutoString("id"), id);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get element's id");
        if (NS_FAILED(rv)) return rv;
    }

    for (Updater* updater = mUpdaters; updater != nsnull; updater = updater->mNext) {
        // Skip any nodes that don't match our 'events' or 'targets'
        // filters.
        if (! Matches(updater->mEvents, aEventName))
            continue;

        if (! Matches(updater->mTargets, id))
            continue;

        nsCOMPtr<nsIContent> content = do_QueryInterface(updater->mElement);
        NS_ASSERTION(content != nsnull, "not an nsIContent");
        if (! content)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIDocument> document;
        rv = content->GetDocument(*getter_AddRefs(document));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get document");
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(document != nsnull, "element has no document");
        if (! document)
            continue;

        PRInt32 count = document->GetNumberOfShells();
        for (PRInt32 i = 0; i < count; i++) {
            nsCOMPtr<nsIPresShell> shell = dont_AddRef(document->GetShellAt(i));
            if (! shell)
                continue;

            // Retrieve the context in which our DOM event will fire.
            nsCOMPtr<nsIPresContext> context;
            rv = shell->GetPresContext(getter_AddRefs(context));
            if (NS_FAILED(rv)) return rv;

            // Handle the DOM event
            nsEventStatus status = nsEventStatus_eIgnore;
            nsEvent event;
            event.eventStructType = NS_EVENT;
            event.message = NS_FORM_CHANGE; // XXX: I feel dirty and evil for subverting this.
            content->HandleDOMEvent(*context, &event, nsnull, NS_EVENT_FLAG_INIT, status);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
XULCommandDispatcherImpl::GetController(nsIController** aResult)
{
  if (mCurrentElement) {
    nsCOMPtr<nsIDOMXULElement> xulElement = do_QueryInterface(mCurrentElement);
    if (xulElement)
      return xulElement->GetController(aResult);
  }

  *aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
XULCommandDispatcherImpl::SetController(nsIController* aController)
{
  if (mCurrentElement) {
    nsCOMPtr<nsIDOMXULElement> xulElement = do_QueryInterface(mCurrentElement);
    if (xulElement)
      return xulElement->SetController(aController);
  }

  return NS_OK;
}

/////
// nsIDOMFocusListener
/////

nsresult 
XULCommandDispatcherImpl::Focus(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMNode> t;
  aEvent->GetTarget(getter_AddRefs(t));
  nsCOMPtr<nsIDOMElement> target = do_QueryInterface(t);
  
  if (target) {
    SetFocusedElement(target);
    UpdateCommands("focus");
  }

  return NS_OK;
}

nsresult 
XULCommandDispatcherImpl::Blur(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMNode> t;
  aEvent->GetTarget(getter_AddRefs(t));
  nsCOMPtr<nsIDOMElement> target = do_QueryInterface(t);
  
  if (target.get() == mCurrentElement) {
    SetFocusedElement(nsnull);
    UpdateCommands("blur");
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIScriptObjectOwner interface
NS_IMETHODIMP
XULCommandDispatcherImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    nsresult res = NS_OK;
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();

    if (nsnull == mScriptObject) {
        res = NS_NewScriptXULCommandDispatcher(aContext, (nsISupports *)(nsIDOMXULCommandDispatcher*)this, global, (void**)&mScriptObject);
    }
    *aScriptObject = mScriptObject;

    NS_RELEASE(global);
    return res;
}


NS_IMETHODIMP
XULCommandDispatcherImpl::SetScriptObject(void *aScriptObject)
{
    mScriptObject = aScriptObject;
    return NS_OK;
}


PRBool
XULCommandDispatcherImpl::Matches(const nsString& aList, const nsString& aElement)
{
    if (aList == "*")
        return PR_TRUE; // match _everything_!

    PRInt32 indx = aList.Find(aElement);
    if (indx == -1)
        return PR_FALSE; // not in the list at all

    // okay, now make sure it's not a substring snafu; e.g., 'ur'
    // found inside of 'blur'.
    if (indx > 0) {
        PRUnichar ch = aList[indx - 1];
        if (! nsString::IsSpace(ch) && ch != PRUnichar(','))
            return PR_FALSE;
    }

    if (indx + aElement.Length() < aList.Length()) {
        PRUnichar ch = aList[indx + aElement.Length()];
        if (! nsString::IsSpace(ch) && ch != PRUnichar(','))
            return PR_FALSE;
    }

    return PR_TRUE;
}


////////////////////////////////////////////////////////////////
nsresult
NS_NewXULCommandDispatcher(nsIXULCommandDispatcher** CommandDispatcher)
{
    XULCommandDispatcherImpl* focus = new XULCommandDispatcherImpl();
    if (!focus)
      return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(focus);
    *CommandDispatcher = focus;
    return NS_OK;
}
