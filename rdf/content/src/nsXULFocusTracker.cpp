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
#include "nsIDOMXULFocusTracker.h"
#include "nsIXULFocusTracker.h"
#include "nsIDOMFocusListener.h"
#include "nsRDFCID.h"

#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIScriptContextOwner.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMUIEvent.h"

#include "nsIDOMXULElement.h"
#include "nsIController.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIDomNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDomElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDomEventListenerIID,   NS_IDOMEVENTLISTENER_IID);

////////////////////////////////////////////////////////////////////////
// XULFocusTrackerImpl
//
//   This is the focus manager for XUL documents.
//
class XULFocusTrackerImpl : public nsIDOMXULFocusTracker,
                            public nsIXULFocusTracker,
                            public nsIDOMFocusListener
{
public:
    XULFocusTrackerImpl(void);
    virtual ~XULFocusTrackerImpl(void);

public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIDOMXULFocusTracker interface
    NS_DECL_IDOMXULFOCUSTRACKER
   
    // nsIDOMFocusListener
    virtual nsresult Focus(nsIDOMEvent* aEvent);
    virtual nsresult Blur(nsIDOMEvent* aEvent);

    // nsIDOMEventListener
    virtual nsresult HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; };

private:
    nsIDOMElement* mCurrentElement; // Weak. The focus must obviously be lost if the node goes away.
    nsVoidArray* mFocusListeners; // Holds weak references to listener elements.
};

////////////////////////////////////////////////////////////////////////

XULFocusTrackerImpl::XULFocusTrackerImpl(void)
{
	NS_INIT_REFCNT();
  mCurrentElement = nsnull;
  mFocusListeners = nsnull;
}

XULFocusTrackerImpl::~XULFocusTrackerImpl(void)
{
  delete mFocusListeners;
}

NS_IMPL_ADDREF(XULFocusTrackerImpl)
NS_IMPL_RELEASE(XULFocusTrackerImpl)

NS_IMETHODIMP
XULFocusTrackerImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(nsIXULFocusTracker::GetIID()) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIXULFocusTracker*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    } 
    else if (iid.Equals(nsIDOMXULFocusTracker::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMXULFocusTracker*, this);
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

    return NS_NOINTERFACE;
}


////////////////////////////////////////////////////////////////
// nsIDOMXULTracker Interface

NS_IMETHODIMP
XULFocusTrackerImpl::GetCurrent(nsIDOMElement** aElement)
{
  NS_IF_ADDREF(mCurrentElement);
  *aElement = mCurrentElement;
  return NS_OK;
}

NS_IMETHODIMP
XULFocusTrackerImpl::SetCurrent(nsIDOMElement* aElement)
{
  mCurrentElement = aElement;
  FocusChanged();
  return NS_OK;
}

NS_IMETHODIMP
XULFocusTrackerImpl::AddFocusListener(nsIDOMElement* aElement)
{
  if (!mFocusListeners) {
    mFocusListeners = new nsVoidArray();
    mFocusListeners->AppendElement((void*)aElement); // Weak ref to element.
  }
  return NS_OK;
}

NS_IMETHODIMP
XULFocusTrackerImpl::RemoveFocusListener(nsIDOMElement* aElement)
{
  if (mFocusListeners) {
    mFocusListeners->RemoveElement((void*)aElement); // Weak ref to element
  }
 
  return NS_OK;
}

NS_IMETHODIMP
XULFocusTrackerImpl::FocusChanged()
{
  if (mFocusListeners) {
    PRInt32 count = mFocusListeners->Count();
    for (PRInt32 i = 0; i < count; i++) {
      printf("Notifying an element of a focus change!\n");
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
XULFocusTrackerImpl::GetController(nsIController** aResult)
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
XULFocusTrackerImpl::SetController(nsIController* aController)
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
XULFocusTrackerImpl::Focus(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMNode> t;
  aEvent->GetTarget(getter_AddRefs(t));
  nsCOMPtr<nsIDOMElement> target = do_QueryInterface(t);
  
  if (target) {
    SetCurrent(target);
    FocusChanged();
  }

  return NS_OK;
}

nsresult 
XULFocusTrackerImpl::Blur(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMNode> t;
  aEvent->GetTarget(getter_AddRefs(t));
  nsCOMPtr<nsIDOMElement> target = do_QueryInterface(t);
  
  if (target.get() == mCurrentElement) {
    SetCurrent(nsnull);
    FocusChanged();
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////
nsresult
NS_NewXULFocusTracker(nsIXULFocusTracker** focusTracker)
{
    XULFocusTrackerImpl* focus = new XULFocusTrackerImpl();
    if (!focus)
      return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(focus);
    *focusTracker = focus;
    return NS_OK;
}
