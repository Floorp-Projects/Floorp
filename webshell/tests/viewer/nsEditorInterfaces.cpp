/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsEditorInterfaces.h"
#include "nsEditorMode.h"
#include "nsString.h"

/*
 * nsEditorKeyListener implementation
 */

nsEditorKeyListener::nsEditorKeyListener() 
{
}

nsEditorKeyListener::~nsEditorKeyListener() 
{
}

nsresult nsEditorKeyListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kIDOMKeyListenerIID, NS_IDOMKEYLISTENER_IID);
  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMKeyListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMKeyListener*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsEditorKeyListener)

NS_IMPL_RELEASE(nsEditorKeyListener)

nsresult nsEditorKeyListener::ProcessEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult nsEditorKeyListener::GetCharFromKeyCode(PRUint32 aKeyCode, PRBool aIsShift, char *aChar)
{
  /* This is completely temporary to get this working while I check out Unicode conversion code. */
  if (aKeyCode >= 0x41 && aKeyCode <= 0x5A) {
    if (aIsShift) {
      *aChar = (char)aKeyCode;
    }
    else {
      *aChar = (char)(aKeyCode + 0x20);
    }
    return NS_OK;
  }
  else if ((aKeyCode >= 0x30 && aKeyCode <= 0x39) || aKeyCode == 0x20) {
      *aChar = (char)aKeyCode;
      return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsEditorKeyListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  PRUint32 mKeyCode;
  PRBool mIsShift;
  char mChar;
  
  if (NS_OK == aKeyEvent->GetKeyCode(mKeyCode) && 
      NS_OK == aKeyEvent->GetShiftKey(mIsShift)) {
    switch(mKeyCode) {
    case NS_VK_BACK:
        nsDeleteLast();
      break;
    case NS_VK_RETURN:
      // Need to implement creation of either <P> or <BR> nodes.
      break;
    default:
      // XXX Replace with x-platform NS-virtkeycode transform.
      char mChar;
      if (NS_OK == GetCharFromKeyCode(mKeyCode, mIsShift, &mChar)) {
        nsString* key = new nsString();
        *key += mChar;
        if (!mIsShift) {
          key->ToLowerCase();
        }
        nsAppendText(key);
      }
      break;
    }
  }
  
  return NS_ERROR_BASE;
}

nsresult nsEditorKeyListener::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}

nsresult nsEditorKeyListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}

/*
 * nsEditorMouseListener implementation
 */

nsEditorMouseListener::nsEditorMouseListener() 
{
}

nsEditorMouseListener::~nsEditorMouseListener() 
{
}

nsresult nsEditorMouseListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMMouseListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMMouseListener*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsEditorMouseListener)

NS_IMPL_RELEASE(nsEditorMouseListener)

nsresult nsEditorMouseListener::ProcessEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult nsEditorMouseListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  nsIDOMNode *aTarget;
  if (NS_OK == aMouseEvent->GetTarget(&aTarget)) {
    nsSetCurrentNode(aTarget);
    NS_RELEASE(aTarget);
  }
  //Should not be error.  Need a new way to do return values
  return NS_ERROR_BASE;
}

nsresult nsEditorMouseListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult nsEditorMouseListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult nsEditorMouseListener::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult nsEditorMouseListener::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult nsEditorMouseListener::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

/*
 * Factory functions
 */

nsresult NS_NewEditorKeyListener(nsIDOMEventListener ** aInstancePtrResult)
{
  nsEditorKeyListener* it = new nsEditorKeyListener();
  if (NULL == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);

  return it->QueryInterface(kIDOMEventListenerIID, (void **) aInstancePtrResult);   
}

nsresult NS_NewEditorMouseListener(nsIDOMEventListener ** aInstancePtrResult)
{
  nsEditorMouseListener* it = new nsEditorMouseListener();
  if (NULL == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);

  return it->QueryInterface(kIDOMEventListenerIID, (void **) aInstancePtrResult);   
}

