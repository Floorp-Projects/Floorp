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

#include "nsBaseDialog.h"
#include "nsIDOMEvent.h"
#include "nsIXPBaseWindow.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventTarget.h"

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
    doc->GetElementById(NS_ConvertASCIItoUCS2("cancel"), &mCancelBtn);
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

  nsCOMPtr<nsIDOMEventTarget> target;
  aMouseEvent->GetTarget(getter_AddRefs(target));
  if (target) {
    nsCOMPtr<nsIDOMElement> node(do_QueryInterface(target));
    if (node.get() == mCancelBtn) {
      DoClose();
      aStatus = PR_TRUE;
    }
  }
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
