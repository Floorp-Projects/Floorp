/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsFindDialog.h"
#include "nsIDOMEvent.h"
#include "nsIXPBaseWindow.h"
#include "nsIDOMEventTarget.h"

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
    doc->GetElementById(NS_ConvertASCIItoUCS2("find"),       &mFindBtn);
    doc->GetElementById(NS_ConvertASCIItoUCS2("searchup"),   &mUpRB);
    doc->GetElementById(NS_ConvertASCIItoUCS2("searchdown"), &mDwnRB);
    doc->GetElementById(NS_ConvertASCIItoUCS2("matchcase"),  &mMatchCaseCB);

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
    nsCOMPtr<nsIDOMEventTarget> target;
    aMouseEvent->GetTarget(getter_AddRefs(target));
    if (target) {
      nsCOMPtr<nsIDOMElement> node(do_QueryInterface(target));
      if (node.get() == mFindBtn) {
        DoFind();
      }
    }
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
    if (NS_OK == doc->GetElementById(NS_ConvertASCIItoUCS2("query"), &textNode)) {
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


