/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

#include "nsBaseDialog.h"
#include "nsIDOMEvent.h"
#include "nsIXPBaseWindow.h"

#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLDocument.h"
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

//-------------------------------------------------------------------------
//
// nsBaseDialog constructor
//
//-------------------------------------------------------------------------
//-----------------------------------------------------------------
nsBaseDialog::nsBaseDialog(nsBrowserWindow * aBrowserWindow) :
  mBrowserWindow(aBrowserWindow),
  mWindow(nsnull),
  mCancelBtn(nsnull)
{
}

//-----------------------------------------------------------------
nsBaseDialog::~nsBaseDialog()
{
  NS_IF_RELEASE(mWindow);
}

//---------------------------------------------------------------
void nsBaseDialog::Initialize(nsIXPBaseWindow * aWindow) 
{
  mWindow = aWindow;
  NS_ADDREF(mWindow);

  nsIDOMHTMLDocument *doc = nsnull;
  mWindow->GetDocument(doc);
  if (nsnull != doc) {
    doc->GetElementById("cancel", &mCancelBtn);
    if (nsnull != mCancelBtn) {
      mWindow->AddEventListener(mCancelBtn);
    }
    NS_RELEASE(doc);
  }
}


//-----------------------------------------------------------------
void nsBaseDialog::Destroy(nsIXPBaseWindow * aWindow)
{

  // Unregister event listeners that were registered in the
  // Initialize here. 
  // XXX: Should change code in XPBaseWindow to automatically unregister
  // all event listening, That way this code will not be necessary.
  if (nsnull != mCancelBtn) {
    aWindow->RemoveEventListener(mCancelBtn);
  }
}

//-----------------------------------------------------------------
void nsBaseDialog::MouseClick(nsIDOMEvent* aMouseEvent, nsIXPBaseWindow * aWindow, PRBool &aStatus)
{
   // Event Dispatch. This method should not contain
   // anything but calls to methods. This idea is that this dispatch
   // mechanism may be replaced by JavaScript EventHandlers which call the idl'ed
   // interfaces to perform the same operation that is currently being handled by
   // this C++ code.

  aStatus = PR_FALSE;

  nsIDOMNode * node;
  aMouseEvent->GetTarget(&node);
  if (node == mCancelBtn) {
    DoClose();
    aStatus = PR_TRUE;
  }
  NS_RELEASE(node);

}

//---------------------------------------------------------------
void
nsBaseDialog::DoClose()
{
  mWindow->SetVisible(PR_FALSE);
}


//---------------------------------------------------------------
PRBool 
nsBaseDialog::IsChecked(nsIDOMElement * aNode)
{
  nsIDOMHTMLInputElement * element;
  if (NS_OK == aNode->QueryInterface(kIDOMHTMLInputElementIID, (void**) &element)) {
    PRBool checked;
    element->GetChecked(&checked);
    NS_RELEASE(element);
    return checked;
  }
  return PR_FALSE;
}

//-----------------------------------------------------------------
PRBool 
nsBaseDialog::IsChecked(const nsString & aName)
{
  nsIDOMElement      * node;
  nsIDOMHTMLDocument * doc = nsnull;
  mWindow->GetDocument(doc);
  if (nsnull != doc) {
    if (NS_OK == doc->GetElementById(aName, &node)) {
      PRBool value = IsChecked(node);
      NS_RELEASE(node);
      NS_RELEASE(doc);
      return value;
    }
    NS_RELEASE(doc);
  }
  return PR_FALSE;
}

//---------------------------------------------------------------
void  
nsBaseDialog::SetChecked(nsIDOMElement * aNode, PRBool aValue)
{
  nsIDOMHTMLInputElement * element;
  if (NS_OK == aNode->QueryInterface(kIDOMHTMLInputElementIID, (void**) &element)) {
    element->SetChecked(aValue);
    NS_RELEASE(element);
  }
}

//---------------------------------------------------------------
void  
nsBaseDialog::SetChecked(const nsString & aName, PRBool aValue)
{
  nsIDOMElement      * node;
  nsIDOMHTMLDocument * doc = nsnull;
  mWindow->GetDocument(doc);
  if (nsnull != doc) {
    if (NS_OK == doc->GetElementById(aName,   &node)) {
      SetChecked(node, aValue);
      NS_RELEASE(node);
    }
    NS_RELEASE(doc);
  }
}

//-----------------------------------------------------------------
void nsBaseDialog::GetText(nsIDOMElement * aNode, nsString & aStr)
{
  nsIDOMHTMLInputElement * element;
  if (NS_OK == aNode->QueryInterface(kIDOMHTMLInputElementIID, (void**) &element)) {
    element->GetValue(aStr);
    NS_RELEASE(element);
  }
}

//-----------------------------------------------------------------
void nsBaseDialog::GetText(const nsString & aName, nsString & aStr)
{
  nsIDOMElement      * node;
  nsIDOMHTMLDocument * doc = nsnull;
  mWindow->GetDocument(doc);
  if (nsnull != doc) {
    if (NS_OK == doc->GetElementById(aName,   &node)) {
      GetText(node, aStr);
      NS_RELEASE(node);
    }
    NS_RELEASE(doc);
  }
}

//---------------------------------------------------------------
void  
nsBaseDialog::SetText(nsIDOMElement * aNode, const nsString &aValue)
{
  nsIDOMHTMLInputElement * element;
  if (NS_OK == aNode->QueryInterface(kIDOMHTMLInputElementIID, (void**) &element)) {
    element->SetValue(aValue);
    NS_RELEASE(element);
  }
}


//-----------------------------------------------------------------
void nsBaseDialog::SetText(const nsString & aName, const nsString & aStr)
{
  nsIDOMElement      * node;
  nsIDOMHTMLDocument * doc = nsnull;
  mWindow->GetDocument(doc);
  if (nsnull != doc) {
    if (NS_OK == doc->GetElementById(aName,   &node)) {
      SetText(node, aStr);
      NS_RELEASE(node);
    }
    NS_RELEASE(doc);
  }
}

//-----------------------------------------------------------------
float nsBaseDialog::GetFloat(nsString & aStr)
{

  return (float)0.0;
}
