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

#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIXULPopupListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMFocusListener.h"
#include "nsRDFCID.h"

#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIScriptContextOwner.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMUIEvent.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kXULKeyListenerCID,      NS_XULKEYUPLISTENER_CID);
static NS_DEFINE_IID(kIXULKeyListenerIID,     NS_IXULKEYUPLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIDomNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDomElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDomEventListenerIID,   NS_IDOMEVENTLISTENER_IID);

////////////////////////////////////////////////////////////////////////
// KeyListenerImpl
//
//   This is the key listener implementation for keybinding
//
class nsXULKeyListenerImpl : public nsIXULKeyListener,
                           public nsIDOMKeyListener
{
public:
    nsXULKeyListenerImpl(void);
    virtual nsXULKeyListenerImpl(void);

public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIXULKeyListener
    NS_IMETHOD Init(nsIDOMElement* aElement);

    // nsIDOMKeyListener
    
    /**
     * Processes a key pressed event
     * @param aKeyEvent @see nsIDOMEvent.h 
     * @returns whether the event was consumed or ignored. @see nsresult
     */
    virtual nsresult KeyDown(nsIDOMEvent* aKeyEvent) = 0;

    /**
     * Processes a key release event
     * @param aKeyEvent @see nsIDOMEvent.h 
     * @returns whether the event was consumed or ignored. @see nsresult
     */
    virtual nsresult KeyUp(nsIDOMEvent* aKeyEvent) = 0;

    /**
     * Processes a key typed event
     * @param aKeyEvent @see nsIDOMEvent.h 
     * @returns whether the event was consumed or ignored. @see nsresult
     *
     */
    virtual nsresult KeyPress(nsIDOMEvent* aKeyEvent) = 0;

    // nsIDOMEventListener
    virtual nsresult HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; };

protected:

private:
    nsIDOMElement* element; // Weak reference. The element will go away first.
};

////////////////////////////////////////////////////////////////////////

nsXULKeyListenerImpl::nsXULKeyListenerImpl(void)
{
	NS_INIT_REFCNT();
	
}

nsXULKeyListenerImpl::nsXULKeyListenerImpl(void)
{
}

NS_IMPL_ADDREF(nsXULKeyListenerImpl)
NS_IMPL_RELEASE(nsXULKeyListenerImpl)

NS_IMETHODIMP
nsXULKeyListenerImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(nsIXULKeyListener::GetIID())) {
        *result = NS_STATIC_CAST(nsIXULKeyListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(nsIDOMKeyListener::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMKeyListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(kIDomEventListenerIID)) {
        *result = (nsIDOMEventListener*)(nsIDOMMouseListener*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsXULKeyListenerImpl::Init(nsIDOMElement* aElement)
{
  element = aElement; // Weak reference. Don't addref it.
  return NS_OK;
}

////////////////////////////////////////////////////////////////
// nsIDOMKeyListener

/**
 * Processes a key down event
 * @param aKeyEvent @see nsIDOMEvent.h 
 * @returns whether the event was consumed or ignored. @see nsresult
 */
nsresult nsXULKeyListenerImpl::KeyDown(nsIDOMEvent* aKeyEvent)
{
  nsresult result = NS_OK;
  return result;
}

/**
 * Processes a key release event
 * @param aKeyEvent @see nsIDOMEvent.h 
 * @returns whether the event was consumed or ignored. @see nsresult
 */
nsresult nsXULKeyListenerImpl::KeyUp(nsIDOMEvent* aKeyEvent)
{
  nsresult result = NS_OK;
  return result;
}

/**
 * Processes a key typed event
 * @param aKeyEvent @see nsIDOMEvent.h 
 * @returns whether the event was consumed or ignored. @see nsresult
 *
 */
nsresult nsXULKeyListenerImpl::KeyPress(nsIDOMEvent* aKeyEvent)
{
  nsresult result = NS_OK;
  return result;
}
