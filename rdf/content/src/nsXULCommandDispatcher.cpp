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

private:
    void*                      mScriptObject;       // ????

    nsIDOMElement* mCurrentElement; // Weak. The focus must obviously be lost if the node goes away.
    nsVoidArray* mFocusListeners; // Holds weak references to listener elements.
};

////////////////////////////////////////////////////////////////////////

XULCommandDispatcherImpl::XULCommandDispatcherImpl(void)
:mScriptObject(nsnull)
{
	NS_INIT_REFCNT();
  mCurrentElement = nsnull;
  mFocusListeners = nsnull;
}

XULCommandDispatcherImpl::~XULCommandDispatcherImpl(void)
{
  delete mFocusListeners;
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
  NS_IF_ADDREF(mCurrentElement);
  *aElement = mCurrentElement;
  return NS_OK;
}

NS_IMETHODIMP
XULCommandDispatcherImpl::SetFocusedElement(nsIDOMElement* aElement)
{
  mCurrentElement = aElement;
  UpdateCommands();
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
XULCommandDispatcherImpl::AddCommand(nsIDOMElement* aElement)
{
  if (!mFocusListeners) {
    mFocusListeners = new nsVoidArray();
    mFocusListeners->AppendElement((void*)aElement); // Weak ref to element.
  }
  return NS_OK;
}

NS_IMETHODIMP
XULCommandDispatcherImpl::RemoveCommand(nsIDOMElement* aElement)
{
  if (mFocusListeners) {
    mFocusListeners->RemoveElement((void*)aElement); // Weak ref to element
  }
 
  return NS_OK;
}

NS_IMETHODIMP
XULCommandDispatcherImpl::UpdateCommands()
{
  if (mFocusListeners) {
    PRInt32 count = mFocusListeners->Count();
    for (PRInt32 i = 0; i < count; i++) {
      nsIDOMElement* domElement = (nsIDOMElement*)mFocusListeners->ElementAt(i);

      nsCOMPtr<nsIContent> content;
      content = do_QueryInterface(domElement);
    
      nsCOMPtr<nsIDocument> document;
      content->GetDocument(*getter_AddRefs(document));

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
    UpdateCommands();
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
    UpdateCommands();
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
