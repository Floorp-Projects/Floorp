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
#include "nsPrintSetupDialog.h"
#include "nsIDOMEvent.h"
#include "nsIXPBaseWindow.h"
#include "nsBrowserWindow.h"
#include "nsIDOMEventTarget.h"

#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLDocument.h"
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

PrintSetupInfo::PrintSetupInfo(const PrintSetupInfo & aPSI) :
  mPortrait(aPSI.mPortrait),
  mBevelLines(aPSI.mBevelLines),
  mBlackText(aPSI.mBlackText),
  mBlackLines(aPSI.mBlackLines),
  mLastPageFirst(aPSI.mLastPageFirst),
  mPrintBackgrounds(aPSI.mPrintBackgrounds),
  mTopMargin(aPSI.mTopMargin),
  mBottomMargin(aPSI.mBottomMargin),
  mLeftMargin(aPSI.mLeftMargin),
  mRightMargin(aPSI.mRightMargin),
  mDocTitle(aPSI.mDocTitle),
  mDocLocation(aPSI.mDocLocation),
  mHeaderText(aPSI.mHeaderText),
  mPageNum(aPSI.mPageNum),
  mPageTotal(aPSI.mPageTotal),
  mDatePrinted(aPSI.mDatePrinted),
  mFooterText(aPSI.mFooterText)
{
}


//-------------------------------------------------------------------------
//
// nsPrintSetupDialog constructor
//
//-------------------------------------------------------------------------
//-----------------------------------------------------------------
nsPrintSetupDialog::nsPrintSetupDialog(nsBrowserWindow * aBrowserWindow) :
  nsBaseDialog(aBrowserWindow),
  mOKBtn(nsnull),
  mPrinterSetupInfo(nsnull)
{
}

//-----------------------------------------------------------------
nsPrintSetupDialog::~nsPrintSetupDialog()
{
}

//---------------------------------------------------------------
void nsPrintSetupDialog::Initialize(nsIXPBaseWindow * aWindow) 
{
  nsBaseDialog::Initialize(aWindow);

  nsIDOMHTMLDocument *doc = nsnull;
  mWindow->GetDocument(doc);
  if (nsnull != doc) {
    doc->GetElementById(NS_LITERAL_STRING("ok"),       &mOKBtn);

    // XXX: Register event listening on each dom element. We should change this so
    // all DOM events are automatically passed through.
    mWindow->AddEventListener(mOKBtn);
    //SetChecked(mMatchCaseCB, PR_FALSE);
    
    NS_RELEASE(doc);

    if (nsnull != mPrinterSetupInfo) {
      SetSetupInfoInternal(*mPrinterSetupInfo);
      delete mPrinterSetupInfo;
      mPrinterSetupInfo = nsnull;
    }
  }
}

//-----------------------------------------------------------------
void nsPrintSetupDialog::GetSetupInfo(PrintSetupInfo & aPSI)
{
  aPSI.mPortrait         = IsChecked(NS_LITERAL_STRING("portrait"));
  aPSI.mBevelLines       = IsChecked(NS_LITERAL_STRING("bevellines"));
  aPSI.mBlackText        = IsChecked(NS_LITERAL_STRING("blacktext"));
  aPSI.mBlackLines       = IsChecked(NS_LITERAL_STRING("blacklines"));
  aPSI.mLastPageFirst    = IsChecked(NS_LITERAL_STRING("lastpagefirst"));
  aPSI.mPrintBackgrounds = IsChecked(NS_LITERAL_STRING("printbg"));

  nsString str;
  GetText(NS_LITERAL_STRING("toptext"), str);
  aPSI.mTopMargin = GetFloat(str);

  GetText(NS_LITERAL_STRING("lefttext"), str);
  aPSI.mLeftMargin = GetFloat(str);

  GetText(NS_LITERAL_STRING("bottomtext"), str);
  aPSI.mBottomMargin = GetFloat(str);

  GetText(NS_LITERAL_STRING("righttext"), str);
  aPSI.mRightMargin = GetFloat(str);

  GetText(NS_LITERAL_STRING("lefttext"),   str);
  GetText(NS_LITERAL_STRING("bottomtext"), str);
  GetText(NS_LITERAL_STRING("righttext"),  str);

  nsString header;
  nsString footer;
  GetText(NS_LITERAL_STRING("headertext"),  header);
  GetText(NS_LITERAL_STRING("footertext"),  footer);
  aPSI.mHeaderText = header;
  aPSI.mFooterText = footer;

  //GetText("headertext",  aPSI.mHeaderText);
  //GetText("footertext",  aPSI.mFooterText);

  aPSI.mDocTitle    = IsChecked(NS_LITERAL_STRING("doctitle"));
  aPSI.mDocLocation = IsChecked(NS_LITERAL_STRING("docloc"));
  aPSI.mPageNum     = IsChecked(NS_LITERAL_STRING("pagenum"));
  aPSI.mPageTotal   = IsChecked(NS_LITERAL_STRING("pagetotal"));
  aPSI.mDatePrinted = IsChecked(NS_LITERAL_STRING("dateprinted"));

}

//-----------------------------------------------------------------
void nsPrintSetupDialog::SetSetupInfo(const PrintSetupInfo & aPSI)
{
  if (nsnull == mWindow) {
    mPrinterSetupInfo = new PrintSetupInfo(aPSI);
  } else {
    SetSetupInfoInternal(aPSI);
  }
}

//-----------------------------------------------------------------
void nsPrintSetupDialog::SetSetupInfoInternal(const PrintSetupInfo & aPSI)
{
  SetChecked(NS_LITERAL_STRING("portrait"),      aPSI.mPortrait);
  SetChecked(NS_LITERAL_STRING("bevellines"),    aPSI.mBevelLines);
  SetChecked(NS_LITERAL_STRING("blacktext"),     aPSI.mBlackText);
  SetChecked(NS_LITERAL_STRING("blacklines"),    aPSI.mBlackLines);
  SetChecked(NS_LITERAL_STRING("lastpagefirst"), aPSI.mLastPageFirst);
  SetChecked(NS_LITERAL_STRING("printbg"),       aPSI.mPrintBackgrounds);

  char str[64];
  sprintf(str, "%5.2f", aPSI.mTopMargin);
  SetText(NS_LITERAL_STRING("toptext"), NS_ConvertASCIItoUCS2(str));
  sprintf(str, "%5.2f", aPSI.mLeftMargin);
  SetText(NS_LITERAL_STRING("lefttext"), NS_ConvertASCIItoUCS2(str));
  sprintf(str, "%5.2f", aPSI.mBottomMargin);
  SetText(NS_LITERAL_STRING("bottomtext"), NS_ConvertASCIItoUCS2(str));
  sprintf(str, "%5.2f", aPSI.mRightMargin);
  SetText(NS_LITERAL_STRING("righttext"),  NS_ConvertASCIItoUCS2(str));

  SetText(NS_LITERAL_STRING("headertext"),  aPSI.mHeaderText);
  SetText(NS_LITERAL_STRING("footertext"),  aPSI.mFooterText);

  SetChecked(NS_LITERAL_STRING("doctitle"),    aPSI.mDocTitle);
  SetChecked(NS_LITERAL_STRING("docloc"),      aPSI.mDocLocation);
  SetChecked(NS_LITERAL_STRING("pagenum"),     aPSI.mPageNum);
  SetChecked(NS_LITERAL_STRING("pagetotal"),   aPSI.mPageTotal);
  SetChecked(NS_LITERAL_STRING("dateprinted"), aPSI.mDatePrinted);

}

//-----------------------------------------------------------------
void nsPrintSetupDialog::MouseClick(nsIDOMEvent* aMouseEvent, nsIXPBaseWindow * aWindow, PRBool &aStatus)
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
        GetSetupInfo(mBrowserWindow->mPrintSetupInfo);
        DoClose();
      } 
    }
  }
}

//-----------------------------------------------------------------
void nsPrintSetupDialog::Destroy(nsIXPBaseWindow * aWindow)
{
  // Unregister event listeners that were registered in the
  // Initialize here. 
  // XXX: Should change code in XPBaseWindow to automatically unregister
  // all event listening, That way this code will not be necessary.
  aWindow->RemoveEventListener(mOKBtn);
}


void
nsPrintSetupDialog::DoClose()
{
  mWindow->SetVisible(PR_FALSE);
}


