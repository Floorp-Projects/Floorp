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

#include "nsPrintSetupDialog.h"
#include "nsIDOMEvent.h"
#include "nsIXPBaseWindow.h"
#include "nsBrowserWindow.h"

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
    doc->GetElementById("ok",       &mOKBtn);

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
    nsIDOMNode * node;
    aMouseEvent->GetTarget(&node);
    if (node == mOKBtn) {
      GetSetupInfo(mBrowserWindow->mPrintSetupInfo);
      DoClose();
    } 
    NS_RELEASE(node);
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


