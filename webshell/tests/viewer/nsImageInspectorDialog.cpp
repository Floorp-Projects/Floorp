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
#include "nsImageInspectorDialog.h"
#include "nsIDOMEvent.h"
#include "nsIXPBaseWindow.h"
#include "nsBrowserWindow.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMDocument.h"

#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLDocument.h"
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);


//-------------------------------------------------------------------------
//
// nsImageInspectorDialog constructor
//
//-------------------------------------------------------------------------
//-----------------------------------------------------------------
nsImageInspectorDialog::nsImageInspectorDialog(nsBrowserWindow * aBrowserWindow, nsIDOMDocument * aDoc) :
  nsBaseDialog(aBrowserWindow),
  mOKBtn(nsnull),
  mDOMDoc(aDoc)
{
}

//-----------------------------------------------------------------
nsImageInspectorDialog::~nsImageInspectorDialog()
{
}

//---------------------------------------------------------------
void nsImageInspectorDialog::Initialize(nsIXPBaseWindow * aWindow) 
{
  nsBaseDialog::Initialize(aWindow);

  nsIDOMHTMLDocument *doc = nsnull;
  mWindow->GetDocument(doc);
  if (nsnull != doc) {
    doc->GetElementById(NS_LITERAL_STRING("ok"), &mOKBtn);

    // XXX: Register event listening on each dom element. We should change this so
    // all DOM events are automatically passed through.
    mWindow->AddEventListener(mOKBtn);
    //SetChecked(mMatchCaseCB, PR_FALSE);
    
    NS_RELEASE(doc);
  }
}

#if 0
//-----------------------------------------------------------------
void nsImageInspectorDialog::GetSetupInfo(PrintSetupInfo & aPSI)
{
  aPSI.mPortrait         = IsChecked("portrait");
  aPSI.mBevelLines       = IsChecked("bevellines");
  aPSI.mBlackText        = IsChecked("blacktext");
  aPSI.mBlackLines       = IsChecked("blacklines");
  aPSI.mLastPageFirst    = IsChecked("lastpagefirst");
  aPSI.mPrintBackgrounds = IsChecked("printbg");

  nsString str;
  GetText("toptext", str);
  aPSI.mTopMargin = GetFloat(str);

  GetText("lefttext", str);
  aPSI.mLeftMargin = GetFloat(str);

  GetText("bottomtext", str);
  aPSI.mBottomMargin = GetFloat(str);

  GetText("righttext", str);
  aPSI.mRightMargin = GetFloat(str);

  GetText("lefttext",   str);
  GetText("bottomtext", str);
  GetText("righttext",  str);

  nsString header("");
  nsString footer("");
  GetText("headertext",  header);
  GetText("footertext",  footer);
  aPSI.mHeaderText = header;
  aPSI.mFooterText = footer;

  //GetText("headertext",  aPSI.mHeaderText);
  //GetText("footertext",  aPSI.mFooterText);

  aPSI.mDocTitle    = IsChecked("doctitle");
  aPSI.mDocLocation = IsChecked("docloc");
  aPSI.mPageNum     = IsChecked("pagenum");
  aPSI.mPageTotal   = IsChecked("pagetotal");
  aPSI.mDatePrinted = IsChecked("dateprinted");

}

//-----------------------------------------------------------------
void nsImageInspectorDialog::SetSetupInfo(const PrintSetupInfo & aPSI)
{
  if (nsnull == mWindow) {
    mPrinterSetupInfo = new PrintSetupInfo(aPSI);
  } else {
    SetSetupInfoInternal(aPSI);
  }
}

//-----------------------------------------------------------------
void nsImageInspectorDialog::SetSetupInfoInternal(const PrintSetupInfo & aPSI)
{
  SetChecked("portrait",      aPSI.mPortrait);
  SetChecked("bevellines",    aPSI.mBevelLines);
  SetChecked("blacktext",     aPSI.mBlackText);
  SetChecked("blacklines",    aPSI.mBlackLines);
  SetChecked("lastpagefirst", aPSI.mLastPageFirst);
  SetChecked("printbg",       aPSI.mPrintBackgrounds);

  char str[64];
  sprintf(str, "%5.2f", aPSI.mTopMargin);
  SetText("toptext", str);
  sprintf(str, "%5.2f", aPSI.mLeftMargin);
  SetText("lefttext", str);
  sprintf(str, "%5.2f", aPSI.mBottomMargin);
  SetText("bottomtext", str);
  sprintf(str, "%5.2f", aPSI.mRightMargin);
  SetText("righttext",  str);

  SetText("headertext",  aPSI.mHeaderText);
  SetText("footertext",  aPSI.mFooterText);

  SetChecked("doctitle",    aPSI.mDocTitle);
  SetChecked("docloc",      aPSI.mDocLocation);
  SetChecked("pagenum",     aPSI.mPageNum);
  SetChecked("pagetotal",   aPSI.mPageTotal);
  SetChecked("dateprinted", aPSI.mDatePrinted);

}
#endif

//-----------------------------------------------------------------
void nsImageInspectorDialog::MouseClick(nsIDOMEvent* aMouseEvent, nsIXPBaseWindow * aWindow, PRBool &aStatus)
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
      if (node.get() == mOKBtn) {
        nsString str;
        GetText(NS_LITERAL_STRING("url"), str);
        //DoClose();
      } 
    }
  }
}

//-----------------------------------------------------------------
void nsImageInspectorDialog::Destroy(nsIXPBaseWindow * aWindow)
{
  // Unregister event listeners that were registered in the
  // Initialize here. 
  // XXX: Should change code in XPBaseWindow to automatically unregister
  // all event listening, That way this code will not be necessary.
  aWindow->RemoveEventListener(mOKBtn);
}


void
nsImageInspectorDialog::DoClose()
{
  mWindow->SetVisible(PR_FALSE);
}


