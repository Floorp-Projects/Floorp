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

#include "nsFindDialog.h"
#include "nsIDOMEvent.h"
#include "nsIXPBaseWindow.h"

#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLDocument.h"
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

//-------------------------------------------------------------------------
//
// nsFindDialog constructor
//
//-------------------------------------------------------------------------
//-----------------------------------------------------------------
nsFindDialog::nsFindDialog(nsBrowserWindow * aBrowserWindow) :
  nsBaseDialog(aBrowserWindow),
  mFindBtn(nsnull),
  mSearchDown(PR_TRUE)
{
}

//-----------------------------------------------------------------
nsFindDialog::~nsFindDialog()
{
}

//---------------------------------------------------------------
void nsFindDialog::Initialize(nsIXPBaseWindow * aWindow) 
{
  nsBaseDialog::Initialize(aWindow);

  nsIDOMHTMLDocument *doc = nsnull;
  mWindow->GetDocument(doc);
  if (nsnull != doc) {
    doc->GetElementById("find",       &mFindBtn);
    doc->GetElementById("searchup",   &mUpRB);
    doc->GetElementById("searchdown", &mDwnRB);
    doc->GetElementById("matchcase",  &mMatchCaseCB);

    // XXX: Register event listening on each dom element. We should change this so
    // all DOM events are automatically passed through.
    aWindow->AddEventListener(mFindBtn);
    aWindow->AddEventListener(mUpRB);
    aWindow->AddEventListener(mDwnRB);

    SetChecked(mMatchCaseCB, PR_FALSE);
    NS_RELEASE(doc);
  }
}

//-----------------------------------------------------------------
void nsFindDialog::MouseClick(nsIDOMEvent* aMouseEvent, nsIXPBaseWindow * aWindow, PRBool &aStatus)
{
   // Event Dispatch. This method should not contain
   // anything but calls to methods. This idea is that this dispatch
   // mechanism may be replaced by JavaScript EventHandlers which call the idl'ed
   // interfaces to perform the same operation that is currently being handled by
   // this C++ code.

  nsBaseDialog::MouseClick(aMouseEvent, aWindow, aStatus);
  if (!aStatus) { // is not consumed
    nsIDOMNode * node;
    aMouseEvent->GetTarget(&node);
    if (node == mFindBtn) {
      DoFind();
    }
    NS_RELEASE(node);
  }
}

//-----------------------------------------------------------------
void nsFindDialog::Destroy(nsIXPBaseWindow * aWindow)
{
  // Unregister event listeners that were registered in the
  // Initialize here. 
  // XXX: Should change code in XPBaseWindow to automatically unregister
  // all event listening, That way this code will not be necessary.
  aWindow->RemoveEventListener(mFindBtn);
}

//---------------------------------------------------------------
void
nsFindDialog::DoFind()
{
  // Now we have the content tree, lets find the 
  // widgets holding the info.

  nsIDOMElement * textNode = nsnull;
  nsIDOMHTMLDocument *doc = nsnull;
  mWindow->GetDocument(doc);
  if (nsnull != doc) {
    if (NS_OK == doc->GetElementById("query", &textNode)) {
      nsIDOMHTMLInputElement * element;
      if (NS_OK == textNode->QueryInterface(kIDOMHTMLInputElementIID, (void**) &element)) {
        nsString str;
        PRBool foundIt = PR_FALSE;
        element->GetValue(str);
        PRBool searchDown = IsChecked(mDwnRB);
        PRBool matchcase  = IsChecked(mMatchCaseCB);

        mBrowserWindow->FindNext(str, matchcase, searchDown, foundIt);
        if (foundIt) {
          mBrowserWindow->ForceRefresh();
        }

        NS_RELEASE(element);
      }
      NS_RELEASE(textNode);
    }
    NS_RELEASE(doc);
  }
}

//---------------------------------------------------------------
void
nsFindDialog::DoClose()
{
  mWindow->SetVisible(PR_FALSE);
}


