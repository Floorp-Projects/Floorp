/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsWidgetListener.h"
#include "nsIDOMEventReceiver.h"

static NS_DEFINE_IID(kIWidgetControllerIID, NS_IWIDGETCONTROLLER_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID,   NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMFormListenerIID,  NS_IDOMFORMLISTENER_IID);
static NS_DEFINE_IID(kIDOMLoadListenerIID,  NS_IDOMLOADLISTENER_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kISupportsIID,         NS_ISUPPORTS_IID);





nsWidgetListener::nsWidgetListener()
{
  NS_INIT_REFCNT();
}


nsWidgetListener::~nsWidgetListener()
{
}

/* Implement ISupports methods... */
NS_IMPL_ADDREF(nsWidgetListener);
NS_IMPL_RELEASE(nsWidgetListener);

nsresult
nsWidgetListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv = NS_NOINTERFACE;

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIWidgetController*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  } else if (aIID.Equals(kIWidgetControllerIID)) {
    *aInstancePtr = (void*)(nsIWidgetController*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  } else if (aIID.Equals(kIDOMMouseListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMMouseListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  } else if (aIID.Equals(kIDOMKeyListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMKeyListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  } else if (aIID.Equals(kIDOMFormListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMFormListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  } else if (aIID.Equals(kIDOMLoadListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMLoadListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return rv;
}


nsresult nsWidgetListener::AddEventListener(nsISupports* aNode, 
                                            const nsIID& aInterfaceIID)
{
  nsresult rv;
  nsIDOMEventReceiver* receiver;

  if (nsnull != aNode) {
    rv = aNode->QueryInterface(kIDOMEventReceiverIID, (void**) &receiver);
    if (NS_SUCCEEDED(rv)) {
      receiver->AddEventListenerByIID((nsIDOMEventListener*)(nsIDOMMouseListener*)this, 
                                 aInterfaceIID);
      NS_RELEASE(receiver);
    }
  } else {
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}


//-----------------------------------------------------------------
// nsIDOMMouseListener interface...
//
nsresult nsWidgetListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult nsWidgetListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult nsWidgetListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult nsWidgetListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult nsWidgetListener::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult nsWidgetListener::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult nsWidgetListener::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMKeyListener interface...
//
nsresult nsWidgetListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}

nsresult nsWidgetListener::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}

nsresult nsWidgetListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMFormListener interface...
//
nsresult nsWidgetListener::Submit(nsIDOMEvent* aFormEvent)
{
  return NS_OK;
}

nsresult nsWidgetListener::Reset(nsIDOMEvent* aFormEvent)
{
  return NS_OK;
}

nsresult nsWidgetListener::Change(nsIDOMEvent* aFormEvent)
{
  return NS_OK;
}


//----------------------------------------------------------------------
// nsIDOMLoadListener interface...
//
nsresult nsWidgetListener::Load(nsIDOMEvent* aLoadEvent)
{
  return NS_OK;
}

nsresult nsWidgetListener::Unload(nsIDOMEvent* aLoadEvent)
{
  return NS_OK;
}

nsresult nsWidgetListener::Abort(nsIDOMEvent* aLoadEvent)
{
  return NS_OK;
}

nsresult nsWidgetListener::Error(nsIDOMEvent* aLoadEvent)
{
  return NS_OK;
}

