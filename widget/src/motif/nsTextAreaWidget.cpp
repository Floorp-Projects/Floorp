/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsTextAreaWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsXtEventHandler.h"

#include <Xm/Text.h>

#define DBG 0

//-------------------------------------------------------------------------
//
// nsTextAreaWidget constructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::nsTextAreaWidget(nsISupports *aOuter) : nsWindow(aOuter),
  mMakeReadOnly(PR_FALSE)
{
  //mBackground = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsTextAreaWidget destructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::~nsTextAreaWidget()
{
}

//-------------------------------------------------------------------------
void nsTextAreaWidget::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData) 
{
  Widget parentWidget = nsnull;

  if (DBG) fprintf(stderr, "aParent 0x%x\n", aParent);

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (Widget) aInitData ;
  }

  if (DBG) fprintf(stderr, "Parent 0x%x\n", parentWidget);

  mWidget = ::XtVaCreateManagedWidget("button",
                                    xmTextWidgetClass, 
                                    parentWidget,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNrecomputeSize, False,
                                    XmNhighlightOnEnter, False,
                                    XmNeditMode, XmMULTI_LINE_EDIT,
		                    XmNx, aRect.x,
		                    XmNy, aRect.y, 
                                    nsnull);
  mHelper = new nsTextHelper(mWidget);
  if (DBG) fprintf(stderr, "Button 0x%x  this 0x%x\n", mWidget, this);

  // save the event callback function
  mEventCallback = aHandleEventFunction;

  InitCallbacks("nsTextAreaWidget");

  if (mMakeReadOnly) {
    SetReadOnly(PR_TRUE);
  }

}

//-------------------------------------------------------------------------
void nsTextAreaWidget::Create(nsNativeWindow aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
}


//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsTextAreaWidget::QueryObject(REFNSIID aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
  static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);

  if (aIID.Equals(kITextWidgetIID)) {
    AddRef();
    *aInstancePtr = (void**) &mAggWidget;
    return NS_OK;
  }
  if (aIID.Equals(kITextAreaWidgetIID)) {
    AddRef();
    *aInstancePtr = (void**) &mAggWidget;
    return NS_OK;
  }
  return nsWindow::QueryObject(aIID, aInstancePtr);
}


//-------------------------------------------------------------------------
//
// paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsTextAreaWidget::OnPaint(nsPaintEvent & aEvent)
{
  return PR_FALSE;
}


//--------------------------------------------------------------
PRBool nsTextAreaWidget::OnResize(nsSizeEvent &aEvent)
{
  return PR_FALSE;
}

//--------------------------------------------------------------
void nsTextAreaWidget::SetPassword(PRBool aIsPassword)
{
}

//--------------------------------------------------------------
PRBool  nsTextAreaWidget::SetReadOnly(PRBool aReadOnlyFlag)
{
  if (mWidget == nsnull && aReadOnlyFlag) {
    mMakeReadOnly = PR_TRUE;
    return PR_TRUE;
  }
  return mHelper->SetReadOnly(aReadOnlyFlag);
}

//--------------------------------------------------------------
void nsTextAreaWidget::SetMaxTextLength(PRUint32 aChars)
{
  mHelper->SetMaxTextLength(aChars);
}

//--------------------------------------------------------------
PRUint32  nsTextAreaWidget::GetText(nsString& aTextBuffer, PRUint32 aBufferSize) {
  return mHelper->GetText(aTextBuffer, aBufferSize);
}

//--------------------------------------------------------------
PRUint32  nsTextAreaWidget::SetText(const nsString& aText)
{ 
  return mHelper->SetText(aText);
}

//--------------------------------------------------------------
PRUint32  nsTextAreaWidget::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos)
{ 
  return mHelper->InsertText(aText, aStartPos, aEndPos);
}

//--------------------------------------------------------------
void  nsTextAreaWidget::RemoveText()
{
  mHelper->RemoveText();
}

//--------------------------------------------------------------
void nsTextAreaWidget::SelectAll()
{
  mHelper->SelectAll();
}


//--------------------------------------------------------------
void  nsTextAreaWidget::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  mHelper->SetSelection(aStartSel, aEndSel);
}


//--------------------------------------------------------------
void  nsTextAreaWidget::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
  mHelper->GetSelection(aStartSel, aEndSel);
}

//--------------------------------------------------------------
void  nsTextAreaWidget::SetCaretPosition(PRUint32 aPosition)
{
  mHelper->SetCaretPosition(aPosition);
}

//--------------------------------------------------------------
PRUint32  nsTextAreaWidget::GetCaretPosition()
{
  return mHelper->GetCaretPosition();
}

//--------------------------------------------------------------
PRBool nsTextAreaWidget::AutoErase()
{
  return mHelper->AutoErase();
}



//--------------------------------------------------------------
#define GET_OUTER() ((nsTextAreaWidget*) ((char*)this - nsTextAreaWidget::GetOuterOffset()))


//--------------------------------------------------------------
void nsTextAreaWidget::AggTextAreaWidget::SetMaxTextLength(PRUint32 aChars)
{
  GET_OUTER()->SetMaxTextLength(aChars);
}

//--------------------------------------------------------------
PRUint32  nsTextAreaWidget::AggTextAreaWidget::GetText(nsString& aTextBuffer, PRUint32 aBufferSize) {
  return GET_OUTER()->GetText(aTextBuffer, aBufferSize);
}

//--------------------------------------------------------------
PRUint32  nsTextAreaWidget::AggTextAreaWidget::SetText(const nsString& aText)
{ 
  return GET_OUTER()->SetText(aText);
}

//--------------------------------------------------------------
PRUint32  nsTextAreaWidget::AggTextAreaWidget::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos)
{ 
  return GET_OUTER()->InsertText(aText, aStartPos, aEndPos);
}

//--------------------------------------------------------------
void  nsTextAreaWidget::AggTextAreaWidget::RemoveText()
{
  GET_OUTER()->RemoveText();
}

//--------------------------------------------------------------
void  nsTextAreaWidget::AggTextAreaWidget::SetPassword(PRBool aIsPassword)
{
  GET_OUTER()->SetPassword(aIsPassword);
}

//--------------------------------------------------------------
PRBool  nsTextAreaWidget::AggTextAreaWidget::SetReadOnly(PRBool aReadOnlyFlag)
{
  GET_OUTER()->SetReadOnly(aReadOnlyFlag);
}

//--------------------------------------------------------------
void nsTextAreaWidget::AggTextAreaWidget::SelectAll()
{
  GET_OUTER()->SelectAll();
}


//--------------------------------------------------------------
void  nsTextAreaWidget::AggTextAreaWidget::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  GET_OUTER()->SetSelection(aStartSel, aEndSel);
}


//--------------------------------------------------------------
void  nsTextAreaWidget::AggTextAreaWidget::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
  GET_OUTER()->GetSelection(aStartSel, aEndSel);
}

//--------------------------------------------------------------
void  nsTextAreaWidget::AggTextAreaWidget::SetCaretPosition(PRUint32 aPosition)
{
  GET_OUTER()->SetCaretPosition(aPosition);
}

//--------------------------------------------------------------
PRUint32  nsTextAreaWidget::AggTextAreaWidget::GetCaretPosition()
{
  return GET_OUTER()->GetCaretPosition();
}

PRBool nsTextAreaWidget::AggTextAreaWidget::AutoErase()
{
  return GET_OUTER()->AutoErase();
}


//----------------------------------------------------------------------

BASE_IWIDGET_IMPL(nsTextAreaWidget, AggTextAreaWidget);

